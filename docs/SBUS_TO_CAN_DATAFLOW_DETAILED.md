# SBUS数据流到CAN发送详细分析

本文档详细说明SBUS数据从接收、处理到CAN发送的完整数据流，帮助理解代码逻辑和发现潜在问题。

## 📊 数据流概览

```
SBUS接收机 → UART2(GPIO22) → SBUS解析 → 通道处理 → 电机控制 → CAN发送 → 电机驱动器
```

## 🔄 完整数据流路径

### 阶段1: SBUS硬件接收 (sbus.c)

**位置**: `main/sbus.c` - `sbus_uart_task()`

**流程**:
1. **UART配置**: 
   - 波特率: 100000 bps
   - 数据位: 8位
   - 校验: 偶校验 (EVEN)
   - 停止位: 2位
   - 信号反相: 启用 (SBUS使用反相逻辑)

```18:144:main/sbus.c
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
                                    g_sbus_pt--; // 回退，重新等待
                                } else {
#if ENABLE_SBUS_FRAME_INFO
                                    ESP_LOGD(TAG, "✅ 检测到SBUS帧头: 0x%02X", data);
#endif
                                }
                            } else if (g_sbus_pt == 25) {
                                // 判断帧尾
                                if (data == 0x00) {
#if ENABLE_SBUS_FRAME_INFO
                                    ESP_LOGD(TAG, "✅ 检测到SBUS帧尾: 0x%02X，完整帧接收完成", data);
#endif
                                    g_sbus_pt |= 0x80; // 标记一帧数据的接收
                                    // LED指示
                                    gpio_set_level(LED1_GREEN_PIN, 0);
                                    gpio_set_level(LED2_GREEN_PIN, 0);
                                } else {
#if ENABLE_SBUS_FRAME_INFO
                                    ESP_LOGW(TAG, "❌ 帧尾错误: 0x%02X (期望: 0x00)，丢弃帧", data);
#endif
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
```

**关键点**:
- 使用 `g_sbus_rx_buf[25]` 存储完整SBUS帧
- 使用 `g_sbus_pt` 作为指针，最高位(0x80)标记帧接收完成
- 帧头检查: 第1字节必须是 `0x0F`
- 帧尾检查: 第25字节必须是 `0x00`
- 完整帧接收后，设置 `g_sbus_pt |= 0x80` 标记

**SBUS帧格式**:
```
Byte[0]:  0x0F (帧头)
Byte[1-22]: 通道数据 (16个通道，每个11位)
Byte[23]: 标志位 (fail-safe, frame lost等)
Byte[24]: 0x00 (帧尾)
```

---

### 阶段2: SBUS数据获取 (sbus.c)

**位置**: `main/sbus.c` - `sbus_get_data()`

**流程**:
```307:316:main/sbus.c
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
```

**关键点**:
- 检查 `g_sbus_pt & 0x80` 判断是否有新帧
- 复制完整25字节数据
- 清除标志位，准备接收下一帧

---

### 阶段3: SBUS数据处理任务 (main.c)

**位置**: `main/main.c` - `sbus_process_task()`

**流程**:
```254:292:main/main.c
static void sbus_process_task(void *pvParameters)
{
    uint8_t sbus_raw_data[LEN_SBUS] = {0};
    uint16_t ch_val[LEN_CHANEL] = {0};
    sbus_data_t sbus_data;

    // SBUS处理任务已启动

    while (1) {
        // 检查SBUS数据
        if (sbus_get_data(sbus_raw_data)) {
            // 解析SBUS数据
            parse_sbus_msg(sbus_raw_data, ch_val);

            // SBUS通道值已在parse_sbus_msg函数中打印，此处不重复打印

            // 保存SBUS状态用于Web接口
            memcpy(g_last_sbus_channels, ch_val, sizeof(ch_val));
            g_last_sbus_update = xTaskGetTickCount();

            // 复制通道值到队列数据结构
            memcpy(sbus_data.channel, ch_val, sizeof(ch_val));

            // 发送到队列，如果队列满则覆盖旧数据
            if (xQueueSend(sbus_queue, &sbus_data, 0) != pdPASS) {
                // 队列满时，先取出一个旧数据，再放入新数据
                sbus_data_t dummy;
                xQueueReceive(sbus_queue, &dummy, 0);
                xQueueSend(sbus_queue, &sbus_data, 0);
                // SBUS队列已满，覆盖旧数据
            }
        }

        // ⚡ 性能优化：2ms延迟足够处理SBUS数据
        // SBUS更新率：模拟模式14ms (71.4Hz)，高速模式7ms (142.9Hz)
        // 2ms延迟可支持高达500Hz的处理频率，完全满足SBUS需求
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
```

