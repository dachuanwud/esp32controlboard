# 🔧 GPIO模块详解

本文档详细介绍ESP32控制板项目中GPIO模块的实现，包括引脚配置、LED控制、按键输入、中断处理和扩展接口设计。

## 🎯 GPIO模块概述

ESP32控制板使用GPIO模块实现LED状态指示、按键输入检测、扩展接口控制等功能。GPIO模块采用统一的配置和管理方式，确保各功能模块的协调工作。

## 📍 GPIO引脚分配

### 当前使用的GPIO

<augment_code_snippet path="main/main.h" mode="EXCERPT">
````c
// 定义GPIO引脚
// LED指示灯引脚 - 共阳极RGB LED
// LED1组
#define LED1_RED_PIN            GPIO_NUM_12  // LED1红色引脚
#define LED1_GREEN_PIN          GPIO_NUM_13  // LED1绿色引脚
#define LED1_BLUE_PIN           GPIO_NUM_14  // LED1蓝色引脚

// LED2组
#define LED2_RED_PIN            GPIO_NUM_25  // LED2红色引脚
#define LED2_GREEN_PIN          GPIO_NUM_26  // LED2绿色引脚
#define LED2_BLUE_PIN           GPIO_NUM_27  // LED2蓝色引脚

// 按键引脚
#define KEY1_PIN                GPIO_NUM_0   // 按键1
#define KEY2_PIN                GPIO_NUM_35  // 按键2
````
</augment_code_snippet>

### 通信接口GPIO

| GPIO | 功能 | 方向 | 协议 | 说明 |
|------|------|------|------|------|
| GPIO16 | CAN_TX | OUTPUT | TWAI | CAN总线发送 |
| GPIO17 | CAN_RX | INPUT | TWAI | CAN总线接收 |
| GPIO21 | CMD_RX | INPUT | UART1 | 命令接收 |
| GPIO22 | SBUS_RX | INPUT | UART2 | SBUS数据接收 |

## 💡 LED控制模块

### 1. LED硬件配置

<augment_code_snippet path="main/main.c" mode="EXCERPT">
````c
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
````
</augment_code_snippet>

### 2. LED控制接口

```c
// LED颜色定义
typedef enum {
    LED_COLOR_OFF = 0,      // 熄灭
    LED_COLOR_RED,          // 红色
    LED_COLOR_GREEN,        // 绿色
    LED_COLOR_BLUE,         // 蓝色
    LED_COLOR_YELLOW,       // 黄色 (红+绿)
    LED_COLOR_CYAN,         // 青色 (绿+蓝)
    LED_COLOR_MAGENTA,      // 紫色 (红+蓝)
    LED_COLOR_WHITE         // 白色 (红+绿+蓝)
} led_color_t;

// LED组定义
typedef enum {
    LED_GROUP_1 = 0,        // LED1组
    LED_GROUP_2,            // LED2组
    LED_GROUP_ALL           // 所有LED
} led_group_t;
```

### 3. LED控制函数

```c
/**
 * 设置LED颜色
 * @param group LED组选择
 * @param color 颜色选择
 */
void led_set_color(led_group_t group, led_color_t color)
{
    gpio_num_t red_pin, green_pin, blue_pin;
    
    // 选择LED组
    if (group == LED_GROUP_1 || group == LED_GROUP_ALL) {
        red_pin = LED1_RED_PIN;
        green_pin = LED1_GREEN_PIN;
        blue_pin = LED1_BLUE_PIN;
        
        // 共阳极LED，低电平点亮
        gpio_set_level(red_pin, !(color & 0x01));
        gpio_set_level(green_pin, !(color & 0x02));
        gpio_set_level(blue_pin, !(color & 0x04));
    }
    
    if (group == LED_GROUP_2 || group == LED_GROUP_ALL) {
        red_pin = LED2_RED_PIN;
        green_pin = LED2_GREEN_PIN;
        blue_pin = LED2_BLUE_PIN;
        
        // 共阳极LED，低电平点亮
        gpio_set_level(red_pin, !(color & 0x01));
        gpio_set_level(green_pin, !(color & 0x02));
        gpio_set_level(blue_pin, !(color & 0x04));
    }
}

/**
 * LED闪烁控制
 * @param group LED组选择
 * @param color 闪烁颜色
 * @param period 闪烁周期(ms)
 */
void led_blink(led_group_t group, led_color_t color, uint32_t period)
{
    static uint32_t last_time = 0;
    static bool led_state = false;
    
    uint32_t current_time = esp_timer_get_time() / 1000;
    
    if (current_time - last_time >= period / 2) {
        led_state = !led_state;
        led_set_color(group, led_state ? color : LED_COLOR_OFF);
        last_time = current_time;
    }
}
```

