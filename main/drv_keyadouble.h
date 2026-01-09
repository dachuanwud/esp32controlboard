#ifndef DRV_KEYADOUBLE_H
#define DRV_KEYADOUBLE_H

#include <stdint.h>
#include "esp_err.h"

/**
 * 设置左右电机速度实现运动
 * @param speed_left 左电机速度(-100到100)
 * @param speed_right 右电机速度(-100到100)
 * @return 0=成功，1=参数错误
 */
uint8_t intf_move_keyadouble(int8_t speed_left, int8_t speed_right);

/**
 * 初始化电机驱动
 * @return ESP_OK=成功
 */
esp_err_t drv_keyadouble_init(void);

/**
 * 打印CAN诊断信息
 * 用于调试CAN总线发送问题
 */
void drv_keyadouble_print_diag(void);

/**
 * 发送控制器心跳帧（不发送电机控制命令）
 * @param speed_left 左电机速度(-100到100)
 * @param speed_right 右电机速度(-100到100)
 */
void drv_keyadouble_send_heartbeat(int8_t speed_left, int8_t speed_right);

#endif /* DRV_KEYADOUBLE_H */