**关键点**:
- 任务优先级: 12 (高优先级)
- 循环延迟: 2ms
- 使用FreeRTOS队列 `sbus_queue` 传递数据
- 队列满时覆盖旧数据，确保最新数据优先

---

### 阶段4: SBUS通道解析 (sbus.c)

**位置**: `main/sbus.c` - `parse_sbus_msg()`

**流程**:
```198:302:main/sbus.c
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

    // SBUS原始值映射到标准PWM范围 (282~1722 → 1050~1950)
    for (int i = 0; i < LEN_CHANEL; i++) {
        channel[i] = (raw_channel[i] - 282) * 5 / 8 + 1050;
    }

#if ENABLE_SBUS_DEBUG
    // 打印映射后的通道值（1050-1950范围）
    ESP_LOGD(TAG, "📊 SBUS映射通道值 (1050-1950):");
    for (int i = 0; i < LEN_CHANEL; i++) {
        ESP_LOGD(TAG, "   CH%02d: %4d", i, channel[i]);
    }
#endif

    // ⚡ 性能优化：减少日志输出频率，降低CPU占用
    static uint16_t last_channels[16] = {0};
    static bool first_sbus_data = true;
    static uint32_t frame_count = 0;
    bool significant_change = false;

    frame_count++;

    // 检查关键通道是否有显著变化（阈值从20增加到30，减少打印频率）
    uint8_t key_ch[] = {0, 1, 2, 3, 6, 7};
    for (int i = 0; i < 6; i++) {
        uint8_t ch = key_ch[i];
        if (abs((int16_t)channel[ch] - (int16_t)last_channels[ch]) > 30) {
            significant_change = true;
            break;
        }
    }

#if ENABLE_SBUS_DEBUG
    // 调试模式：减少打印频率，每5帧打印一次而不是每帧
    if (frame_count % 5 == 0) {
        ESP_LOGI(TAG, "🎮 SBUS帧#%lu - 所有通道数据:", frame_count);
        ESP_LOGI(TAG, "   CH0-3:  %4d %4d %4d %4d", channel[0], channel[1], channel[2], channel[3]);
        ESP_LOGI(TAG, "   CH4-7:  %4d %4d %4d %4d", channel[4], channel[5], channel[6], channel[7]);
        ESP_LOGI(TAG, "   CH8-11: %4d %4d %4d %4d", channel[8], channel[9], channel[10], channel[11]);
    }

    // 避免未使用变量警告
    (void)significant_change;
    (void)first_sbus_data;
#else
    // 正常模式：只在有显著变化时打印关键通道
    if (first_sbus_data || significant_change) {
        ESP_LOGI(TAG, "🎮 SBUS帧#%lu - 关键通道: CH0:%4u CH1:%4u CH2:%4u CH3:%4u CH6:%4u CH7:%4u",
                 frame_count, channel[0], channel[1], channel[2], channel[3], channel[6], channel[7]);
    } else {
        // 每100帧打印一次状态（从10增加到100），减少日志负担
        if (frame_count % 100 == 0) {
            ESP_LOGD(TAG, "🎮 SBUS活跃 - 帧#%lu: CH0:%4u CH2:%4u CH3:%4u",
                     frame_count, channel[0], channel[2], channel[3]);
        }
    }
#endif

    // 更新保存的通道值
    for (int i = 0; i < 16; i++) {
        last_channels[i] = channel[i];
    }
    first_sbus_data = false;

    return 0;
}
```