### 4. LED状态指示

```c
// 系统状态LED指示
typedef enum {
    SYSTEM_STATE_INIT,      // 初始化 - 红色
    SYSTEM_STATE_READY,     // 就绪 - 绿色
    SYSTEM_STATE_RUNNING,   // 运行 - 蓝色
    SYSTEM_STATE_ERROR,     // 错误 - 红色闪烁
    SYSTEM_STATE_WARNING    // 警告 - 黄色闪烁
} system_state_t;

void led_show_system_state(system_state_t state)
{
    switch (state) {
        case SYSTEM_STATE_INIT:
            led_set_color(LED_GROUP_1, LED_COLOR_RED);
            break;
        case SYSTEM_STATE_READY:
            led_set_color(LED_GROUP_1, LED_COLOR_GREEN);
            break;
        case SYSTEM_STATE_RUNNING:
            led_set_color(LED_GROUP_1, LED_COLOR_BLUE);
            break;
        case SYSTEM_STATE_ERROR:
            led_blink(LED_GROUP_1, LED_COLOR_RED, 500);
            break;
        case SYSTEM_STATE_WARNING:
            led_blink(LED_GROUP_1, LED_COLOR_YELLOW, 1000);
            break;
    }
}
```

## 🔘 按键输入模块

### 1. 按键硬件配置

<augment_code_snippet path="main/main.c" mode="EXCERPT">
````c
// 配置按键引脚
io_conf.intr_type = GPIO_INTR_POSEDGE;  // 上升沿触发中断
io_conf.mode = GPIO_MODE_INPUT;
io_conf.pin_bit_mask = (1ULL << KEY1_PIN) | (1ULL << KEY2_PIN);
io_conf.pull_down_en = 0;
io_conf.pull_up_en = 1;  // 启用内部上拉电阻
gpio_config(&io_conf);
````
</augment_code_snippet>

### 2. 按键中断处理

```c
// 按键状态定义
typedef enum {
    KEY_STATE_RELEASED = 0,
    KEY_STATE_PRESSED,
    KEY_STATE_LONG_PRESSED
} key_state_t;

// 按键事件定义
typedef enum {
    KEY_EVENT_NONE = 0,
    KEY_EVENT_PRESS,        // 按下
    KEY_EVENT_RELEASE,      // 释放
    KEY_EVENT_CLICK,        // 单击
    KEY_EVENT_LONG_PRESS    // 长按
} key_event_t;

// 按键状态结构
typedef struct {
    gpio_num_t pin;
    key_state_t state;
    uint32_t press_time;
    uint32_t release_time;
    bool debounce_flag;
} key_info_t;

static key_info_t key_info[2] = {
    {KEY1_PIN, KEY_STATE_RELEASED, 0, 0, false},
    {KEY2_PIN, KEY_STATE_RELEASED, 0, 0, false}
};

/**
 * GPIO中断服务程序
 */
static void IRAM_ATTR gpio_isr_handler(void* arg)
{
    uint32_t gpio_num = (uint32_t) arg;
    
    // 发送中断事件到队列
    BaseType_t xHigherPriorityTaskWoken = pdFALSE;
    xQueueSendFromISR(gpio_evt_queue, &gpio_num, &xHigherPriorityTaskWoken);
    
    if (xHigherPriorityTaskWoken) {
        portYIELD_FROM_ISR();
    }
}
```

### 3. 按键防抖处理

