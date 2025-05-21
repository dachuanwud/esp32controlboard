#include <stdio.h>
#include <stdlib.h>
#include "channel_parse.h"
#include "drv_keyadouble.h"

// 函数指针，指向实际的电机控制函数
static uint8_t (*intf_move)(int8_t, int8_t) = intf_move_keyadouble;

/**
 * 将通道值转换为速度值
 * @param val 通道值(1050~1950)
 * @return 速度值(-100~100)
 */
static int8_t chg_val(uint16_t val)
{
    int8_t sp = (((int16_t)val - 1500) / 9 * 2) & 0xff;
    return sp;
}

/**
 * 计算速度偏移
 * @param v1 速度1
 * @param v2 速度2
 * @return 偏移值
 */
static int8_t cal_offset(int8_t v1, int8_t v2)
{
    if (abs(v1) < abs(v2)) {
        return 0;
    }
    
    // 保持v1的符号
    if (v1 > 0) {
        return abs(v1) - abs(v2);
    } else {
        return abs(v2) - abs(v1);
    }
}

/**
 * 解析通道值并控制电机运动
 * 1050~1950映射到-100~100上，1500对应0
 */
uint8_t parse_chan_val(uint16_t* ch_val)
{
    int8_t sp_fb = chg_val(ch_val[2]); // 前后方向，向前>0
    int8_t sp_lr = chg_val(ch_val[0]); // 左右方向，向右>0
    
    if (ch_val[6] == 1950) {
        // 差速控制模式，左右轮不同控制
        sp_lr = chg_val(ch_val[3]); // 左右方向，向右>0
    }
    
    if (ch_val[7] == 1950) {
        // 低速控制模式，速度减半
        sp_fb /= 2;
        sp_lr /= 2;
    }
    
    if (sp_fb == 0) {
        if (sp_lr == 0) {
            // 停止
            intf_move(0, 0);
        } else {
            // 原地转弯
            intf_move(sp_lr, (-1) * sp_lr);
        }
    } else {
        if (sp_lr == 0) {
            // 前进或后退
            intf_move(sp_fb, sp_fb);
        } else if (sp_lr > 0) {
            // 右转弯
            intf_move(sp_fb, cal_offset(sp_fb, sp_lr));
        } else {
            // 左转弯
            intf_move(cal_offset(sp_fb, sp_lr), sp_fb);
        }
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