**关键点**:
- **原始值范围**: 0-2047 (11位，每个通道)
- **映射公式**: `channel[i] = (raw_channel[i] - 282) * 5 / 8 + 1050`
- **映射后范围**: 1050-1950 (标准PWM范围)
- **中位值**: 1500

**通道解析示例** (通道0):
```
Byte[1] = 0xAB (低8位)
Byte[2] = 0xCD (高3位在低3位)
通道0 = (Byte[1] >> 0 | Byte[2] << 8) & 0x07FF
      = (0xAB | 0xCD00) & 0x07FF
      = 0x0DAB (11位)
```

---

### 阶段5: 电机控制任务 (main.c)

**位置**: `main/main.c` - `motor_control_task()`

**流程**:
```358:403:main/main.c
static void motor_control_task(void *pvParameters)
{
    sbus_data_t sbus_data;
    motor_cmd_t motor_cmd;
    uint32_t cmd_timeout = 0;
    bool sbus_control = false;

    ESP_LOGI(TAG, "电机控制任务已启动");

    while (1) {
        // 检查是否有CMD_VEL命令
        if (xQueueReceive(cmd_queue, &motor_cmd, 0) == pdPASS) {
            // 收到CMD_VEL命令，优先处理
            parse_cmd_vel(motor_cmd.speed_left, motor_cmd.speed_right);
            cmd_timeout = xTaskGetTickCount() + pdMS_TO_TICKS(1000); // 1秒超时
            sbus_control = false;

            // 保存电机状态用于Web接口
            g_last_motor_left = motor_cmd.speed_left;
            g_last_motor_right = motor_cmd.speed_right;
            g_last_motor_update = xTaskGetTickCount();

            // 注销LED指示 - 接收到CMD_VEL命令时，两组LED的绿色闪烁
            // 注意：共阳极LED，取反操作需要考虑逻辑（1变0，0变1）
            // gpio_set_level(LED1_GREEN_PIN, !gpio_get_level(LED1_GREEN_PIN));
            // gpio_set_level(LED2_GREEN_PIN, !gpio_get_level(LED2_GREEN_PIN));
        }
        // 检查是否有SBUS数据
        else if (xQueueReceive(sbus_queue, &sbus_data, 0) == pdPASS) {
            // 如果没有活跃的CMD_VEL命令或CMD_VEL已超时，则处理SBUS
            if (sbus_control || xTaskGetTickCount() > cmd_timeout) {
                parse_chan_val(sbus_data.channel);
                sbus_control = true;

                // 注销LED指示 - 接收到SBUS命令时，两组LED的蓝色闪烁
                // 注意：共阳极LED，取反操作需要考虑逻辑（1变0，0变1）
                // gpio_set_level(LED1_BLUE_PIN, !gpio_get_level(LED1_BLUE_PIN));
                // gpio_set_level(LED2_BLUE_PIN, !gpio_get_level(LED2_BLUE_PIN));
            }
        }

        // ⚡ 性能优化：减少延迟从10ms到2ms，提高控制响应速度
        // 电机控制需要快速响应SBUS输入，2ms延迟可提供高达500Hz的控制频率
        vTaskDelay(pdMS_TO_TICKS(2));
    }
}
```

**关键点**:
- 任务优先级: 10 (中优先级)
- 循环延迟: 2ms
- CMD_VEL命令优先级高于SBUS
- CMD_VEL超时1秒后切换回SBUS控制

---

### 阶段6: 通道值解析 (channel_parse.c)

**位置**: `main/channel_parse.c` - `parse_chan_val()`

