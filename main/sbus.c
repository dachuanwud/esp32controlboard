#include "sbus.h"
#include "main.h"
#include "hal/uart_types.h"  // 包含UART_INVERT_RXD定义

static const char *TAG = "SBUS";

// SBUS接收缓冲区
static uint8_t g_sbus_rx_buf[LEN_SBUS] = {0};
static uint8_t g_sbus_pt = 0;
static bool g_sbus_frame_ready = false;

// UART事件队列
static QueueHandle_t sbus_uart_queue;

/**
 * SBUS UART接收任务
 */
static void sbus_uart_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t data;

    while (1) {
        if (xQueueReceive(sbus_uart_queue, (void *)&event, portMAX_DELAY)) {
            if (event.type == UART_DATA) {
                // 读取UART数据
                uart_read_bytes(UART_SBUS, &data, 1, portMAX_DELAY);

                if ((g_sbus_pt & 0x80) == 0) { // 数据未解析
                    if (g_sbus_pt > (LEN_SBUS - 1)) {
                        // 缓冲区满，重新开始
                        g_sbus_pt = 0;
                    }

                    // 存入缓冲区
                    g_sbus_rx_buf[g_sbus_pt] = data;
                    g_sbus_pt++;

                    // 判断帧头
                    if (g_sbus_pt == 1) {
                        if (data != 0x0f) {
                            g_sbus_pt--; // 回退，重新等待
                        }
                    } else if (g_sbus_pt == 25) {
                        // 判断帧尾
                        if (data == 0x00) {
                            g_sbus_pt |= 0x80; // 标记一帧数据的解析
                            g_sbus_frame_ready = true;
                        } else {
                            g_sbus_pt = 0; // 数据错误，重新等待
                        }
                    }
                }
            }
        }
    }
}

/**
 * 初始化SBUS接收
 */
esp_err_t sbus_init(void)
{
    // 配置SBUS UART参数
    // 注意：我们将使用ESP32的UART_INVERT_RXD功能直接接收SBUS信号
    // 这样可以简化硬件设计，不需要外部反相器电路
    uart_config_t uart_config = {
        .baud_rate = 100000,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,
        .stop_bits = UART_STOP_BITS_2,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    // 安装UART驱动
    ESP_ERROR_CHECK(uart_driver_install(UART_SBUS, 256, 0, 20, &sbus_uart_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_SBUS, &uart_config));

    // 根据电路图设置SBUS接收引脚
    // 注意：由于GPIO16现在用于CAN TX，我们需要使用其他引脚接收SBUS信号
    // 这里使用GPIO22作为SBUS接收引脚，请根据实际硬件连接调整
    ESP_ERROR_CHECK(uart_set_pin(UART_SBUS, UART_PIN_NO_CHANGE, GPIO_NUM_22, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // 启用RX信号反相，用于直接接收SBUS信号
    // 这样可以直接连接SBUS接收机，不需要外部反相器电路
    ESP_ERROR_CHECK(uart_set_line_inverse(UART_SBUS, UART_SIGNAL_RXD_INV));

    // 创建UART接收任务
    xTaskCreate(sbus_uart_task, "sbus_uart_task", 2048, NULL, 12, NULL);

    ESP_LOGI(TAG, "SBUS initialized with UART_INVERT_RXD for direct signal reception");
    return ESP_OK;
}

/**
 * 解析SBUS数据，映射到12个通道上
 */
uint8_t parse_sbus_msg(uint8_t* sbus_data, uint16_t* channel)
{
    channel[0] = (sbus_data[1] >> 0 | sbus_data[2] << 8) & 0x07FF;
    channel[1] = (sbus_data[2] >> 3 | sbus_data[3] << 5) & 0x07FF;
    channel[2] = (sbus_data[3] >> 6 | sbus_data[4] << 2 | sbus_data[5] << 10) & 0x07FF;
    channel[3] = (sbus_data[5] >> 1 | sbus_data[6] << 7) & 0x07FF;
    channel[4] = (sbus_data[6] >> 4 | sbus_data[7] << 4) & 0x07FF;
    channel[5] = (sbus_data[7] >> 7 | sbus_data[8] << 1 | sbus_data[9] << 9) & 0x07FF;
    channel[6] = (sbus_data[9] >> 2 | sbus_data[10] << 6) & 0x07FF;
    channel[7] = (sbus_data[10] >> 5 | sbus_data[11] << 3) & 0x07FF;
    channel[8] = (sbus_data[12] >> 0 | sbus_data[13] << 8) & 0x07FF;
    channel[9] = (sbus_data[13] >> 3 | sbus_data[14] << 5) & 0x07FF;
    channel[10] = (sbus_data[14] >> 6 | sbus_data[15] << 2 | sbus_data[16] << 10) & 0x07FF;
    channel[11] = (sbus_data[16] >> 1 | sbus_data[17] << 7) & 0x07FF;

    // PPM协议的遥控器需要映射到SBUS范围：282~1722映射到1050~1950。
    for (int i = 0; i < LEN_CHANEL; i++) {
        channel[i] = (channel[i] - 282) * 5 / 8 + 1050;
    }

    return 0;
}

/**
 * 获取最新的SBUS数据
 */
bool sbus_get_data(uint8_t* sbus_data)
{
    if (g_sbus_frame_ready) {
        memcpy(sbus_data, g_sbus_rx_buf, LEN_SBUS);
        g_sbus_pt = 0;
        g_sbus_frame_ready = false;
        return true;
    }
    return false;
}
