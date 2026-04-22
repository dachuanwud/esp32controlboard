#ifndef MOTOR_DRIVER_H
#define MOTOR_DRIVER_H

#include <stdint.h>
#include "esp_err.h"

uint8_t motor_driver_move(int8_t speed_left, int8_t speed_right);
esp_err_t motor_driver_init(void);
void motor_driver_print_diag(void);

#endif /* MOTOR_DRIVER_H */