**流程**:
```77:184:main/channel_parse.c
uint8_t parse_chan_val(uint16_t* ch_val)
{
    // ⚡ 性能优化：始终执行控制逻辑，确保实时响应
    // 移除变化检测的限制，让CAN总线始终发送最新的控制命令
    // 这样可以确保即使微小的控制变化也能立即响应

    // 始终执行控制逻辑，不再跳过处理
    if (true) {  // 原来是: if (first_run || channels_changed)
        if (first_run) {
            ESP_LOGI(TAG, "🚀 First run - initializing track vehicle control");
            first_run = false;
        }

        int8_t sp_fb = chg_val(ch_val[2]); // 前后分量，向前>0
        int8_t sp_lr = chg_val(ch_val[0]); // 左右分量，向右>0

        // 记录特殊模式状态变化
        static bool last_single_hand_mode = false;
        static bool last_low_speed_mode = false;
        bool current_single_hand = (ch_val[6] == 1950);
        bool current_low_speed = (ch_val[7] == 1950);

        if (current_single_hand != last_single_hand_mode) {
            ESP_LOGI(TAG, "🤟 Single-hand mode: %s", current_single_hand ? "ON" : "OFF");
            last_single_hand_mode = current_single_hand;
        }

        if (current_low_speed != last_low_speed_mode) {
            ESP_LOGI(TAG, "🐌 Low speed mode: %s", current_low_speed ? "ON" : "OFF");
            last_low_speed_mode = current_low_speed;
        }

        if (current_single_hand) {
            sp_lr = chg_val(ch_val[3]); // 左右分量，向右>0
        }

        if (current_low_speed) {
            sp_fb /= 2;
            sp_lr /= 2;
        }

        ESP_LOGD(TAG, "🎯 Control values - FB:%d LR:%d", sp_fb, sp_lr);

        // 履带车差速控制逻辑
        static int8_t last_left_speed = 0, last_right_speed = 0;
        int8_t left_speed, right_speed;

        // ⚡ 性能优化：增大速度变化阈值，减少不必要的日志输出
        // 从5增加到15，只在显著变化时才打印日志
        #define SPEED_LOG_THRESHOLD 15

        if (sp_fb == 0) {
            if (sp_lr == 0) {
                // 停止
                left_speed = 0;
                right_speed = 0;
                if (last_left_speed != 0 || last_right_speed != 0) {
                    ESP_LOGI(TAG, "⏹️ STOP");
                }
            } else {
                // 原地转向
                left_speed = sp_lr;
                right_speed = (-1) * sp_lr;
                if (abs(left_speed - last_left_speed) > SPEED_LOG_THRESHOLD || abs(right_speed - last_right_speed) > SPEED_LOG_THRESHOLD) {
                    ESP_LOGI(TAG, "🔄 TURN IN PLACE - LR:%d", sp_lr);
                }
            }
        } else {
            if (sp_lr == 0) {
                // 前进或后退
                left_speed = sp_fb;
                right_speed = sp_fb;
                if (abs(left_speed - last_left_speed) > SPEED_LOG_THRESHOLD || abs(right_speed - last_right_speed) > SPEED_LOG_THRESHOLD) {
                    ESP_LOGI(TAG, "%s STRAIGHT - Speed:%d", sp_fb > 0 ? "⬆️ FORWARD" : "⬇️ BACKWARD", sp_fb);
                }
            } else if (sp_lr > 0) {
                // 差速右转
                left_speed = sp_fb;
                right_speed = cal_offset(sp_fb, sp_lr);
                if (abs(left_speed - last_left_speed) > SPEED_LOG_THRESHOLD || abs(right_speed - last_right_speed) > SPEED_LOG_THRESHOLD) {
                    ESP_LOGI(TAG, "↗️ DIFFERENTIAL RIGHT - Left:%d Right:%d", left_speed, right_speed);
                }
            } else {
                // 差速左转
                left_speed = cal_offset(sp_fb, sp_lr);
                right_speed = sp_fb;
                if (abs(left_speed - last_left_speed) > SPEED_LOG_THRESHOLD || abs(right_speed - last_right_speed) > SPEED_LOG_THRESHOLD) {
                    ESP_LOGI(TAG, "↖️ DIFFERENTIAL LEFT - Left:%d Right:%d", left_speed, right_speed);
                }
            }
        }

        // 执行电机控制
        intf_move(left_speed, right_speed);

        // 更新上次速度值
        last_left_speed = left_speed;
        last_right_speed = right_speed;

        // 更新保存的通道值（用于变化检测和日志输出）
        update_last_channels(ch_val);
    }

    // ⚡ 性能优化：移除了"无变化则跳过"的逻辑
    // 现在每次调用都会发送CAN命令，确保实时性和准确性

    return 0;
}
```

