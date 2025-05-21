#include "main.h"
#include "channel_parse.h"
#include "drv_keyadouble.h"
#include "sbus.h"

static const char *TAG = "MAIN";

// CMD_VEL接收缓冲区
static uint8_t g_cmd_rx_buf[LEN_CMD] = {0};
static uint8_t g_cmd_pt = 0;

// UART事件队列
static QueueHandle_t cmd_uart_queue;

// 定时器句柄
static TimerHandle_t brake_timer_left = NULL;
static TimerHandle_t brake_timer_right = NULL;

// FreeRTOS任务句柄
static TaskHandle_t sbus_task_handle = NULL;
static TaskHandle_t cmd_task_handle = NULL;
static TaskHandle_t control_task_handle = NULL;
static TaskHandle_t status_task_handle = NULL;

// FreeRTOS队列句柄
static QueueHandle_t sbus_queue = NULL;
static QueueHandle_t cmd_queue = NULL;

// 队列数据结构
typedef struct {
    uint16_t channel[LEN_CHANEL];
} sbus_data_t;

typedef struct {
    int8_t speed_left;
    int8_t speed_right;
} motor_cmd_t;

/**
 * 左刹车定时器回调函数
 */
static void brake_timer_left_callback(TimerHandle_t xTimer)
{
    if (bk_flag_left == 0) {
        // 通过CAN总线发送刹车命令
        ESP_LOGI(TAG, "Left brake applied");
        // 红色LED亮起表示刹车（共阳极LED，低电平点亮）
        gpio_set_level(LED1_RED_PIN, 0);
        gpio_set_level(LED2_RED_PIN, 0);
    }
}

/**
 * 右刹车定时器回调函数
 */
static void brake_timer_right_callback(TimerHandle_t xTimer)
{
    if (bk_flag_right == 0) {
        // 通过CAN总线发送刹车命令
        ESP_LOGI(TAG, "Right brake applied");
        // 红色LED亮起表示刹车（共阳极LED，低电平点亮）
        gpio_set_level(LED1_RED_PIN, 0);
        gpio_set_level(LED2_RED_PIN, 0);
    }
}

/**
 * SBUS数据处理任务
 * 接收SBUS数据并通过队列发送给控制任务
 */
static void sbus_process_task(void *pvParameters)
{
    uint8_t sbus_raw_data[LEN_SBUS] = {0};
    uint16_t ch_val[LEN_CHANEL] = {0};
    sbus_data_t sbus_data;

    ESP_LOGI(TAG, "SBUS处理任务已启动");

    while (1) {
        // 检查SBUS数据
        if (sbus_get_data(sbus_raw_data)) {
            // 解析SBUS数据
            parse_sbus_msg(sbus_raw_data, ch_val);

            // 打印解析后的SBUS通道值(调试用)
            ESP_LOGI(TAG, "SBUS Channels:");
            for (int i = 0; i < LEN_CHANEL; i++) {
                ESP_LOGI(TAG, "CH%d: %u", i + 1, ch_val[i]);
            }

            // 复制通道值到队列数据结构
            memcpy(sbus_data.channel, ch_val, sizeof(ch_val));

            // 发送到队列
            if (xQueueSend(sbus_queue, &sbus_data, 0) != pdPASS) {
                ESP_LOGW(TAG, "SBUS队列已满");
            }
        }

        // 短暂延时，避免过度占用CPU
        vTaskDelay(pdMS_TO_TICKS(10));
    }
}

/**
 * CMD_VEL UART接收任务
 * 接收CMD_VEL命令并通过队列发送给控制任务
 */
