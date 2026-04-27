#include "drv_payout.h"

#include "main.h"

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "driver/gpio.h"
#include "driver/uart.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "DRV_PAYOUT";

#if ENABLE_PAYOUT_DEVICE

static bool s_initialized = false;
static int16_t s_last_target_pwm = 0;

/**
 * Modbus RTU CRC16 计算（多项式 0xA001）。
 * 移植自 fangxianqi 参考工程 main.c:Modbus_CRC16。
 */
static uint16_t payout_modbus_crc16(const uint8_t *data, uint16_t len)
{
    uint16_t crc = 0xFFFF;
    while (len--) {
        crc ^= (uint16_t)(*data++);
        for (int i = 0; i < 8; i++) {
            if (crc & 0x0001) {
                crc >>= 1;
                crc ^= 0xA001;
            } else {
                crc >>= 1;
            }
        }
    }
    return crc;
}

/**
 * 将输入夹紧到 [min,max]，并在接近中位时吸附到 mid（死区）。
 */
static int16_t clamp_with_deadband(int16_t value, int16_t min_v, int16_t max_v,
                                   int16_t mid, int16_t precision)
{
    if (value > max_v) {
        return max_v;
    }
    if (value < min_v) {
        return min_v;
    }
    if (abs((int)(value - mid)) < precision) {
        return mid;
    }
    return value;
}

/**
 * SBUS 通道值 → 目标 PWM 换算，与 fangxianqi 参考工程一致：
 *   PWM = (channel - 1500) / 9 * 20  →  -1000 ~ +1000
 */
static int16_t channel_to_target_pwm(int16_t channel_pwm)
{
    int32_t tmp = (int32_t)channel_pwm - 1500;
    tmp = tmp / 9;
    tmp = tmp * PAYOUT_PWM_SCALE;
    if (tmp > PAYOUT_PWM_LIMIT) tmp = PAYOUT_PWM_LIMIT;
    if (tmp < -PAYOUT_PWM_LIMIT) tmp = -PAYOUT_PWM_LIMIT;
    return (int16_t)tmp;
}

#if PAYOUT_RS485_DE_PIN != GPIO_NUM_NC
static inline void rs485_set_tx_mode(bool tx_enable)
{
    gpio_set_level(PAYOUT_RS485_DE_PIN, tx_enable ? 1 : 0);
}
#else
static inline void rs485_set_tx_mode(bool tx_enable) { (void)tx_enable; }
#endif

esp_err_t drv_payout_init(void)
{
    if (s_initialized) {
        return ESP_OK;
    }

    uart_config_t uart_config = {
        .baud_rate = PAYOUT_BAUD_RATE,
        .data_bits = UART_DATA_8_BITS,
        .parity    = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_DEFAULT,
    };

    esp_err_t ret = uart_driver_install(PAYOUT_UART, 256, 256, 0, NULL, 0);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ uart_driver_install failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_param_config(PAYOUT_UART, &uart_config);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ uart_param_config failed: %s", esp_err_to_name(ret));
        return ret;
    }

    ret = uart_set_pin(PAYOUT_UART,
                       PAYOUT_UART_TX_PIN,
                       PAYOUT_UART_RX_PIN,
                       UART_PIN_NO_CHANGE,
                       UART_PIN_NO_CHANGE);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "❌ uart_set_pin failed: %s", esp_err_to_name(ret));
        return ret;
    }

#if PAYOUT_RS485_DE_PIN != GPIO_NUM_NC
    gpio_config_t de_cfg = {
        .pin_bit_mask = 1ULL << PAYOUT_RS485_DE_PIN,
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&de_cfg);
    rs485_set_tx_mode(false);
#endif

    s_initialized = true;
    s_last_target_pwm = 0;
    ESP_LOGI(TAG, "✅ Payout RS485 init OK (UART%d TX=GPIO%d RX=%d %d bps, 8E1, slave=0x%02X reg=0x%04X)",
             PAYOUT_UART, PAYOUT_UART_TX_PIN, PAYOUT_UART_RX_PIN, PAYOUT_BAUD_RATE,
             PAYOUT_MODBUS_SLAVE, PAYOUT_MODBUS_REG);
    return ESP_OK;
}

/**
 * 打包并下发一次 Modbus RTU 写单寄存器（功能码 0x06）。
 * 帧结构：[slave][0x06][reg_h][reg_l][val_h][val_l][crc_l][crc_h]
 */
static void send_modbus_write_single(int16_t target_pwm)
{
    uint8_t frame[8];
    frame[0] = PAYOUT_MODBUS_SLAVE;
    frame[1] = 0x06;
    frame[2] = (PAYOUT_MODBUS_REG >> 8) & 0xFF;
    frame[3] = PAYOUT_MODBUS_REG & 0xFF;
    frame[4] = (uint8_t)((target_pwm >> 8) & 0xFF);
    frame[5] = (uint8_t)(target_pwm & 0xFF);

    uint16_t crc = payout_modbus_crc16(frame, 6);
    frame[6] = (uint8_t)(crc & 0xFF);
    frame[7] = (uint8_t)((crc >> 8) & 0xFF);

    rs485_set_tx_mode(true);
    uart_write_bytes(PAYOUT_UART, (const char *)frame, sizeof(frame));
    uart_wait_tx_done(PAYOUT_UART, pdMS_TO_TICKS(20));
    rs485_set_tx_mode(false);

    ESP_LOGD(TAG, "UART%d Modbus TX pwm=%d frame=%02X %02X %02X %02X %02X %02X %02X %02X",
             PAYOUT_UART,
             target_pwm, frame[0], frame[1], frame[2], frame[3],
             frame[4], frame[5], frame[6], frame[7]);
}

void drv_payout_send_channel_pwm(uint16_t channel_value)
{
    if (!s_initialized) {
        return;
    }

    int16_t clamped = clamp_with_deadband((int16_t)channel_value,
                                          PAYOUT_CHANNEL_MIN,
                                          PAYOUT_CHANNEL_MAX,
                                          PAYOUT_CHANNEL_MID,
                                          PAYOUT_CHANNEL_DEADBAND);
    int16_t target_pwm = channel_to_target_pwm(clamped);

    if (target_pwm != s_last_target_pwm) {
        ESP_LOGI(TAG, "🪢 Payout pwm: %d → %d (ch=%u)",
                 s_last_target_pwm, target_pwm, (unsigned)channel_value);
    }
    s_last_target_pwm = target_pwm;

    send_modbus_write_single(target_pwm);
}

void drv_payout_stop(void)
{
    if (!s_initialized) {
        return;
    }
    if (s_last_target_pwm != 0) {
        ESP_LOGI(TAG, "🛑 Payout STOP (last pwm=%d)", s_last_target_pwm);
    }
    s_last_target_pwm = 0;
    send_modbus_write_single(0);
}

int16_t drv_payout_get_last_pwm(void)
{
    return s_last_target_pwm;
}

#else /* ENABLE_PAYOUT_DEVICE == 0 */

esp_err_t drv_payout_init(void) { return ESP_OK; }
void drv_payout_send_channel_pwm(uint16_t channel_value) { (void)channel_value; }
void drv_payout_stop(void) {}
int16_t drv_payout_get_last_pwm(void) { return 0; }

#endif /* ENABLE_PAYOUT_DEVICE */