**通道映射**:
- **通道0** (`ch_val[0]`): 左右方向控制，右>0 → `sp_lr`
- **通道2** (`ch_val[2]`): 前后方向控制，前>0 → `sp_fb`
- **通道3** (`ch_val[3]`): 备用左右方向（单手模式）
- **通道6** (`ch_val[6]`): 单手模式开关，1950时启用
- **通道7** (`ch_val[7]`): 低速模式开关，1950时启用

**速度转换函数**:
```26:31:main/channel_parse.c
static int8_t chg_val(uint16_t val)
{
    // 优化的映射算法：(val-1500)/9*2，范围900/9*2=200，即-100到+100
    int8_t sp = (((int16_t)val - 1500) / 9 * 2) & 0xff;
    return sp;
}
```

**转换公式**:
- 输入: 1050-1950 (中位值1500)
- 输出: -100到+100
- 公式: `sp = ((val - 1500) / 9 * 2)`
- 示例: 
  - 1500 → 0
  - 1950 → 100
  - 1050 → -100

**差速控制逻辑**:
- **前进/后退**: `left = right = sp_fb`
- **原地转向**: `left = sp_lr`, `right = -sp_lr`
- **差速右转**: `left = sp_fb`, `right = cal_offset(sp_fb, sp_lr)`
- **差速左转**: `left = cal_offset(sp_fb, sp_lr)`, `right = sp_fb`

---

### 阶段7: 电机驱动接口 (drv_keyadouble.c)

**位置**: `main/drv_keyadouble.c` - `intf_move_keyadouble()`

**流程**:
```166:201:main/drv_keyadouble.c
uint8_t intf_move_keyadouble(int8_t speed_left, int8_t speed_right)
{
    if ((abs(speed_left) > 100) || (abs(speed_right) > 100)) {
        printf("Wrong parameter in intf_move!!![lf=%d],[ri=%d]", speed_left, speed_right);
        return 1;
    }

    // 更新刹车标志
    if (speed_left != 0) {
        bk_flag_left = 1; // 1为松开
    } else {
        bk_flag_left = 0; // 0为刹车
    }

    if (speed_right != 0) {
        bk_flag_right = 1; // 1为松开
    } else {
        bk_flag_right = 0; // 0为刹车
    }

    // ⚡ 性能优化：只在首次调用时发送使能命令，避免重复发送
    // 电机驱动器在使能后会保持状态，无需每次都发送使能命令
    // 这将减少50%的CAN帧发送量（从4帧减少到2帧）
    if (!motor_enabled) {
        motor_control(CMD_ENABLE, MOTOR_CHANNEL_A, 0); // 使能A路(左侧)
        motor_control(CMD_ENABLE, MOTOR_CHANNEL_B, 0); // 使能B路(右侧)
        motor_enabled = true;
        ESP_LOGI(TAG, "⚡ Motors enabled (one-time initialization)");
    }

    // 设置速度命令（每次都需要发送）
    motor_control(CMD_SPEED, MOTOR_CHANNEL_A, speed_left); // A路(左侧)速度
    motor_control(CMD_SPEED, MOTOR_CHANNEL_B, speed_right); // B路(右侧)速度

    return 0;
}
```

**关键点**:
- 参数验证: 速度范围 -100 到 +100
- 首次调用时发送使能命令（2帧）
- 每次调用发送速度命令（2帧）
- 总共: 首次4帧，后续每次2帧

---

### 阶段8: 电机控制命令封装 (drv_keyadouble.c)

**位置**: `main/drv_keyadouble.c` - `motor_control()`