```c
#define KEY_DEBOUNCE_TIME_MS    50      // 防抖时间
#define KEY_LONG_PRESS_TIME_MS  1000    // 长按时间

/**
 * 按键扫描任务
 */
static void key_scan_task(void *pvParameters)
{
    uint32_t gpio_num;
    
    while (1) {
        if (xQueueReceive(gpio_evt_queue, &gpio_num, portMAX_DELAY)) {
            // 确定按键索引
            int key_index = -1;
            if (gpio_num == KEY1_PIN) key_index = 0;
            else if (gpio_num == KEY2_PIN) key_index = 1;
            
            if (key_index >= 0) {
                process_key_event(key_index);
            }
        }
    }
}

/**
 * 处理按键事件
 */
static void process_key_event(int key_index)
{
    key_info_t *key = &key_info[key_index];
    uint32_t current_time = esp_timer_get_time() / 1000;
    int level = gpio_get_level(key->pin);
    
    // 防抖处理
    if (!key->debounce_flag) {
        key->debounce_flag = true;
        vTaskDelay(pdMS_TO_TICKS(KEY_DEBOUNCE_TIME_MS));
        
        // 重新读取电平
        level = gpio_get_level(key->pin);
        key->debounce_flag = false;
        
        if (level == 0) {  // 按键按下 (低电平)
            if (key->state == KEY_STATE_RELEASED) {
                key->state = KEY_STATE_PRESSED;
                key->press_time = current_time;
                handle_key_event(key_index, KEY_EVENT_PRESS);
            }
        } else {  // 按键释放 (高电平)
            if (key->state == KEY_STATE_PRESSED || key->state == KEY_STATE_LONG_PRESSED) {
                key->release_time = current_time;
                uint32_t press_duration = key->release_time - key->press_time;
                
                if (press_duration >= KEY_LONG_PRESS_TIME_MS) {
                    handle_key_event(key_index, KEY_EVENT_LONG_PRESS);
                } else {
                    handle_key_event(key_index, KEY_EVENT_CLICK);
                }
                
                key->state = KEY_STATE_RELEASED;
                handle_key_event(key_index, KEY_EVENT_RELEASE);
            }
        }
    }
}
```

### 4. 按键事件处理

```c
/**
 * 按键事件处理函数
 */
static void handle_key_event(int key_index, key_event_t event)
{
    ESP_LOGI(TAG, "🔘 Key%d Event: %d", key_index + 1, event);
    
    switch (key_index) {
        case 0:  // KEY1
            switch (event) {
                case KEY_EVENT_CLICK:
                    ESP_LOGI(TAG, "KEY1 单击 - 切换LED1状态");
                    toggle_led1_state();
                    break;
                case KEY_EVENT_LONG_PRESS:
                    ESP_LOGI(TAG, "KEY1 长按 - 系统复位");
                    esp_restart();
                    break;
                default:
                    break;
            }
            break;
            
        case 1:  // KEY2
            switch (event) {
                case KEY_EVENT_CLICK:
                    ESP_LOGI(TAG, "KEY2 单击 - 切换LED2状态");
                    toggle_led2_state();
                    break;
                case KEY_EVENT_LONG_PRESS:
                    ESP_LOGI(TAG, "KEY2 长按 - 进入配置模式");
                    enter_config_mode();
                    break;
                default:
                    break;
            }
            break;
    }
}
```

## 🔌 扩展接口模块

### 1. I2C接口配置

```c
#define I2C_MASTER_SCL_IO       GPIO_NUM_5      // I2C时钟线
#define I2C_MASTER_SDA_IO       GPIO_NUM_4      // I2C数据线
#define I2C_MASTER_NUM          I2C_NUM_0       // I2C端口号
#define I2C_MASTER_FREQ_HZ      100000          // I2C频率

/**
 * 初始化I2C接口
 */
esp_err_t i2c_master_init(void)
{
    i2c_config_t conf = {
        .mode = I2C_MODE_MASTER,
        .sda_io_num = I2C_MASTER_SDA_IO,
        .sda_pullup_en = GPIO_PULLUP_ENABLE,
        .scl_io_num = I2C_MASTER_SCL_IO,
        .scl_pullup_en = GPIO_PULLUP_ENABLE,
        .master.clk_speed = I2C_MASTER_FREQ_HZ,
    };
    
    esp_err_t err = i2c_param_config(I2C_MASTER_NUM, &conf);
    if (err != ESP_OK) {
        return err;
    }
    
    return i2c_driver_install(I2C_MASTER_NUM, conf.mode, 0, 0, 0);
}
```

### 2. SPI接口配置

```c
#define SPI_CLK_PIN             GPIO_NUM_18     // SPI时钟
#define SPI_MISO_PIN            GPIO_NUM_19     // SPI主入从出
#define SPI_MOSI_PIN            GPIO_NUM_23     // SPI主出从入
#define SPI_CS_PIN              GPIO_NUM_5      // SPI片选

/**
 * 初始化SPI接口
 */
esp_err_t spi_master_init(void)
{
    spi_bus_config_t buscfg = {
        .miso_io_num = SPI_MISO_PIN,
        .mosi_io_num = SPI_MOSI_PIN,
        .sclk_io_num = SPI_CLK_PIN,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = 4096,
    };
    
    return spi_bus_initialize(VSPI_HOST, &buscfg, 1);
}
```

### 3. ADC模拟输入

