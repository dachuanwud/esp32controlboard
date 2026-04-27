#ifndef DRV_PAYOUT_H
#define DRV_PAYOUT_H

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * 放线设备驱动（RS485 / Modbus RTU）
 *
 * 移植自 esp32controlboard_fangxianqi_esp32 工程：
 *   - 透过 UART1 外接 RS485 收发器
 *   - Modbus RTU 功能码 0x06（写单寄存器）
 *   - 写入目标寄存器 PAYOUT_MODBUS_REG（默认 0x0042），值 = int16 PWM
 *   - PWM 换算：SBUS 通道 1050~1950 → -1000 ~ +1000
 *
 * 与履带车电机驱动解耦：共用 SBUS 输入，独占 UART1，不影响 CAN 总线。
 */

/**
 * 初始化 UART1 + RS485 + DE 引脚（如有）
 */
esp_err_t drv_payout_init(void);

/**
 * 按 SBUS 原始通道值下发放线速度。
 * 内部做：中位死区夹紧 → 按 PAYOUT_PWM_SCALE 换算 → Modbus CRC → UART 发送。
 *
 * @param channel_value SBUS 通道原始值（1050 ~ 1950，1500=停）
 */
void drv_payout_send_channel_pwm(uint16_t channel_value);

/**
 * 立即下发"停止"命令（中位 1500）。
 * 失能、失步、CH6 关闭时调用。
 */
void drv_payout_stop(void);

/**
 * 查询上一次实际下发的目标 PWM（-1000~+1000），用于 Web/日志。
 */
int16_t drv_payout_get_last_pwm(void);

#ifdef __cplusplus
}
#endif

#endif /* DRV_PAYOUT_H */
