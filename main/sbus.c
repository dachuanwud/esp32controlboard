#include <inttypes.h>
#include "sbus.h"
#include "main.h"
#include "hal/uart_types.h"  // 包含UART_INVERT_RXD定义

static const char *TAG = "SBUS";

// SBUS接收缓冲区
static uint8_t g_sbus_rx_buf[LEN_SBUS] = {0};
static uint8_t g_sbus_pt = 0;

// UART事件队列
static QueueHandle_t sbus_uart_queue;

/**
 * SBUS UART接收任务
 */
static void sbus_uart_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t data;

    static uint32_t byte_count = 0;

    ESP_LOGI(TAG, "🚀 SBUS UART task started, waiting for data on GPIO22...");
    ESP_LOGI(TAG, "📡 UART2 Config: 100000bps, 8E2, RX_INVERT enabled");
    ESP_LOGI(TAG, "🔌 Hardware: Connect SBUS signal to GPIO22, GND to GND");

    // 初始化绿灯状态为熄灭（共阳极LED，高电平熄灭）
    gpio_set_level(LED1_GREEN_PIN, 1);
    gpio_set_level(LED2_GREEN_PIN, 1);
    ESP_LOGI(TAG, "💚 Green LEDs initialized (OFF) - will blink when data received");

    while (1) {
        // 每次循环都打印一次状态（用于调试）
        static uint32_t loop_count = 0;
        static uint32_t last_event_time = 0;
        loop_count++;

        // 移除冗余的调试输出，只保留必要的错误检查
        if (loop_count % 10000 == 0) {  // 大幅减少调试频率
            // 检查UART缓冲区是否溢出
            size_t uart_buf_len = 0;
            uart_get_buffered_data_len(UART_SBUS, &uart_buf_len);
            if (uart_buf_len > 500) {
                uart_flush(UART_SBUS);
                ESP_LOGW(TAG, "⚠️ UART buffer overflow, flushed %" PRIu32 " bytes", (uint32_t)uart_buf_len);
                g_sbus_pt = 0; // 重置SBUS解析状态
            }
        }

        // 移除GPIO22直接读取，避免与UART2功能冲突
        // GPIO22现在专门用于UART2接收SBUS数据

        if (xQueueReceive(sbus_uart_queue, (void *)&event, pdMS_TO_TICKS(10))) {
            last_event_time = xTaskGetTickCount();
            ESP_LOGD(TAG, "📨 UART event received at tick: %" PRIu32, last_event_time);
            if (event.type == UART_DATA) {
                // 读取所有可用的UART数据，而不是一次只读一个字节
                uint8_t temp_buffer[64];
                int len = uart_read_bytes(UART_SBUS, temp_buffer, sizeof(temp_buffer), pdMS_TO_TICKS(10));
                if (len > 0) {
                    // 处理接收到的每个字节
                    for (int i = 0; i < len; i++) {
                        data = temp_buffer[i];
                        byte_count++;

                    // 移除冗余的LED闪烁和调试输出，专注于数据处理

                    // 使用STM32相同的逻辑：检查最高位标志
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
                                g_sbus_pt |= 0x80; // 标记一帧数据的接收
                                // LED指示
                                gpio_set_level(LED1_GREEN_PIN, 0);
                                gpio_set_level(LED2_GREEN_PIN, 0);
                            } else {
                                g_sbus_pt = 0; // 数据错误，重新等待
                            }
                        }
                    }
                    } // 关闭 for 循环
                }
            } else {
                ESP_LOGD(TAG, "UART event type: %d", event.type);
            }
        } else {
            // 超时，没有接收到数据
            static uint32_t no_data_count = 0;
            no_data_count++;

            // 简化超时处理，减少日志输出
            if (no_data_count > 500) {  // 约5秒无数据时提示一次
                ESP_LOGW(TAG, "⚠️ No SBUS data for 5 seconds - check connections");
                no_data_count = 0;
            }

            // 让出CPU时间，避免看门狗超时
            vTaskDelay(pdMS_TO_TICKS(1));
        }
    }
}

/**
 * 初始化SBUS接收
 */