**流程**:
```99:142:main/drv_keyadouble.c
static void motor_control(uint8_t cmd_type, uint8_t channel, int8_t speed)
{
    uint8_t tx_data[8] = {0};
    uint32_t tx_id = DRIVER_TX_ID + DRIVER_ADDRESS;

    if (cmd_type == CMD_ENABLE) {
        // 使能电机: 23 0D 20 01/02 00 00 00 00
        tx_data[0] = 0x23;
        tx_data[1] = 0x0D;
        tx_data[2] = 0x20;
        tx_data[3] = channel; // 01=A路(左侧), 02=B路(右侧)
        tx_data[4] = 0x00;
        tx_data[5] = 0x00;
        tx_data[6] = 0x00;
        tx_data[7] = 0x00;
    } else if (cmd_type == CMD_DISABLE) {
        // 失能电机: 23 0C 20 01/02 00 00 00 00
        tx_data[0] = 0x23;
        tx_data[1] = 0x0C;
        tx_data[2] = 0x20;
        tx_data[3] = channel; // 01=A路(左侧), 02=B路(右侧)
        tx_data[4] = 0x00;
        tx_data[5] = 0x00;
        tx_data[6] = 0x00;
        tx_data[7] = 0x00;
    } else if (cmd_type == CMD_SPEED) {
        // 设置速度: 23 00 20 01/02 HH HH LL LL
        tx_data[0] = 0x23;
        tx_data[1] = 0x00;
        tx_data[2] = 0x20;
        tx_data[3] = channel; // 01=A路(左侧), 02=B路(右侧)

        // 将-100到100的速度转换为-10000到10000
        int32_t sp_value = (int32_t)speed * 100;

        // 32位有符号整数表示，高字节在前
        tx_data[4] = (sp_value >> 24) & 0xFF; // 最高字节
        tx_data[5] = (sp_value >> 16) & 0xFF;
        tx_data[6] = (sp_value >> 8) & 0xFF;
        tx_data[7] = sp_value & 0xFF; // 最低字节
    }

    keya_send_data(tx_id, tx_data);
}
```

**CAN消息格式**:

**使能命令**:
```
ID: 0x06000001
Data: [0x23, 0x0D, 0x20, 0x01/0x02, 0x00, 0x00, 0x00, 0x00]
```

**速度命令**:
```
ID: 0x06000001
Data: [0x23, 0x00, 0x20, 0x01/0x02, HH, HH, LL, LL]
      HH HH LL LL = 32位有符号整数，大端序
      例如: speed=50 → sp_value=5000 → 0x00 0x00 0x13 0x88
```

**速度值转换示例**:
- `speed = 50` → `sp_value = 5000` → `[0x00, 0x00, 0x13, 0x88]`
- `speed = -50` → `sp_value = -5000` → `[0xFF, 0xFF, 0xEC, 0x78]`
- `speed = 100` → `sp_value = 10000` → `[0x00, 0x00, 0x27, 0x10]`

---

### 阶段9: CAN数据发送 (drv_keyadouble.c)

**位置**: `main/drv_keyadouble.c` - `keya_send_data()`