```c
#define ADC_CHANNEL_0           ADC1_CHANNEL_0  // GPIO36
#define ADC_CHANNEL_3           ADC1_CHANNEL_3  // GPIO39
#define ADC_CHANNEL_4           ADC1_CHANNEL_4  // GPIO32
#define ADC_CHANNEL_5           ADC1_CHANNEL_5  // GPIO33

/**
 * 初始化ADC
 */
esp_err_t adc_init(void)
{
    // 配置ADC1
    adc1_config_width(ADC_WIDTH_BIT_12);
    adc1_config_channel_atten(ADC_CHANNEL_0, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC_CHANNEL_3, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC_CHANNEL_4, ADC_ATTEN_DB_11);
    adc1_config_channel_atten(ADC_CHANNEL_5, ADC_ATTEN_DB_11);
    
    return ESP_OK;
}

/**
 * 读取ADC值
 */
uint32_t adc_read_voltage(adc1_channel_t channel)
{
    int raw_value = adc1_get_raw(channel);
    // 转换为电压值 (mV)
    return esp_adc_cal_raw_to_voltage(raw_value, &adc_chars);
}
```

## 📊 GPIO性能特性

### 1. 电气特性

| 参数 | 最小值 | 典型值 | 最大值 | 单位 |
|------|--------|--------|--------|------|
| 输出电流 | - | - | 40 | mA |
| 输入阻抗 | 1 | - | - | MΩ |
| 上拉电阻 | 30 | 45 | 100 | kΩ |
| 下拉电阻 | 30 | 45 | 100 | kΩ |
| 切换频率 | - | - | 40 | MHz |

### 2. 中断性能

| 指标 | 数值 | 说明 |
|------|------|------|
| 中断延迟 | < 10μs | 硬件到软件 |
| 防抖时间 | 50ms | 软件防抖 |
| 最大中断频率 | 1kHz | 实际应用 |
| 中断优先级 | 可配置 | 0-7级 |

## 🛠️ 调试和测试

### 1. GPIO状态监控

```c
/**
 * GPIO状态监控任务
 */
static void gpio_monitor_task(void *pvParameters)
{
    while (1) {
        ESP_LOGI(TAG, "📊 GPIO Status:");
        ESP_LOGI(TAG, "   KEY1: %d, KEY2: %d", 
                 gpio_get_level(KEY1_PIN), gpio_get_level(KEY2_PIN));
        ESP_LOGI(TAG, "   LED1_R: %d, LED1_G: %d, LED1_B: %d",
                 gpio_get_level(LED1_RED_PIN), 
                 gpio_get_level(LED1_GREEN_PIN),
                 gpio_get_level(LED1_BLUE_PIN));
        
        vTaskDelay(pdMS_TO_TICKS(5000));
    }
}
```

### 2. 按键测试

```c
/**
 * 按键功能测试
 */
void test_key_functions(void)
{
    ESP_LOGI(TAG, "🔘 Key Test Started");
    ESP_LOGI(TAG, "   Press KEY1 for LED1 control");
    ESP_LOGI(TAG, "   Press KEY2 for LED2 control");
    ESP_LOGI(TAG, "   Long press for special functions");
}
```

### 3. LED测试

```c
/**
 * LED功能测试
 */
void test_led_functions(void)
{
    ESP_LOGI(TAG, "💡 LED Test Started");
    
    // 测试所有颜色
    led_color_t colors[] = {
        LED_COLOR_RED, LED_COLOR_GREEN, LED_COLOR_BLUE,
        LED_COLOR_YELLOW, LED_COLOR_CYAN, LED_COLOR_MAGENTA,
        LED_COLOR_WHITE, LED_COLOR_OFF
    };
    
    for (int i = 0; i < 8; i++) {
        led_set_color(LED_GROUP_ALL, colors[i]);
        vTaskDelay(pdMS_TO_TICKS(500));
    }
}
```

## 🚨 故障排除

### 1. 常见问题

#### LED不亮
**检查项目**:
1. GPIO配置是否正确
2. 共阳极接线是否正确
3. 限流电阻是否合适
4. 电源供电是否正常

#### 按键无响应
**检查项目**:
1. 上拉电阻是否启用
2. 中断配置是否正确
3. 防抖时间是否合适
4. 按键硬件是否正常

### 2. 调试技巧

```c
// GPIO调试宏
#define GPIO_DEBUG_TOGGLE(pin) do { \
    static int level = 0; \
    level = !level; \
    gpio_set_level(pin, level); \
} while(0)

// 在关键位置添加调试信号
void debug_signal_output(void)
{
    GPIO_DEBUG_TOGGLE(GPIO_NUM_2);  // 使用内置LED作为调试信号
}
```

---

💡 **提示**: GPIO模块是硬件接口的基础，正确的配置和使用是系统稳定运行的关键！