static void cmd_uart_task(void *pvParameters)
{
    uart_event_t event;
    uint8_t data;
    motor_cmd_t motor_cmd;

    ESP_LOGI(TAG, "CMD_VEL接收任务已启动");

    while (1) {
        if (xQueueReceive(cmd_uart_queue, (void *)&event, portMAX_DELAY)) {
            if (event.type == UART_DATA) {
                // 读取UART数据
                uart_read_bytes(UART_CMD, &data, 1, portMAX_DELAY);

                if ((g_cmd_pt & 0x80) == 0) { // 数据未解析
                    if (g_cmd_pt > (LEN_CMD - 1)) {
                        // 缓冲区满，重新开始
                        g_cmd_pt = 0;
                    }

                    // 存入缓冲区
                    g_cmd_rx_buf[g_cmd_pt] = data;
                    g_cmd_pt++;

                    // 判断帧头
                    if ((g_cmd_pt == 1) && (data != 0xff)) {
                        g_cmd_pt--; // 回退，重新等待
                    } else if ((g_cmd_pt == 2) && (data != 0x2)) {
                        g_cmd_pt--; // 回退，重新等待
                    } else if (g_cmd_pt == 5) {
                        // 判断帧尾
                        if (data == 0x00) {
                            // 提取电机速度命令
                            motor_cmd.speed_left = (int8_t)g_cmd_rx_buf[2];
                            motor_cmd.speed_right = (int8_t)g_cmd_rx_buf[3];

                            // 发送到队列
                            if (xQueueSend(cmd_queue, &motor_cmd, 0) != pdPASS) {
                                ESP_LOGW(TAG, "CMD队列已满");
                            }

                            // 打印调试信息
                            ESP_LOGI(TAG, "CMD received: %d %d",
                                    motor_cmd.speed_left, motor_cmd.speed_right);

                            g_cmd_pt = 0; // 清零，等待下一帧数据的解析
                        } else {
                            g_cmd_pt = 0; // 数据错误，重新等待
                        }
                    }
                }
            }
        }
    }
}

/**
 * 电机控制任务
 * 接收来自SBUS和CMD_VEL的命令，控制电机
 */
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

            // 接收到CMD_VEL命令时，两组LED的绿色闪烁
            // 注意：共阳极LED，取反操作需要考虑逻辑（1变0，0变1）
            gpio_set_level(LED1_GREEN_PIN, !gpio_get_level(LED1_GREEN_PIN));
            gpio_set_level(LED2_GREEN_PIN, !gpio_get_level(LED2_GREEN_PIN));
        }
        // 检查是否有SBUS数据
        else if (xQueueReceive(sbus_queue, &sbus_data, 0) == pdPASS) {
            // 如果没有活跃的CMD_VEL命令或CMD_VEL已超时，则处理SBUS
            if (sbus_control || xTaskGetTickCount() > cmd_timeout) {
                parse_chan_val(sbus_data.channel);
                sbus_control = true;

                // 接收到SBUS命令时，两组LED的蓝色闪烁
                // 注意：共阳极LED，取反操作需要考虑逻辑（1变0，0变1）
                gpio_set_level(LED1_BLUE_PIN, !gpio_get_level(LED1_BLUE_PIN));
                gpio_set_level(LED2_BLUE_PIN, !gpio_get_level(LED2_BLUE_PIN));
            }
        }

        // 短暂延时，避免过度占用CPU
        vTaskDelay(pdMS_TO_TICKS(20));
    }
}

/**
 * 状态监控任务
 * 监控系统状态并更新LED指示
 */