**流程**:
```43:91:main/drv_keyadouble.c
static void keya_send_data(uint32_t id, uint8_t* data)
{
    twai_message_t message;
    message.extd = 1;                 // 扩展帧(29位ID)
    message.identifier = id;
    message.data_length_code = 8;     // 帧长度8字节
    message.rtr = 0;                  // 数据帧

    // 复制数据
    for (int i = 0; i < 8; i++) {
        message.data[i] = data[i];
    }

    // 发送消息 - 不等待ACK，立即发送
    esp_err_t result = twai_transmit(&message, 0);  // 超时设为0，不等待
    if (result != ESP_OK) {
        // 只在严重错误时打印详细信息，超时错误降级为调试级别
        if (result == ESP_ERR_TIMEOUT) {
            ESP_LOGD(TAG, "CAN send timeout (normal in no-ACK mode)");
        } else {
            ESP_LOGW(TAG, "CAN send error: %s", esp_err_to_name(result));
            // 只在严重错误时打印状态
            twai_status_info_t status_info;
            if (twai_get_status_info(&status_info) == ESP_OK) {
                ESP_LOGW(TAG, "CAN Status - State: %" PRIu32 ", TX Error: %" PRIu32 ", RX Error: %" PRIu32,
                         (unsigned long)status_info.state, (unsigned long)status_info.tx_error_counter, (unsigned long)status_info.rx_error_counter);
            }
        }
    }

    // 只在调试模式下打印详细的CAN数据
    ESP_LOGD(TAG, "CAN TX: %08" PRIX32 " [%02X %02X %02X %02X %02X %02X %02X %02X]",
             id, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

    // 只在速度命令时打印简化的速度信息 (0x23 0x00 0x20 channel speed_bytes)
    if (data[0] == 0x23 && data[1] == 0x00 && data[2] == 0x20) {
        int32_t sp_value_tx = ((int32_t)data[4] << 24) |
                             ((int32_t)data[5] << 16) |
                             ((int32_t)data[6] << 8) |
                             ((int32_t)data[7]);
        int8_t actual_speed = (int8_t)(sp_value_tx / 100);
        uint8_t channel = data[3];
        ESP_LOGD(TAG, "Motor Ch%d speed: %d", channel, actual_speed);
    }

    // ⚡ 性能优化：移除延迟，避免阻塞控制循环
    // CAN发送采用非阻塞模式(超时=0)，无需额外延迟
    // 原有的10ms延迟会导致每次电机控制延迟40ms（4帧×10ms）
}
```

**CAN配置**:
```147:158:main/drv_keyadouble.c
esp_err_t drv_keyadouble_init(void)
{
    // 初始化TWAI (CAN)
    ESP_ERROR_CHECK(twai_driver_install(&g_config, &t_config, &f_config));
    ESP_ERROR_CHECK(twai_start());

    // 初始化电机使能状态
    motor_enabled = false;

    ESP_LOGI(TAG, "Motor driver initialized");
    return ESP_OK;
}
```

**CAN硬件配置**:
- **TX引脚**: GPIO16 (连接到SN65HVD232D的D引脚)
- **RX引脚**: GPIO17 (连接到SN65HVD232D的R引脚)
- **波特率**: 250000 bps
- **模式**: 标准模式 (TWAI_MODE_NORMAL)
- **帧格式**: 扩展帧 (29位ID)
- **数据长度**: 8字节

**关键点**:
- **非阻塞发送**: `twai_transmit(&message, 0)` 超时=0，立即返回
- **错误处理**: 超时错误不打印警告（正常情况）
- **性能优化**: 无延迟，避免阻塞控制循环

---

## 📈 数据流时序图

```
时间轴 →
SBUS接收机
  │
  ├─ 发送SBUS帧 (每14ms)
  │   [0x0F][22字节数据][标志][0x00]
  │
  ▼
UART2 (GPIO22)
  │
  ├─ sbus_uart_task (优先级12, 10ms循环)
  │   ├─ 接收字节流
  │   ├─ 帧头检查 (0x0F)
  │   ├─ 帧尾检查 (0x00)
  │   └─ 标记完整帧 (g_sbus_pt |= 0x80)
  │
  ▼
sbus_process_task (优先级12, 2ms循环)
  │
  ├─ sbus_get_data() → 获取完整25字节帧
  ├─ parse_sbus_msg() → 解析16个通道
  │   ├─ 原始值: 0-2047 (11位)
  │   └─ 映射值: 1050-1950
  ├─ 保存到全局变量 (g_last_sbus_channels)
  └─ 发送到队列 (sbus_queue)
  │
  ▼
motor_control_task (优先级10, 2ms循环)
  │
  ├─ 从队列接收 (sbus_queue)
  └─ parse_chan_val()
      ├─ 通道映射
      │   ├─ CH0 → sp_lr (左右)
      │   └─ CH2 → sp_fb (前后)
      ├─ 差速控制计算
      │   ├─ left_speed
      │   └─ right_speed
      └─ intf_move_keyadouble()
          │
          ▼
intf_move_keyadouble()
  │
  ├─ 首次调用: 发送使能命令 (2帧)
  │   ├─ 使能A路 (0x06000001, [0x23,0x0D,0x20,0x01,...])
  │   └─ 使能B路 (0x06000001, [0x23,0x0D,0x20,0x02,...])
  │
  └─ 每次调用: 发送速度命令 (2帧)
      ├─ A路速度 (0x06000001, [0x23,0x00,0x20,0x01,HH,HH,LL,LL])
      └─ B路速度 (0x06000001, [0x23,0x00,0x20,0x02,HH,HH,LL,LL])
      │
      ▼
motor_control()
  │
  ├─ 封装CAN消息
  │   ├─ ID: 0x06000001
  │   ├─ 数据: 8字节
  │   └─ 速度值: -10000 ~ +10000 (32位大端序)
  │
  ▼
keya_send_data()
  │
  ├─ twai_transmit() (非阻塞, 超时=0)
  │
  ▼
CAN总线 (GPIO16/17)
  │
  ├─ SN65HVD232D收发器
  │
  ▼
电机驱动器
```

