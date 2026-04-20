#ifndef T12D_RECEIVER_H
#define T12D_RECEIVER_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

#define T12D_CHANNEL_MIN_VALUE        1050U
#define T12D_CHANNEL_MID_VALUE        1500U
#define T12D_CHANNEL_MAX_VALUE        1950U
#define T12D_SWITCH_LOW_MAX_VALUE     1100U
#define T12D_SWITCH_HIGH_MIN_VALUE    1900U

// 当前车控逻辑使用的“逻辑通道”位置。
// T12D 的物理通道可以自由映射到这些逻辑槽位。
#define T12D_LOGICAL_STEERING_CHANNEL            0U
#define T12D_LOGICAL_THROTTLE_CHANNEL            2U
#define T12D_LOGICAL_SINGLE_HAND_AXIS_CHANNEL    3U
#define T12D_LOGICAL_REMOTE_ENABLE_CHANNEL       4U
#define T12D_LOGICAL_SINGLE_HAND_SWITCH_CHANNEL  6U
#define T12D_LOGICAL_LOW_SPEED_SWITCH_CHANNEL    7U

typedef struct {
    uint8_t steering_channel;
    uint8_t throttle_channel;
    uint8_t single_hand_axis_channel;
    uint8_t remote_enable_channel;
    uint8_t single_hand_switch_channel;
    uint8_t low_speed_switch_channel;
} t12d_channel_map_t;

/**
 * 获取默认 T12D 通道映射。
 * 默认值与当前项目既有控制逻辑保持一致。
 */
void t12d_receiver_get_default_map(t12d_channel_map_t *map);

/**
 * 初始化一组安全的 T12D 逻辑通道默认值。
 */
void t12d_receiver_init_safe_channels(uint16_t *channels, size_t channel_count);

/**
 * 将 T12D 原始通道值映射为当前控制逻辑使用的逻辑通道。
 * 适配时会自动规范化三段开关值，降低不同遥控器端点设置带来的兼容风险。
 */
void t12d_receiver_apply_mapping(const uint16_t *input_channels, size_t input_count,
                                 uint16_t *output_channels, size_t output_count);

/**
 * 判断三段/两段开关是否处于低档。
 */
bool t12d_receiver_switch_is_low(uint16_t value);

/**
 * 判断三段/两段开关是否处于高档。
 */
bool t12d_receiver_switch_is_high(uint16_t value);

#ifdef __cplusplus
}
#endif

#endif /* T12D_RECEIVER_H */
