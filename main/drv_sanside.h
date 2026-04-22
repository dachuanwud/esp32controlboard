#ifndef DRV_SANSIDE_H
#define DRV_SANSIDE_H

#include <stdint.h>
#include "esp_err.h"

uint8_t intf_move_sanside(int8_t speed_left, int8_t speed_right);
esp_err_t drv_sanside_init(void);
void drv_sanside_print_diag(void);

#endif /* DRV_SANSIDE_H */