static void status_monitor_task(void *pvParameters)
{
    ESP_LOGI(TAG, "状态监控任务已启动");
    uint8_t led_state = 0;

    while (1) {
        // 循环显示不同颜色，表示系统正常运行
        // 注意：共阳极LED，低电平(0)点亮，高电平(1)熄灭
        switch (led_state) {
            case 0: // 红色
                // LED1组
                gpio_set_level(LED1_RED_PIN, 0);  // 红色点亮
                gpio_set_level(LED1_GREEN_PIN, 1); // 绿色熄灭
                gpio_set_level(LED1_BLUE_PIN, 1);  // 蓝色熄灭
                // LED2组
                gpio_set_level(LED2_RED_PIN, 0);  // 红色点亮
                gpio_set_level(LED2_GREEN_PIN, 1); // 绿色熄灭
                gpio_set_level(LED2_BLUE_PIN, 1);  // 蓝色熄灭
                break;
            case 1: // 绿色
                // LED1组
                gpio_set_level(LED1_RED_PIN, 1);  // 红色熄灭
                gpio_set_level(LED1_GREEN_PIN, 0); // 绿色点亮
                gpio_set_level(LED1_BLUE_PIN, 1);  // 蓝色熄灭
                // LED2组
                gpio_set_level(LED2_RED_PIN, 1);  // 红色熄灭
                gpio_set_level(LED2_GREEN_PIN, 0); // 绿色点亮
                gpio_set_level(LED2_BLUE_PIN, 1);  // 蓝色熄灭
                break;
            case 2: // 蓝色
                // LED1组
                gpio_set_level(LED1_RED_PIN, 1);  // 红色熄灭
                gpio_set_level(LED1_GREEN_PIN, 1); // 绿色熄灭
                gpio_set_level(LED1_BLUE_PIN, 0);  // 蓝色点亮
                // LED2组
                gpio_set_level(LED2_RED_PIN, 1);  // 红色熄灭
                gpio_set_level(LED2_GREEN_PIN, 1); // 绿色熄灭
                gpio_set_level(LED2_BLUE_PIN, 0);  // 蓝色点亮
                break;
            case 3: // 全部关闭
                // LED1组
                gpio_set_level(LED1_RED_PIN, 1);  // 红色熄灭
                gpio_set_level(LED1_GREEN_PIN, 1); // 绿色熄灭
                gpio_set_level(LED1_BLUE_PIN, 1);  // 蓝色熄灭
                // LED2组
                gpio_set_level(LED2_RED_PIN, 1);  // 红色熄灭
                gpio_set_level(LED2_GREEN_PIN, 1); // 绿色熄灭
                gpio_set_level(LED2_BLUE_PIN, 1);  // 蓝色熄灭
                break;
        }

        // 更新状态
        led_state = (led_state + 1) % 4;

        // 延时500ms
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}

/**
 * 初始化GPIO
 */
static void gpio_init(void)
{
    // 配置LED引脚 - 两组共阳极RGB LED
    gpio_config_t io_conf = {};
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = (1ULL << LED1_RED_PIN) | (1ULL << LED1_GREEN_PIN) | (1ULL << LED1_BLUE_PIN) |
                          (1ULL << LED2_RED_PIN) | (1ULL << LED2_GREEN_PIN) | (1ULL << LED2_BLUE_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 0;
    gpio_config(&io_conf);

    // 配置按键引脚
    io_conf.intr_type = GPIO_INTR_POSEDGE;  // 上升沿触发中断
    io_conf.mode = GPIO_MODE_INPUT;
    io_conf.pin_bit_mask = (1ULL << KEY1_PIN) | (1ULL << KEY2_PIN);
    io_conf.pull_down_en = 0;
    io_conf.pull_up_en = 1;  // 启用内部上拉电阻
    gpio_config(&io_conf);

    // 设置LED初始状态 - 共阳极LED，高电平(1)熄灭，低电平(0)点亮
    // LED1组初始状态 - 全部熄灭
    gpio_set_level(LED1_RED_PIN, 1);
    gpio_set_level(LED1_GREEN_PIN, 1);
    gpio_set_level(LED1_BLUE_PIN, 1);

    // LED2组初始状态 - 全部熄灭
    gpio_set_level(LED2_RED_PIN, 1);
    gpio_set_level(LED2_GREEN_PIN, 1);
    gpio_set_level(LED2_BLUE_PIN, 1);
}

/**
 * 初始化UART
 */
static void uart_init(void)
{
    // 配置调试UART
    uart_config_t uart_config = {
        .baud_rate = 115200,
        .data_bits = UART_DATA_8_BITS,
        .parity = UART_PARITY_DISABLE,
        .stop_bits = UART_STOP_BITS_1,
        .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
        .source_clk = UART_SCLK_APB,
    };

    // 安装UART驱动
    ESP_ERROR_CHECK(uart_driver_install(UART_DEBUG, 256, 0, 0, NULL, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_DEBUG, &uart_config));

    // 配置CMD_VEL UART
    uart_config.baud_rate = 115200;
    ESP_ERROR_CHECK(uart_driver_install(UART_CMD, 256, 0, 20, &cmd_uart_queue, 0));
    ESP_ERROR_CHECK(uart_param_config(UART_CMD, &uart_config));
    // 注意：由于GPIO17现在用于CAN RX，我们需要使用其他引脚接收CMD_VEL信号
    // 这里使用GPIO21作为CMD_VEL接收引脚，请根据实际硬件连接调整
    ESP_ERROR_CHECK(uart_set_pin(UART_CMD, UART_PIN_NO_CHANGE, GPIO_NUM_21, UART_PIN_NO_CHANGE, UART_PIN_NO_CHANGE));

    // 创建CMD_VEL接收任务
    BaseType_t xReturned = xTaskCreate(
        cmd_uart_task,
        "cmd_uart_task",
        2048,
        NULL,
        12,
        &cmd_task_handle);

    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create CMD UART task");
    }
}

/**
 * 初始化定时器
 */
static void app_timer_init(void)
{
    // 创建左刹车定时器 (5秒超时)
    brake_timer_left = xTimerCreate("brake_timer_left", pdMS_TO_TICKS(5000), pdFALSE, 0, brake_timer_left_callback);
    if (brake_timer_left == NULL) {
        ESP_LOGE(TAG, "Failed to create left brake timer");
    }

    // 创建右刹车定时器 (5秒超时)
    brake_timer_right = xTimerCreate("brake_timer_right", pdMS_TO_TICKS(5000), pdFALSE, 0, brake_timer_right_callback);
    if (brake_timer_right == NULL) {
        ESP_LOGE(TAG, "Failed to create right brake timer");
    }

    // 启动定时器
    xTimerStart(brake_timer_left, 0);
    xTimerStart(brake_timer_right, 0);
}

void app_main(void)
{
    // 初始化GPIO
    gpio_init();

    // 初始化UART
    uart_init();

    // 初始化SBUS
    sbus_init();

    // 初始化电机驱动
    drv_keyadouble_init();

    // 初始化定时器
    app_timer_init();

    ESP_LOGI(TAG, "System initialized");

    // 创建FreeRTOS队列
    sbus_queue = xQueueCreate(5, sizeof(sbus_data_t));
    cmd_queue = xQueueCreate(5, sizeof(motor_cmd_t));

    if (sbus_queue == NULL || cmd_queue == NULL) {
        ESP_LOGE(TAG, "Failed to create queues");
        return;
    }

    // 创建FreeRTOS任务
    BaseType_t xReturned;

    // SBUS处理任务 - 高优先级
    xReturned = xTaskCreate(
        sbus_process_task,
        "sbus_task",
        4096,
        NULL,
        12,  // 高优先级
        &sbus_task_handle);
    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create SBUS task");
    }

    // CMD_VEL处理任务已在UART初始化中创建

    // 电机控制任务 - 中优先级
    xReturned = xTaskCreate(
        motor_control_task,
        "motor_task",
        4096,
        NULL,
        10,  // 中优先级
        &control_task_handle);
    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create motor control task");
    }

    // 状态监控任务 - 低优先级
    xReturned = xTaskCreate(
        status_monitor_task,
        "status_task",
        2048,
        NULL,
        5,   // 低优先级
        &status_task_handle);
    if (xReturned != pdPASS) {
        ESP_LOGE(TAG, "Failed to create status monitor task");
    }

    ESP_LOGI(TAG, "All FreeRTOS tasks created");
}
