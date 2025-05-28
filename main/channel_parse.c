#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "channel_parse.h"
#include "drv_keyadouble.h"
#include "esp_log.h"

static const char *TAG = "CHAN_PARSE";

// 函数指针，指向实际的电机控制函数
static uint8_t (*intf_move)(int8_t, int8_t) = intf_move_keyadouble;

// 保存上一次的通道值，用于变化检测
// 初始化为0，表示未接收到有效数据
static uint16_t last_ch_val[16] = {0};
static bool first_run = true;

// 通道变化阈值 - 避免微小抖动触发发送
#define CHANNEL_THRESHOLD 10  // 通道值变化超过10才认为是有效变化

/**
 * 将通道值转换为速度值
 * 标准SBUS协议：输入范围1050~1950，中位值1500，映射到-100~100
 * 优化算法：直接映射，无需范围限制
 * @param val 通道值(1050~1950)
 * @return 速度值(-100~100)
 */
static int8_t chg_val(uint16_t val)
{
    // 优化的映射算法：(val-1500)/9*2，范围900/9*2=200，即-100到+100
    int8_t sp = (((int16_t)val - 1500) / 9 * 2) & 0xff;
    return sp;
}

/**
 * 检查关键通道是否有变化
 * @param ch_val 当前通道值数组
 * @return true=有变化，false=无变化
 */
static bool check_channel_changed(uint16_t* ch_val)
{
    // 检查关键控制通道：0(左右), 2(前后), 3(备用左右), 6(模式), 7(速度减半)
    uint8_t key_channels[] = {0, 2, 3, 6, 7};
    bool changed = false;

    for (int i = 0; i < 5; i++) {
        uint8_t ch = key_channels[i];
        // 如果是第一次运行，last_ch_val[ch]为0，不显示变化信息
        if (last_ch_val[ch] != 0 && abs((int16_t)ch_val[ch] - (int16_t)last_ch_val[ch]) > CHANNEL_THRESHOLD) {
            ESP_LOGI(TAG, "📈 Channel %d changed: %d → %d (diff: %d)",
                     ch, last_ch_val[ch], ch_val[ch],
                     abs((int16_t)ch_val[ch] - (int16_t)last_ch_val[ch]));
            changed = true;
        } else if (last_ch_val[ch] == 0) {
            // 第一次接收到数据，标记为有变化但不显示变化信息
            changed = true;
        }
    }

    return changed;
}

/**
 * 更新保存的通道值
 * @param ch_val 当前通道值数组
 */
static void update_last_channels(uint16_t* ch_val)
{
    for (int i = 0; i < 16; i++) {
        last_ch_val[i] = ch_val[i];
    }
}

/**
 * 计算差速转弯的速度偏移
 * 履带车差速控制：当转弯时，内侧履带速度减小，外侧履带保持原速
 * @param v1 主速度分量（前后）
 * @param v2 转向速度分量（左右）
 * @return 偏移后的速度值
 */
static int8_t cal_offset(int8_t v1, int8_t v2)
{
    if (abs(v1) < abs(v2)) {
        return 0;
    }

    // 带上v1的符号，确保转弯时速度方向正确
    if (v1 > 0) {
        return abs(v1) - abs(v2);
    } else {
        return abs(v2) - abs(v1);
    }
}

/**
 * 解析通道值并控制履带车运动
 * 标准SBUS协议：1050~1950映射到-100~100，1500对应0
 * 履带车差速控制：通过左右履带速度差实现转弯
 *
 * 通道分配：
 * - 通道0 (ch_val[0]): 左右方向控制，右>0
 * - 通道2 (ch_val[2]): 前后方向控制，前>0
 * - 通道3 (ch_val[3]): 备用左右方向控制（单手模式）
 * - 通道6 (ch_val[6]): 单手模式开关，1950时启用
 * - 通道7 (ch_val[7]): 低速模式开关，1950时启用
 */
uint8_t parse_chan_val(uint16_t* ch_val)
{
    // 检查关键通道是否有变化
    bool channels_changed = check_channel_changed(ch_val);

    // 如果是第一次运行或通道值有变化，才执行控制逻辑
    if (first_run || channels_changed) {
        if (first_run) {
            ESP_LOGI(TAG, "🚀 First run - initializing track vehicle control");
            first_run = false;
        } else {
            ESP_LOGI(TAG, "🎮 Channel values changed - updating track control");
        }

        int8_t sp_fb = chg_val(ch_val[2]); // 前后分量，向前>0
        int8_t sp_lr = chg_val(ch_val[0]); // 左右分量，向右>0

        if (ch_val[6] == 1950) {
            // 启动单手模式，仅用左边拨杆控制
            sp_lr = chg_val(ch_val[3]); // 左右分量，向右>0
            ESP_LOGI(TAG, "�️ Single-hand mode activated");
        }

        if (ch_val[7] == 1950) {
            // 启动低速模式，速度减半
            sp_fb /= 2;
            sp_lr /= 2;
            ESP_LOGI(TAG, "🐌 Low speed mode activated");
        }

        ESP_LOGI(TAG, "🎯 Control values - FB:%d LR:%d", sp_fb, sp_lr);

        // 履带车差速控制逻辑
        if (sp_fb == 0) {
            if (sp_lr == 0) {
                // 停止
                ESP_LOGI(TAG, "⏹️ STOP");
                intf_move(0, 0);
            } else {
                // 原地转向
                ESP_LOGI(TAG, "🔄 TURN IN PLACE - LR:%d", sp_lr);
                intf_move(sp_lr, (-1) * sp_lr);
            }
        } else {
            if (sp_lr == 0) {
                // 前进或后退
                ESP_LOGI(TAG, "%s STRAIGHT - Speed:%d", sp_fb > 0 ? "⬆️ FORWARD" : "⬇️ BACKWARD", sp_fb);
                intf_move(sp_fb, sp_fb);
            } else if (sp_lr > 0) {
                // 差速右转
                int8_t right_speed = cal_offset(sp_fb, sp_lr);
                ESP_LOGI(TAG, "↗️ DIFFERENTIAL RIGHT - Left:%d Right:%d", sp_fb, right_speed);
                intf_move(sp_fb, right_speed);
            } else {
                // 差速左转
                int8_t left_speed = cal_offset(sp_fb, sp_lr);
                ESP_LOGI(TAG, "↖️ DIFFERENTIAL LEFT - Left:%d Right:%d", left_speed, sp_fb);
                intf_move(left_speed, sp_fb);
            }
        }

        // 更新保存的通道值
        update_last_channels(ch_val);
    } else {
        ESP_LOGD(TAG, "📊 No significant channel changes - skipping CAN transmission");
    }

    return 0;
}

/**
 * 解析cmd_vel命令并控制电机运动
 */
uint8_t parse_cmd_vel(uint8_t spl, uint8_t spr)
{
    intf_move((int8_t)spl, (int8_t)spr);
    return 0;
}
