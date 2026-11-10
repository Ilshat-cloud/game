/*
 * SPDX-FileCopyrightText: 2026 VIEWESMART / VIEWE
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief BSP touchscreen (GT911) for UEDX80480043E-WB
 */

#pragma once

#include "esp_lcd_touch.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    void *dummy;
} bsp_touch_config_t;

/**
 * @brief Create GT911 touch controller.
 *
 * I2C is initialized automatically if needed.
 */
esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch);

#ifdef __cplusplus
}
#endif
