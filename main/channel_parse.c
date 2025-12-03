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

/**
 * 将通道值转换为速度值
 * 标准SBUS协议：输入范围1050~1950，中位值1500，映射到-100~100
 * 🔧 优化算法：使用更精确的计算方法，减少舍入误差
 * @param val 通道值(1050~1950)
 * @return 速度值(-100~100)
 */
static int8_t chg_val(uint16_t val)
{
    // 🔧 改进的映射算法：使用四舍五入提高精度
    // 公式：(val - 1500) * 2 / 9，使用四舍五入减少误差
    // 范围：900 * 2 / 9 = 200，即-100到+100
    int16_t diff = (int16_t)val - 1500;
    // 使用四舍五入：先乘以2，加上4.5的等价操作（加9/2），再除以9
    int16_t sp = (diff * 2 + (diff >= 0 ? 4 : -4)) / 9;
    
    // 限制在有效范围内
    if (sp > 100) sp = 100;
    if (sp < -100) sp = -100;
    
    return (int8_t)sp;
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
 * - 通道4 (ch_val[4]): 遥控使能开关，1050=使能，1500/1950=禁用
 * - 通道6 (ch_val[6]): 单手模式开关，1950时启用
 * - 通道7 (ch_val[7]): 低速模式开关，1950时启用
 */
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
        static bool last_remote_enabled = false;
        static bool last_single_hand_mode = false;
        static bool last_low_speed_mode = false;
        
        // CH4 遥控使能开关：1050=使能，1500/1950=禁用
        bool current_remote_enabled = (ch_val[4] == 1050);
        // CH6 单手模式开关：1950时启用
        bool current_single_hand = (ch_val[6] == 1950);
        // CH7 低速模式开关：1950时启用
        bool current_low_speed = (ch_val[7] == 1950);

        if (current_remote_enabled != last_remote_enabled) {
            ESP_LOGI(TAG, "🔒 Remote control: %s (CH4=%d)", 
                     current_remote_enabled ? "ENABLED" : "DISABLED", ch_val[4]);
            last_remote_enabled = current_remote_enabled;
        }

        if (current_single_hand != last_single_hand_mode) {
            ESP_LOGI(TAG, "🤟 Single-hand mode: %s", current_single_hand ? "ON" : "OFF");
            last_single_hand_mode = current_single_hand;
        }

        if (current_low_speed != last_low_speed_mode) {
            ESP_LOGI(TAG, "🐌 Low speed mode: %s", current_low_speed ? "ON" : "OFF");
            last_low_speed_mode = current_low_speed;
        }

        // 🔒 如果遥控未使能，直接返回，不发送任何 CAN 信号
        if (!current_remote_enabled) {
            return 0;
        }

        if (current_single_hand) {
            sp_lr = chg_val(ch_val[3]); // 左右分量，向右>0
        }

        if (current_low_speed) {
            // 🔧 低速档：速度范围限制为 -20 ~ +20 (缩放比例 0.2)
            // 正常速度 -100 ~ +100 → 低速档 -20 ~ +20
            sp_fb = (sp_fb * 20) / 100;
            sp_lr = (sp_lr * 20) / 100;
        }

        ESP_LOGD(TAG, "🎯 Control values - FB:%d LR:%d", sp_fb, sp_lr);

        // 🔧 添加死区处理：对于微小的左右偏差，强制设为0，避免前进/后退时的偏差
        // 死区阈值：±3（推荐值），如果左右分量在死区内，视为0
        // 对应通道值范围：1500 ± 13.5 ≈ 1486 ~ 1514
        // 这个范围可以消除：
        //   - 高质量遥控器的中位波动（±3~5通道值）
        //   - SBUS数据噪声（±1~2通道值）
        //   - 同时不会影响正常的转向操作（通常需要 ±10 以上的通道值变化）
        // 
        // 调整建议：
        //   - 如果遥控器质量很好，可以设置为 ±2
        //   - 如果遥控器质量一般，建议设置为 ±3 或 ±4
        //   - 如果遥控器质量较差，可以设置为 ±5
        #define LR_DEADZONE 3
        if (abs(sp_lr) <= LR_DEADZONE) {
            sp_lr = 0;
        }

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
                // 🔧 前进或后退：强制使用完全相同的速度值，确保两个电机完全同步
                // 使用同一个变量赋值，避免任何可能的计算偏差
                left_speed = sp_fb;
                right_speed = sp_fb;  // 直接使用相同的值，确保完全一致
                
                // 🔧 双重保险：再次确保两个速度值完全一致
                if (left_speed != right_speed) {
                    // 如果由于某种原因不一致，使用平均值
                    int8_t avg_speed = (left_speed + right_speed) / 2;
                    left_speed = avg_speed;
                    right_speed = avg_speed;
                    ESP_LOGW(TAG, "⚠️ Speed sync correction applied: %d", avg_speed);
                }
                
                if (abs(left_speed - last_left_speed) > SPEED_LOG_THRESHOLD || abs(right_speed - last_right_speed) > SPEED_LOG_THRESHOLD) {
                    ESP_LOGI(TAG, "%s STRAIGHT - Speed:%d (L:%d R:%d)", 
                             sp_fb > 0 ? "⬆️ FORWARD" : "⬇️ BACKWARD", 
                             sp_fb, left_speed, right_speed);
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

        // 执行电机控制并发送CAN消息（包括速度为0的停止命令）
        // 注意：必须始终发送命令，否则从运动到停止时电机无法收到停止指令
        intf_move(left_speed, right_speed);

        // 更新上次速度值
        last_left_speed = left_speed;
        last_right_speed = right_speed;

        // 更新保存的通道值（用于变化检测和日志输出）
        update_last_channels(ch_val);
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
