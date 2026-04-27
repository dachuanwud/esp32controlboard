#include "t12d_receiver.h"

#include <string.h>
#include "esp_log.h"

static const char *TAG = "T12D";

static uint16_t clamp_axis_value(uint16_t value)
{
    if (value == 0) {
        return T12D_CHANNEL_MID_VALUE;
    }
    if (value < T12D_CHANNEL_MIN_VALUE) {
        return T12D_CHANNEL_MIN_VALUE;
    }
    if (value > T12D_CHANNEL_MAX_VALUE) {
        return T12D_CHANNEL_MAX_VALUE;
    }
    return value;
}

static uint16_t normalize_switch_value(uint16_t value)
{
    if (t12d_receiver_switch_is_low(value)) {
        return T12D_CHANNEL_MIN_VALUE;
    }
    if (t12d_receiver_switch_is_high(value)) {
        return T12D_CHANNEL_MAX_VALUE;
    }
    return T12D_CHANNEL_MID_VALUE;
}

static uint8_t pick_index(uint8_t requested_index, size_t count, uint8_t fallback_index)
{
    if (requested_index < count) {
        return requested_index;
    }
    return fallback_index;
}

void t12d_receiver_get_default_map(t12d_channel_map_t *map)
{
    if (map == NULL) {
        return;
    }

    // 实际接线/标准绑定：CH1(索引0)作为左右转向，CH3(索引2)作为前后油门
    map->steering_channel = T12D_LOGICAL_STEERING_CHANNEL;  // 0 → 物理 CH1
    map->throttle_channel = T12D_LOGICAL_THROTTLE_CHANNEL;  // 2 → 物理 CH3
    map->single_hand_axis_channel = T12D_LOGICAL_SINGLE_HAND_AXIS_CHANNEL;
    map->remote_enable_channel = T12D_LOGICAL_REMOTE_ENABLE_CHANNEL;
    map->single_hand_switch_channel = T12D_LOGICAL_SINGLE_HAND_SWITCH_CHANNEL;
    map->low_speed_switch_channel = T12D_LOGICAL_LOW_SPEED_SWITCH_CHANNEL;
}

void t12d_receiver_init_safe_channels(uint16_t *channels, size_t channel_count)
{
    if (channels == NULL || channel_count == 0) {
        return;
    }

    for (size_t i = 0; i < channel_count; ++i) {
        channels[i] = T12D_CHANNEL_MID_VALUE;
    }

    if (T12D_LOGICAL_REMOTE_ENABLE_CHANNEL < channel_count) {
        channels[T12D_LOGICAL_REMOTE_ENABLE_CHANNEL] = T12D_CHANNEL_MAX_VALUE;
    }
    if (T12D_LOGICAL_SINGLE_HAND_SWITCH_CHANNEL < channel_count) {
        channels[T12D_LOGICAL_SINGLE_HAND_SWITCH_CHANNEL] = T12D_CHANNEL_MIN_VALUE;
    }
    if (T12D_LOGICAL_LOW_SPEED_SWITCH_CHANNEL < channel_count) {
        channels[T12D_LOGICAL_LOW_SPEED_SWITCH_CHANNEL] = T12D_CHANNEL_MIN_VALUE;
    }
}

void t12d_receiver_apply_mapping(const uint16_t *input_channels, size_t input_count,
                                 uint16_t *output_channels, size_t output_count)
{
    static bool mapping_logged = false;
    t12d_channel_map_t map;
    size_t copy_count;

    if (output_channels == NULL || output_count == 0) {
        return;
    }

    t12d_receiver_init_safe_channels(output_channels, output_count);
    if (input_channels == NULL || input_count == 0) {
        return;
    }

    copy_count = input_count < output_count ? input_count : output_count;
    memcpy(output_channels, input_channels, copy_count * sizeof(uint16_t));

    t12d_receiver_get_default_map(&map);
    map.steering_channel = pick_index(map.steering_channel, input_count, T12D_LOGICAL_STEERING_CHANNEL);
    map.throttle_channel = pick_index(map.throttle_channel, input_count, T12D_LOGICAL_THROTTLE_CHANNEL);
    map.single_hand_axis_channel = pick_index(map.single_hand_axis_channel, input_count, T12D_LOGICAL_SINGLE_HAND_AXIS_CHANNEL);
    map.remote_enable_channel = pick_index(map.remote_enable_channel, input_count, T12D_LOGICAL_REMOTE_ENABLE_CHANNEL);
    map.single_hand_switch_channel = pick_index(map.single_hand_switch_channel, input_count, T12D_LOGICAL_SINGLE_HAND_SWITCH_CHANNEL);
    map.low_speed_switch_channel = pick_index(map.low_speed_switch_channel, input_count, T12D_LOGICAL_LOW_SPEED_SWITCH_CHANNEL);

    if (T12D_LOGICAL_STEERING_CHANNEL < output_count) {
        output_channels[T12D_LOGICAL_STEERING_CHANNEL] = clamp_axis_value(input_channels[map.steering_channel]);
    }
    if (T12D_LOGICAL_THROTTLE_CHANNEL < output_count) {
        output_channels[T12D_LOGICAL_THROTTLE_CHANNEL] = clamp_axis_value(input_channels[map.throttle_channel]);
    }
    if (T12D_LOGICAL_SINGLE_HAND_AXIS_CHANNEL < output_count) {
        output_channels[T12D_LOGICAL_SINGLE_HAND_AXIS_CHANNEL] = clamp_axis_value(input_channels[map.single_hand_axis_channel]);
    }
    if (T12D_LOGICAL_REMOTE_ENABLE_CHANNEL < output_count) {
        output_channels[T12D_LOGICAL_REMOTE_ENABLE_CHANNEL] = normalize_switch_value(input_channels[map.remote_enable_channel]);
    }
    if (T12D_LOGICAL_SINGLE_HAND_SWITCH_CHANNEL < output_count) {
        output_channels[T12D_LOGICAL_SINGLE_HAND_SWITCH_CHANNEL] = normalize_switch_value(input_channels[map.single_hand_switch_channel]);
    }
    if (T12D_LOGICAL_LOW_SPEED_SWITCH_CHANNEL < output_count) {
        output_channels[T12D_LOGICAL_LOW_SPEED_SWITCH_CHANNEL] = normalize_switch_value(input_channels[map.low_speed_switch_channel]);
    }

    if (!mapping_logged) {
        ESP_LOGI(TAG, "✅ T12D mapping enabled: steer=CH%u throttle=CH%u single_axis=CH%u enable=CH%u single_sw=CH%u low_sw=CH%u",
                 map.steering_channel + 1,
                 map.throttle_channel + 1,
                 map.single_hand_axis_channel + 1,
                 map.remote_enable_channel + 1,
                 map.single_hand_switch_channel + 1,
                 map.low_speed_switch_channel + 1);
        mapping_logged = true;
    }
}

bool t12d_receiver_switch_is_low(uint16_t value)
{
    return value <= T12D_SWITCH_LOW_MAX_VALUE;
}

bool t12d_receiver_switch_is_high(uint16_t value)
{
    return value >= T12D_SWITCH_HIGH_MIN_VALUE;
}