---

## 🔍 关键数据转换点

### 1. SBUS原始值 → PWM值
```
输入: 0-2047 (11位)
公式: channel = (raw - 282) * 5 / 8 + 1050
输出: 1050-1950
中位: 1500
```

### 2. PWM值 → 速度值
```
输入: 1050-1950
公式: speed = ((val - 1500) / 9 * 2)
输出: -100 到 +100
```

### 3. 速度值 → CAN数据
```
输入: -100 到 +100
公式: sp_value = speed * 100
输出: -10000 到 +10000 (32位有符号整数)
编码: 大端序 [HH, HH, LL, LL]
```

---

## ⚠️ 潜在问题分析

### 1. 数据丢失风险
- **问题**: SBUS队列满时覆盖旧数据
- **位置**: `main/main.c:278-283`
- **影响**: 可能导致控制命令丢失
- **建议**: 监控队列使用率，必要时增加队列大小

### 2. 帧同步问题
- **问题**: 帧头/帧尾检查可能误判
- **位置**: `main/sbus.c:95-121`
- **影响**: 可能导致通道值解析错误
- **建议**: 增加CRC校验或帧序号检查

### 3. 速度转换精度
- **问题**: `chg_val()` 函数使用整数除法
- **位置**: `main/channel_parse.c:26-31`
- **影响**: 可能存在精度损失
- **建议**: 使用浮点数或更高精度计算

### 4. CAN发送超时
- **问题**: 非阻塞发送可能失败但不重试
- **位置**: `main/drv_keyadouble.c:57`
- **影响**: 控制命令可能丢失
- **建议**: 增加重试机制或错误计数

### 5. 任务优先级
- **问题**: 多个高优先级任务可能竞争CPU
- **位置**: `main/main.c:1074-1098`
- **影响**: 可能导致实时性下降
- **建议**: 优化任务优先级分配

---

## 📊 性能指标

| 指标 | 数值 | 说明 |
|------|------|------|
| SBUS更新率 | 71.4Hz (14ms) | 模拟模式 |
| SBUS处理延迟 | <2ms | sbus_process_task循环 |
| 电机控制延迟 | <2ms | motor_control_task循环 |
| CAN发送延迟 | <1ms | 非阻塞发送 |
| 总延迟 | <5ms | 端到端延迟 |
| CAN帧率 | 500Hz+ | 优化后（原25Hz） |

---

## 🎯 总结

SBUS数据流经过以下关键阶段：

1. **硬件接收** (UART2) → 2. **帧解析** (SBUS协议) → 3. **通道提取** (16通道) → 4. **值映射** (PWM范围) → 5. **控制计算** (差速控制) → 6. **CAN封装** (电机命令) → 7. **CAN发送** (总线传输)

整个流程采用FreeRTOS多任务架构，确保实时性和可靠性。关键优化点包括：
- 非阻塞CAN发送
- 智能使能管理（减少50%帧数）
- 高优先级任务处理
- 队列覆盖策略（确保最新数据优先）