esp_err_t sbus_init(void)
{
    // 配置UART参数用于SBUS协议接收
    // SBUS协议配置：100000 bps, 8E2 (8数据位 + 偶校验 + 2停止位)
    // SBUS使用反相逻辑，需要启用UART_SIGNAL_RXD_INV
    uart_config_t uart_config = {
        .baud_rate = 100000,            // SBUS标准波特率
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_EVEN,     // SBUS使用偶校验
        .stop_bits = UART_STOP_BITS_2,  // SBUS使用2停止位
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    ESP_LOGI(TAG, "🔧 Configuring UART2 for SBUS protocol:");
    ESP_LOGI(TAG, "   📡 Baud rate: %d bps", uart_config.baud_rate);
    ESP_LOGI(TAG, "   📊 Data bits: %d", uart_config.data_bits);
    ESP_LOGI(TAG, "   ✅ Parity: %s", uart_config.parity == UART_PARITY_EVEN ? "Even" : "None");
    ESP_LOGI(TAG, "   🛑 Stop bits: %d", uart_config.stop_bits == UART_STOP_BITS_2 ? 2 : 1);

    // 安装UART驱动 - 增加缓冲区大小以防止数据丢失
    ESP_ERROR_CHECK(uart_driver_install(UART_SBUS, 1024, 0, 50, &sbus_uart_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_SBUS, &uart_config));

    // 设置GPIO22作为UART2接收引脚
    // 用于接收SBUS信号（来自遥控接收机）
    ESP_ERROR_CHECK(uart_set_pin(UART_SBUS, UART_PIN_NO_CHANGE, GPIO_NUM_22, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // SBUS使用反相逻辑，硬件无反相器时必须启用软件反相
    ESP_ERROR_CHECK(uart_set_line_inverse(UART_SBUS, UART_SIGNAL_RXD_INV));
    ESP_LOGI(TAG, "🔄 Signal inversion: ENABLED (no hardware inverter)");

    // 创建UART接收任务 (增加栈大小以支持调试输出)
    xTaskCreate(sbus_uart_task, "sbus_uart_task", 4096, NULL, 12, NULL);

    ESP_LOGI(TAG, "✅ UART2 initialized successfully:");
    ESP_LOGI(TAG, "   📍 RX Pin: GPIO22");
    ESP_LOGI(TAG, "   📡 Config: 100000bps, 8E2");
    ESP_LOGI(TAG, "   🔄 Signal inversion: ENABLED");
    ESP_LOGI(TAG, "   🚀 Ready to receive SBUS data!");
    return ESP_OK;
}

/**
 * 解析SBUS数据，按照正确的SBUS协议解析16个通道
 * SBUS协议：25字节 = [0xF0] + [data1-22] + [flags] + [0x00]
 * 每个通道11位，范围0-2047
 */
uint8_t parse_sbus_msg(uint8_t* sbus_data, uint16_t* channel)
{
    // 按照SBUS协议正确解析16个通道（每个通道11位）
    // data1-22包含16个通道的数据

    // 使用正确的SBUS解析方式
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

    channel[12] = (sbus_data[17] >> 4 | sbus_data[18] << 4) & 0x07FF;
    channel[13] = (sbus_data[18] >> 7 | sbus_data[19] << 1 | sbus_data[20] << 9) & 0x07FF;
    channel[14] = (sbus_data[20] >> 2 | sbus_data[21] << 6) & 0x07FF;
    channel[15] = (sbus_data[21] >> 5 | sbus_data[22] << 3) & 0x07FF;

    // SBUS原始值映射到标准PWM范围 (282~1722 → 1050~1950)
    for (int i = 0; i < LEN_CHANEL; i++) {
        channel[i] = (channel[i] - 282) * 5 / 8 + 1050;
    }

    // 打印SBUS通道值 - 对应手柄操作
    ESP_LOGI(TAG, "SBUS CH0:%4u CH1:%4u CH2:%4u CH3:%4u CH6:%4u CH7:%4u",
             channel[0], channel[1], channel[2], channel[3], channel[6], channel[7]);

    return 0;
}

/**
 * 获取最新的SBUS数据
 */
bool sbus_get_data(uint8_t* sbus_data)
{
    // 使用STM32相同的逻辑：检查最高位标志
    if ((g_sbus_pt & 0x80) != 0) {
        memcpy(sbus_data, g_sbus_rx_buf, LEN_SBUS);
        g_sbus_pt = 0; // 清0，等待下一帧数据的接收
        return true;
    }
    return false;
}
