#include "motor_driver.h"

#include "main.h"
#include "drv_keyadouble.h"
#include "drv_sanside.h"

uint8_t bk_flag_left = 0;
uint8_t bk_flag_right = 0;

uint8_t motor_driver_move(int8_t speed_left, int8_t speed_right) {
#if MOTOR_DRIVER_PROTOCOL == MOTOR_DRIVER_PROTOCOL_WEST_CAN
  return intf_move_sanside(speed_left, speed_right);
#else
  return intf_move_keyadouble(speed_left, speed_right);
#endif
}

esp_err_t motor_driver_init(void) {
#if MOTOR_DRIVER_PROTOCOL == MOTOR_DRIVER_PROTOCOL_WEST_CAN
  return drv_sanside_init();
#else
  return drv_keyadouble_init();
#endif
}

void motor_driver_print_diag(void) {
#if MOTOR_DRIVER_PROTOCOL == MOTOR_DRIVER_PROTOCOL_WEST_CAN
  drv_sanside_print_diag();
#else
  drv_keyadouble_print_diag();
#endif
}
