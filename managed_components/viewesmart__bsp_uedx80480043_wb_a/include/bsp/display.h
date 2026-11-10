/*
 * SPDX-FileCopyrightText: 2026 VIEWESMART / VIEWE
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief BSP LCD for UEDX80480043E-WB (4.3" RGB)
 *
 * Compatible panels (same PCB, different LCD module):
 *   - UEDX80480043: 800x480 (default)
 *   - UEDX48270043: 480x272
 *
 * Select in menuconfig: Component config → Board Support Package → Display → 4.3" LCD panel.
 * Resolution, RGB timing, PCLK and bounce buffer follow that choice.
 * RGB 面板由 `bsp_display_new()` 创建。
 * 带 LVGL 的完整启动请调用 `bsp_display_start()`，内部使用 espressif/esp_lvgl_adapter。
 */

#pragma once

#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_types.h"
#include "sdkconfig.h"
#include "bsp/config.h"

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
#include "esp_lv_adapter.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

#define BSP_LCD_DATA_WIDTH               (16)
#define BSP_LCD_BITS_PER_PIXEL           (16)

#if CONFIG_BSP_LCD_PANEL_480X272
#define BSP_LCD_PANEL_NAME               "UEDX48270043"
#define BSP_LCD_H_RES                    (480)
#define BSP_LCD_V_RES                    (272)
#define BSP_LCD_PIXEL_CLOCK_HZ           (12 * 1000 * 1000)
#define BSP_LCD_HSYNC_PULSE_WIDTH        (4)
#define BSP_LCD_HSYNC_BACK_PORCH         (43)
#define BSP_LCD_HSYNC_FRONT_PORCH        (8)
#define BSP_LCD_VSYNC_PULSE_WIDTH        (4)
#define BSP_LCD_VSYNC_BACK_PORCH         (12)
#define BSP_LCD_VSYNC_FRONT_PORCH        (8)
#else
#define BSP_LCD_PANEL_NAME               "UEDX80480043"
#define BSP_LCD_H_RES                    (800)
#define BSP_LCD_V_RES                    (480)
#define BSP_LCD_PIXEL_CLOCK_HZ           (16 * 1000 * 1000)
#define BSP_LCD_HSYNC_PULSE_WIDTH        (4)
#define BSP_LCD_HSYNC_BACK_PORCH         (40)
#define BSP_LCD_HSYNC_FRONT_PORCH        (20)
#define BSP_LCD_VSYNC_PULSE_WIDTH        (4)
#define BSP_LCD_VSYNC_BACK_PORCH         (16)
#define BSP_LCD_VSYNC_FRONT_PORCH        (16)
#endif

/* Follows CONFIG_BSP_LCD_PANEL_* via Kconfig (24 for 800x480, 17 for 480x272). */
#define BSP_LCD_BOUNCE_BUFFER_HEIGHT     (CONFIG_BSP_LCD_RGB_BOUNCE_BUFFER_HEIGHT)

_Static_assert((BSP_LCD_V_RES % BSP_LCD_BOUNCE_BUFFER_HEIGHT) == 0,
               "RGB bounce buffer height must divide LCD V_RES");

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
#define BSP_LCD_ROTATION_DEFAULT         ESP_LV_ADAPTER_ROTATE_0
/* Triple full-frame swap: avoid 50-line partial copies onto the live RGB FB. */
#define BSP_LCD_TEAR_AVOID_MODE          ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_FULL

/**
 * @brief BSP display configuration
 */
typedef struct {
    esp_lv_adapter_rotation_t rotation;             /*!< LVGL display rotation */
    esp_lv_adapter_tear_avoid_mode_t tear_avoid_mode; /*!< RGB tear avoidance mode */
    uint16_t buffer_height;                         /*!< LVGL draw buffer height, 0 uses adapter default */
    uint32_t task_stack_size;                       /*!< LVGL task stack size, 0 uses adapter default */
    bool enable_touch;                              /*!< Register GT911 to LVGL adapter */
} bsp_display_config_t;

#define BSP_DISPLAY_DEFAULT_CONFIG() {             \
        .rotation        = BSP_LCD_ROTATION_DEFAULT, \
        .tear_avoid_mode = BSP_LCD_TEAR_AVOID_MODE,  \
        .buffer_height   = 0,                        \
        .task_stack_size = (32 * 1024),              \
        .enable_touch    = true,                     \
    }
#else
typedef struct {
    int dummy;
} bsp_display_config_t;
#endif

/**
 * @brief Create and initialize the RGB LCD panel (ST7262, no LVGL).
 *
 * Backlight is left off. Call `bsp_display_backlight_on()` after init.
 * RGB panel has no panel IO; `ret_io` is set to NULL when provided.
 */
esp_err_t bsp_display_new(const bsp_display_config_t *config,
                          esp_lcd_panel_handle_t *ret_panel,
                          esp_lcd_panel_io_handle_t *ret_io);

/**
 * @brief Initialize LEDC PWM used for backlight brightness.
 */
esp_err_t bsp_display_brightness_init(void);

/**
 * @brief Set backlight brightness (0-100).
 */
esp_err_t bsp_display_brightness_set(int brightness_percent);

/**
 * @brief Turn LCD backlight on (100%).
 */
esp_err_t bsp_display_backlight_on(void);

/**
 * @brief Turn LCD backlight off (0%).
 */
esp_err_t bsp_display_backlight_off(void);

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
/**
 * @brief Initialize LCD + touch and register them to esp_lvgl_adapter.
 */
lv_display_t *bsp_display_start(void);

/**
 * @brief Same as `bsp_display_start()` with custom configuration.
 */
lv_display_t *bsp_display_start_with_config(const bsp_display_config_t *config);

/**
 * @brief Get the LVGL display created by `bsp_display_start()`.
 */
lv_display_t *bsp_display_get(void);

/**
 * @brief Get the RGB LCD panel handle.
 */
esp_lcd_panel_handle_t bsp_display_get_panel(void);

/**
 * @brief Get the LVGL touch input device.
 */
lv_indev_t *bsp_display_get_input_dev(void);

/**
 * @brief Take the LVGL adapter lock.
 *
 * @param timeout_ms Timeout in milliseconds. Use 0 or -1 to wait forever.
 */
bool bsp_display_lock(int32_t timeout_ms);

/**
 * @brief Release the LVGL adapter lock.
 */
void bsp_display_unlock(void);

/**
 * @brief Rotate the LVGL display.
 */
void bsp_display_rotate(lv_display_t *disp, lv_display_rotation_t rotation);
#endif

#ifdef __cplusplus
}
#endif
