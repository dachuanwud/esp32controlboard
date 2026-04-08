#ifndef CHANNEL_PARSE_H
#define CHANNEL_PARSE_H

#include <stdint.h>

/**
 * 解析通道值并控制电机运动
 * @param ch_val 通道值数组
 * @return 0=成功
 */
uint8_t parse_chan_val(uint16_t* ch_val);

/**
 * 解析cmd_vel命令并控制电机运动
 * @param spl 左电机速度
 * @param spr 右电机速度
 * @return 0=成功
 */
uint8_t parse_cmd_vel(uint8_t spl, uint8_t spr);

/**
 * 强制下发停止命令
 * 用于遥控失联、上层超时保护等场景
 * @param reason 停止原因，可为NULL
 */
void channel_parse_force_stop(const char *reason);

#endif /* CHANNEL_PARSE_H */
