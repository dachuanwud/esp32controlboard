#include <inttypes.h>
#include "sbus.h"
#include "main.h"
#include "hal/uart_types.h"  // 包含UART_INVERT_RXD定义
#include "freertos/semphr.h"
#include "esp_task_wdt.h"    // 🐕 任务看门狗

static const char *TAG = "SBUS";

// SBUS接收缓冲区
static uint8_t g_sbus_rx_buf[LEN_SBUS] = {0};
static uint8_t g_sbus_pt = 0;

// UART事件队列
static QueueHandle_t sbus_uart_queue;

// SBUS数据就绪信号量（用于通知处理任务）
static SemaphoreHandle_t sbus_data_ready_sem = NULL;

/**
 * SBUS UART接收任务
 * 🐕 已添加任务看门狗监控
 */
static void sbus_uart_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t data;

    static uint32_t byte_count = 0;
    // 记录最后一次成功接收完整SBUS帧的时间（用于超时检测）
    static TickType_t last_frame_time = 0;
    static bool first_frame_received = false;
    // 错误统计（用于诊断）
    static uint32_t header_error_count = 0;
    static uint32_t footer_error_count = 0;
    // 🐕 喂狗计数器
    static uint32_t wdt_feed_counter = 0;

    ESP_LOGI(TAG, "🚀 SBUS UART task started, waiting for data on GPIO22...");
    ESP_LOGI(TAG, "📡 UART2 Config: 100000bps, 8E2, RX_INVERT enabled");
    ESP_LOGI(TAG, "🔌 Hardware: Connect SBUS signal to GPIO22, GND to GND");

    // 🐕 订阅任务看门狗监控
    esp_err_t wdt_ret = esp_task_wdt_add(NULL);
    if (wdt_ret == ESP_OK) {
        ESP_LOGI(TAG, "🐕 SBUS UART任务已加入看门狗监控");
    } else {
        ESP_LOGW(TAG, "⚠️ SBUS UART任务加入看门狗失败: %s", esp_err_to_name(wdt_ret));
    }

    // 初始化绿灯状态为熄灭（共阳极LED，高电平熄灭）
    gpio_set_level(LED1_GREEN_PIN, 1);
    gpio_set_level(LED2_GREEN_PIN, 1);
    ESP_LOGI(TAG, "💚 Green LEDs initialized (OFF) - will blink when data received");

    while (1) {
        // 🐕 定期喂狗 - 每500次循环喂狗一次（约5秒）
        wdt_feed_counter++;
        if (wdt_feed_counter >= 500) {
            esp_task_wdt_reset();
            wdt_feed_counter = 0;
        }

        // 每次循环都打印一次状态（用于调试）
        static uint32_t loop_count = 0;
        static uint32_t last_event_time = 0;
        loop_count++;

        // 🔧 优化：更频繁地检查UART缓冲区状态，防止溢出
        // 从每10000次循环改为每100次循环检查一次
        if (loop_count % 100 == 0) {
            size_t uart_buf_len = 0;
            uart_get_buffered_data_len(UART_SBUS, &uart_buf_len);
            // 🔧 降低溢出阈值，提前警告（从500字节降到300字节）
            if (uart_buf_len > 300) {
                ESP_LOGW(TAG, "⚠️ UART buffer filling up: %" PRIu32 " bytes (threshold: 300)", (uint32_t)uart_buf_len);
                // 如果超过800字节，强制刷新（防止完全溢出）
                if (uart_buf_len > 800) {
                    uart_flush(UART_SBUS);
                    ESP_LOGW(TAG, "⚠️ UART buffer overflow, flushed %" PRIu32 " bytes", (uint32_t)uart_buf_len);
                    g_sbus_pt = 0; // 重置SBUS解析状态
                }
            }
        }

        // 移除GPIO22直接读取，避免与UART2功能冲突
        // GPIO22现在专门用于UART2接收SBUS数据

        if (xQueueReceive(sbus_uart_queue, (void *)&event, pdMS_TO_TICKS(10))) {
            last_event_time = xTaskGetTickCount();
            ESP_LOGD(TAG, "📨 UART event received at tick: %" PRIu32, last_event_time);
            if (event.type == UART_DATA) {
                // 🔧 优化：循环读取直到缓冲区清空，而不是只读一次
                // 增加读取缓冲区大小从64到128字节，提高处理效率
                uint8_t temp_buffer[128];
                
                // 循环读取，直到UART缓冲区清空
                while (1) {
                    int len = uart_read_bytes(UART_SBUS, temp_buffer, sizeof(temp_buffer), 0);  // 非阻塞读取
                    if (len <= 0) {
                        break;  // 没有更多数据，退出循环
                    }
#if ENABLE_SBUS_RAW_DATA
                    // 打印接收到的原始数据（用于调试）
                    ESP_LOGD(TAG, "📥 接收到 %d 字节原始数据", len);
                    for (int i = 0; i < len; i++) {
                        ESP_LOGD(TAG, "   [%d] 0x%02X (%d)", i, temp_buffer[i], temp_buffer[i]);
                    }
#endif

                    // 处理接收到的每个字节
                    for (int i = 0; i < len; i++) {
                        data = temp_buffer[i];
                        byte_count++;

#if ENABLE_SBUS_DEBUG
                        ESP_LOGD(TAG, "🔍 处理字节: 0x%02X, 当前位置: %d", data, g_sbus_pt & 0x7F);
#endif

                        // 使用STM32相同的逻辑：检查最高位标志
                        if ((g_sbus_pt & 0x80) == 0) { // 数据未解析
                            if (g_sbus_pt > (LEN_SBUS - 1)) {
                                // 缓冲区满，重新开始
#if ENABLE_SBUS_DEBUG
                                ESP_LOGW(TAG, "⚠️ SBUS缓冲区满，重新开始");
#endif
                                g_sbus_pt = 0;
                            }

                            // 存入缓冲区
                            g_sbus_rx_buf[g_sbus_pt] = data;
                            g_sbus_pt++;

                            // 判断帧头
                            if (g_sbus_pt == 1) {
                                if (data != 0x0f) {
#if ENABLE_SBUS_FRAME_INFO
                                    ESP_LOGD(TAG, "❌ 帧头错误: 0x%02X (期望: 0x0F)", data);
#endif
                                    header_error_count++;
                                    g_sbus_pt--; // 回退，重新等待
                                } else {
#if ENABLE_SBUS_FRAME_INFO
                                    ESP_LOGD(TAG, "✅ 检测到SBUS帧头: 0x%02X", data);
#endif
                                }
                            } else if (g_sbus_pt == 25) {
                                // 🔧 帧尾字节 - 放宽校验，接受任意值
                                // 标准SBUS帧尾应为0x00，但部分设备可能使用其他值
                                // 不再严格校验帧尾，只要帧头正确且长度达到25字节即认为有效
#if ENABLE_SBUS_FRAME_INFO
                                if (data != 0x00) {
                                    ESP_LOGD(TAG, "⚠️ 帧尾非标准值: 0x%02X (标准: 0x00)，但仍接受", data);
                                } else {
                                    ESP_LOGD(TAG, "✅ 检测到SBUS帧尾: 0x%02X，完整帧接收完成", data);
                                }
#endif
                                // 无论帧尾值如何，都标记帧接收完成
                                g_sbus_pt |= 0x80; // 标记一帧数据的接收
                                // 更新最后接收帧的时间戳（用于超时检测）
                                last_frame_time = xTaskGetTickCount();
                                first_frame_received = true;
                                // LED指示
                                gpio_set_level(LED1_GREEN_PIN, 0);
                                gpio_set_level(LED2_GREEN_PIN, 0);
                                // 通知处理任务有新数据
                                if (sbus_data_ready_sem != NULL) {
                                    xSemaphoreGive(sbus_data_ready_sem);
                                }
                                // 统计非标准帧尾（用于调试参考）
                                if (data != 0x00) {
                                    footer_error_count++;
                                }
                            }
                        }
                    } // 关闭 for 循环
                }  // 关闭 while 循环（清空UART缓冲区）
            } else {
                ESP_LOGD(TAG, "UART event type: %d", event.type);
            }
        } else {
            // 超时，没有接收到UART事件
            // 基于实际SBUS帧接收时间判断超时，而不是基于UART事件队列超时
            if (first_frame_received) {
                TickType_t current_time = xTaskGetTickCount();
                TickType_t time_since_last_frame = current_time - last_frame_time;
                
                // SBUS标准更新率：模拟模式14ms，高速模式7ms
                // 如果超过100ms（约7倍正常间隔）没有收到完整帧，则警告
                if (time_since_last_frame > pdMS_TO_TICKS(100)) {
                    static TickType_t last_warning_time = 0;
                    static uint32_t frame_timeout_count = 0;  // 帧超时计数
                    
                    // 每5秒只警告一次，避免日志刷屏
                    if (current_time - last_warning_time > pdMS_TO_TICKS(5000)) {
                        // 检查UART缓冲区状态，判断是信号丢失还是数据错误
                        size_t uart_buf_len = 0;
                        uart_get_buffered_data_len(UART_SBUS, &uart_buf_len);
                        
                        if (uart_buf_len == 0) {
                            // 缓冲区空 - 可能是硬件信号丢失
                            ESP_LOGW(TAG, "⚠️ No SBUS frame for %lu ms - 信号可能丢失 (UART缓冲区空)", 
                                    (unsigned long)(time_since_last_frame * portTICK_PERIOD_MS));
                            ESP_LOGW(TAG, "📡 检查: 1)接收机电源 2)遥控器开启 3)GPIO22连接");
                        } else {
                            // 缓冲区有数据但无有效帧 - 可能是数据错误
                            ESP_LOGW(TAG, "⚠️ No SBUS frame for %lu ms - 帧解析失败 (缓冲区%u字节)", 
                                    (unsigned long)(time_since_last_frame * portTICK_PERIOD_MS),
                                    (unsigned int)uart_buf_len);
                            ESP_LOGW(TAG, "📊 当前解析状态: pt=%d, 累计错误: 帧头%lu/帧尾%lu",
                                    g_sbus_pt & 0x7F, 
                                    (unsigned long)header_error_count,
                                    (unsigned long)footer_error_count);
                        }
                        last_warning_time = current_time;
                        
                        // 记录错误统计（用于调试）
                        frame_timeout_count++;
                        (void)frame_timeout_count;  // 避免未使用警告
                    }
                }
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

    // 🔧 优化：增加UART接收缓冲区大小（从1024增加到2048字节）
    // 防止高频SBUS数据（71.4Hz）导致缓冲区溢出
    // SBUS帧25字节，71.4Hz = 每秒1785字节，2048字节可容纳约1.15秒的数据
    ESP_ERROR_CHECK(uart_driver_install(UART_SBUS, 2048, 0, 50, &sbus_uart_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_SBUS, &uart_config));

    // 设置GPIO22作为UART2接收引脚
    // 用于接收SBUS信号（来自遥控接收机）
    ESP_ERROR_CHECK(uart_set_pin(UART_SBUS, UART_PIN_NO_CHANGE, GPIO_NUM_22, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // SBUS使用反相逻辑，硬件无反相器时必须启用软件反相
    ESP_ERROR_CHECK(uart_set_line_inverse(UART_SBUS, UART_SIGNAL_RXD_INV));
    ESP_LOGI(TAG, "🔄 Signal inversion: ENABLED (no hardware inverter)");

    // 创建SBUS数据就绪信号量
    sbus_data_ready_sem = xSemaphoreCreateBinary();
    if (sbus_data_ready_sem == NULL) {
        ESP_LOGE(TAG, "❌ Failed to create SBUS data ready semaphore");
        return ESP_ERR_NO_MEM;
    }

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
 * SBUS协议：25字节 = [0x0F] + [data1-22] + [flags] + [0x00]
 * 每个通道11位，范围0-2047
 * 更新率：模拟模式14ms (71.4Hz)，高速模式7ms (142.9Hz)
 */
uint8_t parse_sbus_msg(uint8_t* sbus_data, uint16_t* channel)
{
#if ENABLE_SBUS_RAW_DATA
    // 打印完整的SBUS原始帧数据
    ESP_LOGD(TAG, "📦 SBUS原始帧数据 (25字节):");
    for (int i = 0; i < 25; i++) {
        ESP_LOGD(TAG, "   [%02d] 0x%02X (%3d)", i, sbus_data[i], sbus_data[i]);
    }
#endif

    // 按照SBUS协议正确解析16个通道（每个通道11位）
    // data1-22包含16个通道的数据

    // 使用正确的SBUS解析方式
    uint16_t raw_channel[16];
    raw_channel[0] = (sbus_data[1] >> 0 | sbus_data[2] << 8) & 0x07FF;
    raw_channel[1] = (sbus_data[2] >> 3 | sbus_data[3] << 5) & 0x07FF;
    raw_channel[2] = (sbus_data[3] >> 6 | sbus_data[4] << 2 | sbus_data[5] << 10) & 0x07FF;
    raw_channel[3] = (sbus_data[5] >> 1 | sbus_data[6] << 7) & 0x07FF;
    raw_channel[4] = (sbus_data[6] >> 4 | sbus_data[7] << 4) & 0x07FF;
    raw_channel[5] = (sbus_data[7] >> 7 | sbus_data[8] << 1 | sbus_data[9] << 9) & 0x07FF;
    raw_channel[6] = (sbus_data[9] >> 2 | sbus_data[10] << 6) & 0x07FF;
    raw_channel[7] = (sbus_data[10] >> 5 | sbus_data[11] << 3) & 0x07FF;
    raw_channel[8] = (sbus_data[12] >> 0 | sbus_data[13] << 8) & 0x07FF;
    raw_channel[9] = (sbus_data[13] >> 3 | sbus_data[14] << 5) & 0x07FF;
    raw_channel[10] = (sbus_data[14] >> 6 | sbus_data[15] << 2 | sbus_data[16] << 10) & 0x07FF;
    raw_channel[11] = (sbus_data[16] >> 1 | sbus_data[17] << 7) & 0x07FF;
    raw_channel[12] = (sbus_data[17] >> 4 | sbus_data[18] << 4) & 0x07FF;
    raw_channel[13] = (sbus_data[18] >> 7 | sbus_data[19] << 1 | sbus_data[20] << 9) & 0x07FF;
    raw_channel[14] = (sbus_data[20] >> 2 | sbus_data[21] << 6) & 0x07FF;
    raw_channel[15] = (sbus_data[21] >> 5 | sbus_data[22] << 3) & 0x07FF;

#if ENABLE_SBUS_DEBUG
    // 打印原始通道值（0-2047范围）
    ESP_LOGD(TAG, "🔢 SBUS原始通道值 (0-2047):");
    for (int i = 0; i < 16; i++) {
        ESP_LOGD(TAG, "   CH%02d: %4d", i, raw_channel[i]);
    }
#endif

    // 🔧 SBUS原始值映射到标准PWM范围 (282~1722 → 1050~1950)
    // 改进映射公式：使用四舍五入提高精度，减少舍入误差
    // 公式：(raw - 282) * 5 / 8 + 1050
    // 改进：先乘以5，加上4（相当于+0.5*8），再除以8，实现四舍五入
    for (int i = 0; i < LEN_CHANEL; i++) {
        int32_t raw = raw_channel[i];
        int32_t diff = raw - 282;
        // 使用四舍五入：先乘以5，加上4（相当于+0.5*8），再除以8
        int32_t mapped = (diff * 5 + (diff >= 0 ? 4 : -4)) / 8 + 1050;
        
        // 限制在有效范围内
        if (mapped > 1950) mapped = 1950;
        if (mapped < 1050) mapped = 1050;
        
        channel[i] = (uint16_t)mapped;
    }

#if ENABLE_SBUS_DEBUG
    // 打印映射后的通道值（1050-1950范围）
    ESP_LOGD(TAG, "📊 SBUS映射通道值 (1050-1950):");
    for (int i = 0; i < LEN_CHANEL; i++) {
        ESP_LOGD(TAG, "   CH%02d: %4d", i, channel[i]);
    }
#endif

    // ⚡ 性能优化：减少日志输出频率，降低CPU占用
    static uint16_t last_channels[LEN_CHANEL] = {0};
    static bool first_sbus_data = true;
    static uint32_t frame_count = 0;
    bool significant_change = false;

    frame_count++;

    // 检查关键通道是否有显著变化（阈值从20增加到30，减少打印频率）
    // CH4 为遥控使能开关，也加入关键通道
    uint8_t key_ch[] = {0, 1, 2, 3, 4, 6, 7};
    for (int i = 0; i < 7; i++) {
        uint8_t ch = key_ch[i];
        if (abs((int16_t)channel[ch] - (int16_t)last_channels[ch]) > 30) {
            significant_change = true;
            break;
        }
    }

#if ENABLE_SBUS_DEBUG
    // 调试模式：有变化时打印，否则每500帧打印一次（约7秒）
    if (significant_change || first_sbus_data) {
        // 🔔 检测到变化时打印
        ESP_LOGI(TAG, "🔔 SBUS变化! CH0:%4d CH2:%4d CH3:%4d CH4:%4d",
                 channel[0], channel[2], channel[3], channel[4]);
    } else if (frame_count % 500 == 0) {
        // 每500帧打印一次心跳（约7秒，71Hz）
        ESP_LOGI(TAG, "💓 SBUS心跳 #%lu - CH0:%4d CH2:%4d CH4:%4d",
                 frame_count, channel[0], channel[2], channel[4]);
    }
#else
    // 正常模式：只在有显著变化时打印关键通道
    if (first_sbus_data || significant_change) {
        ESP_LOGI(TAG, "🔔 SBUS变化! CH0:%4u CH2:%4u CH3:%4u CH4:%4u",
                 channel[0], channel[2], channel[3], channel[4]);
    } else if (frame_count % 500 == 0) {
        // 每500帧打印一次心跳（约7秒）
        ESP_LOGI(TAG, "💓 SBUS心跳 #%lu", frame_count);
    }
#endif

    // 更新保存的通道值
    for (int i = 0; i < LEN_CHANEL; i++) {
        last_channels[i] = channel[i];
    }
    first_sbus_data = false;

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

/**
 * 等待SBUS数据就绪信号量
 */
BaseType_t sbus_wait_data_ready(TickType_t timeout_ms)
{
    if (sbus_data_ready_sem == NULL) {
        return pdFALSE;
    }
    return xSemaphoreTake(sbus_data_ready_sem, timeout_ms);
}
