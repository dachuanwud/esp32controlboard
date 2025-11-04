#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include "channel_parse.h"
#include "drv_keyadouble.h"
#include "esp_log.h"
#include <inttypes.h>

static const char *TAG = "CHAN_PARSE";

// 函数指针，指向实际的电机控制函数
static uint8_t (*intf_move)(int8_t, int8_t) = intf_move_keyadouble;

// 保存上一次的通道值，用于变化检测
// 初始化为0，表示未接收到有效数据
static uint16_t last_ch_val[16] = {0};
static bool first_run = true;

// ⚡ 性能优化：减小通道变化阈值，提高控制精度和响应速度
// 阈值从10降到5，在保持抗抖动能力的同时，提供更细腻的控制体验
#define CHANNEL_THRESHOLD 5  // 通道值变化超过5才认为是有效变化

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
            ESP_LOGD(TAG, "📈 Channel %d changed: %d → %d (diff: %d)",
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
    // ⚡ 性能优化：始终执行控制逻辑，确保实时响应
    // 移除变化检测的限制，让CAN总线始终发送最新的控制命令
    // 这样可以确保即使微小的控制变化也能立即响应

    // 检查关键通道是否有变化（仅用于日志输出控制）
    bool channels_changed = check_channel_changed(ch_val);

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

/**
 * 解析cmd_vel命令并控制电机运动
 */
uint8_t parse_cmd_vel(uint8_t spl, uint8_t spr)
{
    intf_move((int8_t)spl, (int8_t)spr);
    return 0;
}
