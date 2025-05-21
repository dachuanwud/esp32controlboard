#ifndef SBUS_H
#define SBUS_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

/**
 * 初始化SBUS接收
 * @return ESP_OK=成功
 */
esp_err_t sbus_init(void);

/**
 * 解析SBUS数据，映射到12个通道上
 * @param sbus_data SBUS原始数据
 * @param channel 输出的通道值数组
 * @return 0=成功
 */
uint8_t parse_sbus_msg(uint8_t* sbus_data, uint16_t* channel);

/**
 * 获取最新的SBUS数据
 * @param sbus_data 存储SBUS数据的缓冲区
 * @return true=有新数据，false=无新数据
 */
bool sbus_get_data(uint8_t* sbus_data);

#endif /* SBUS_H */
