/*
 * SPDX-FileCopyrightText: 2026 VIEWESMART / VIEWE
 * SPDX-License-Identifier: Apache-2.0
 */

#include <string.h>
#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_check.h"
#include "esp_log.h"
#include "esp_lcd_panel_ops.h"
#include "esp_lcd_panel_rgb.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_lcd_touch_gt911.h"
#include "esp_spiffs.h"
#include "esp_vfs_fat.h"
#include "button_gpio.h"
#include "bsp/bsp_uedx80480043_wb_a.h"
#include "bsp/touch.h"
#include "bsp_err_check.h"

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
#include "esp_lv_adapter.h"
#endif

#include "nvs_flash.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"

#if CONFIG_BT_ENABLED
#include "esp_bt.h"
#include "esp_bt_main.h"
#endif

static const char *TAG = "UEDX80480043";

/**************************************************************************************************
 * Shared state
 **************************************************************************************************/
static bool i2c_initialized;
static i2c_master_bus_handle_t i2c_handle;
static bool backlight_initialized;
static bool nvs_initialized;
static bool net_initialized;
static sdmmc_card_t *bsp_sdcard;
static bool sdspi_bus_initialized;

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
static esp_lcd_panel_handle_t s_lcd_panel;
static lv_display_t *s_lvgl_display;
static lv_indev_t *s_touch_indev;
static esp_lcd_touch_handle_t s_touch;
static bool s_lcd_config_valid;
static bsp_display_config_t s_lcd_config;
#endif

static bool s_wifi_connected;
static bool s_wifi_started;
static bool s_wifi_handlers_registered;

/**************************************************************************************************
 * Helpers
 **************************************************************************************************/
static esp_err_t bsp_nvs_init(void)
{
    if (nvs_initialized) {
        return ESP_OK;
    }

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_RETURN_ON_ERROR(nvs_flash_erase(), TAG, "nvs erase failed");
        ret = nvs_flash_init();
    }
    ESP_RETURN_ON_ERROR(ret, TAG, "nvs init failed");
    nvs_initialized = true;
    return ESP_OK;
}

static esp_err_t bsp_net_stack_init(void)
{
    if (net_initialized) {
        return ESP_OK;
    }

    ESP_RETURN_ON_ERROR(bsp_nvs_init(), TAG, "nvs init failed");
    ESP_RETURN_ON_ERROR(esp_netif_init(), TAG, "netif init failed");
    ESP_RETURN_ON_ERROR(esp_event_loop_create_default(), TAG, "event loop create failed");
    net_initialized = true;
    return ESP_OK;
}

/**************************************************************************************************
 * I2C
 **************************************************************************************************/
esp_err_t bsp_i2c_init(void)
{
    if (i2c_initialized) {
        return ESP_OK;
    }

    const i2c_master_bus_config_t i2c_bus_conf = {
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .sda_io_num = BSP_I2C_SDA,
        .scl_io_num = BSP_I2C_SCL,
        .i2c_port = BSP_I2C_NUM,
        .glitch_ignore_cnt = 7,
        .flags = {
            .enable_internal_pullup = true,
        },
    };
    BSP_ERROR_CHECK_RETURN_ERR(i2c_new_master_bus(&i2c_bus_conf, &i2c_handle));
    i2c_initialized = true;
    ESP_LOGI(TAG, "I2C%d SDA=%d SCL=%d clk=%d Hz", BSP_I2C_NUM, BSP_I2C_SDA, BSP_I2C_SCL,
             CONFIG_BSP_I2C_CLK_SPEED_HZ);
    return ESP_OK;
}

esp_err_t bsp_i2c_deinit(void)
{
    if (!i2c_initialized) {
        return ESP_OK;
    }
    BSP_ERROR_CHECK_RETURN_ERR(i2c_del_master_bus(i2c_handle));
    i2c_handle = NULL;
    i2c_initialized = false;
    return ESP_OK;
}

i2c_master_bus_handle_t bsp_i2c_get_handle(void)
{
    bsp_i2c_init();
    return i2c_handle;
}

/**************************************************************************************************
 * Backlight (LEDC PWM on GPIO2, active high)
 **************************************************************************************************/
esp_err_t bsp_display_brightness_init(void)
{
    if (backlight_initialized) {
        return ESP_OK;
    }

    const ledc_timer_config_t timer_conf = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_10_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = 5000,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    BSP_ERROR_CHECK_RETURN_ERR(ledc_timer_config(&timer_conf));

    const ledc_channel_config_t ch_conf = {
        .gpio_num = BSP_LCD_BACKLIGHT,
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .channel = CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH,
        .timer_sel = LEDC_TIMER_0,
        .duty = 0,
        .hpoint = 0,
    };
    BSP_ERROR_CHECK_RETURN_ERR(ledc_channel_config(&ch_conf));
    backlight_initialized = true;
    return ESP_OK;
}

esp_err_t bsp_display_brightness_set(int brightness_percent)
{
    if (brightness_percent > 100) {
        brightness_percent = 100;
    }
    if (brightness_percent < 0) {
        brightness_percent = 0;
    }

    BSP_ERROR_CHECK_RETURN_ERR(bsp_display_brightness_init());

    uint32_t duty = (1023 * brightness_percent) / 100;
#if BSP_LCD_BACKLIGHT_ON_LEVEL == 0
    duty = 1023 - duty;
#endif
    BSP_ERROR_CHECK_RETURN_ERR(ledc_set_duty(LEDC_LOW_SPEED_MODE, CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH, duty));
    BSP_ERROR_CHECK_RETURN_ERR(ledc_update_duty(LEDC_LOW_SPEED_MODE, CONFIG_BSP_DISPLAY_BRIGHTNESS_LEDC_CH));
    ESP_LOGD(TAG, "Backlight %d%% (duty=%lu)", brightness_percent, (unsigned long)duty);
    return ESP_OK;
}

esp_err_t bsp_display_backlight_on(void)
{
    return bsp_display_brightness_set(100);
}

esp_err_t bsp_display_backlight_off(void)
{
    return bsp_display_brightness_set(0);
}

/**************************************************************************************************
 * Display (RGB ST7262)
 **************************************************************************************************/
static void bsp_display_hold_backlight_off(void)
{
    const gpio_config_t bk = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << BSP_LCD_BACKLIGHT,
    };
    gpio_config(&bk);
    gpio_set_level(BSP_LCD_BACKLIGHT, BSP_LCD_BACKLIGHT_ON_LEVEL ? 0 : 1);
}

static void bsp_lcd_clear_framebuffers(esp_lcd_panel_handle_t panel, uint8_t num_fbs)
{
    if (!panel || num_fbs == 0) {
        return;
    }
    if (num_fbs > 3) {
        num_fbs = 3;
    }

    void *fbs[3] = {0};
    esp_err_t ret = ESP_ERR_INVALID_ARG;
    if (num_fbs == 1) {
        ret = esp_lcd_rgb_panel_get_frame_buffer(panel, 1, &fbs[0]);
    } else if (num_fbs == 2) {
        ret = esp_lcd_rgb_panel_get_frame_buffer(panel, 2, &fbs[0], &fbs[1]);
    } else {
        ret = esp_lcd_rgb_panel_get_frame_buffer(panel, 3, &fbs[0], &fbs[1], &fbs[2]);
    }
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "get RGB frame buffer failed (%s)", esp_err_to_name(ret));
        return;
    }

    const size_t fb_size = (size_t)BSP_LCD_H_RES * BSP_LCD_V_RES * (BSP_LCD_BITS_PER_PIXEL / 8);
    for (uint8_t i = 0; i < num_fbs; i++) {
        if (fbs[i]) {
            memset(fbs[i], 0, fb_size);
        }
    }
}

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
static bsp_display_config_t bsp_display_get_default_config(void)
{
    const bsp_display_config_t config = BSP_DISPLAY_DEFAULT_CONFIG();
    return config;
}
#endif

esp_err_t bsp_display_new(const bsp_display_config_t *config, esp_lcd_panel_handle_t *ret_panel,
                          esp_lcd_panel_io_handle_t *ret_io)
{
    ESP_RETURN_ON_FALSE(ret_panel, ESP_ERR_INVALID_ARG, TAG, "panel handle output is null");

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
    if (s_lcd_panel) {
        *ret_panel = s_lcd_panel;
        if (ret_io) {
            *ret_io = NULL;
        }
        return ESP_OK;
    }

    bsp_display_config_t active_config = config ? *config : bsp_display_get_default_config();
    uint8_t num_fbs = esp_lv_adapter_get_required_frame_buffer_count(active_config.tear_avoid_mode,
                                                                     active_config.rotation);
    if (num_fbs == 0) {
        num_fbs = 1;
    }
#else
    (void)config;
    uint8_t num_fbs = 1;
#endif

    ESP_LOGI(TAG, "Initializing RGB LCD %s: %dx%d RGB565, pclk=%d Hz, bounce_h=%d, fb_count=%u",
             BSP_LCD_PANEL_NAME, BSP_LCD_H_RES, BSP_LCD_V_RES, BSP_LCD_PIXEL_CLOCK_HZ,
             BSP_LCD_BOUNCE_BUFFER_HEIGHT, (unsigned int)num_fbs);

    /* Drive BL low before LEDC/RGB start so power-on white is not visible. */
    bsp_display_hold_backlight_off();
    BSP_ERROR_CHECK_RETURN_ERR(bsp_display_backlight_off());

    const uint32_t bounce_h = BSP_LCD_BOUNCE_BUFFER_HEIGHT;
    esp_lcd_rgb_panel_config_t panel_conf = {
        .clk_src = LCD_CLK_SRC_DEFAULT,
        .dma_burst_size = 64,
        .data_width = BSP_LCD_DATA_WIDTH,
        .bits_per_pixel = BSP_LCD_BITS_PER_PIXEL,
        .de_gpio_num = BSP_LCD_DE,
        .pclk_gpio_num = BSP_LCD_PCLK,
        .vsync_gpio_num = BSP_LCD_VSYNC,
        .hsync_gpio_num = BSP_LCD_HSYNC,
        .disp_gpio_num = BSP_LCD_DISP,
        .data_gpio_nums = {
            BSP_LCD_DATA0, BSP_LCD_DATA1, BSP_LCD_DATA2, BSP_LCD_DATA3,
            BSP_LCD_DATA4, BSP_LCD_DATA5, BSP_LCD_DATA6, BSP_LCD_DATA7,
            BSP_LCD_DATA8, BSP_LCD_DATA9, BSP_LCD_DATA10, BSP_LCD_DATA11,
            BSP_LCD_DATA12, BSP_LCD_DATA13, BSP_LCD_DATA14, BSP_LCD_DATA15,
        },
        .timings = {
            .pclk_hz = BSP_LCD_PIXEL_CLOCK_HZ,
            .h_res = BSP_LCD_H_RES,
            .v_res = BSP_LCD_V_RES,
            .hsync_pulse_width = BSP_LCD_HSYNC_PULSE_WIDTH,
            .hsync_back_porch = BSP_LCD_HSYNC_BACK_PORCH,
            .hsync_front_porch = BSP_LCD_HSYNC_FRONT_PORCH,
            .vsync_pulse_width = BSP_LCD_VSYNC_PULSE_WIDTH,
            .vsync_back_porch = BSP_LCD_VSYNC_BACK_PORCH,
            .vsync_front_porch = BSP_LCD_VSYNC_FRONT_PORCH,
            .flags = {
                .pclk_active_neg = true,
            },
        },
        .flags.fb_in_psram = true,
        .num_fbs = num_fbs,
        .bounce_buffer_size_px = BSP_LCD_H_RES * bounce_h,
    };

    esp_lcd_panel_handle_t panel = NULL;
    BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_new_rgb_panel(&panel_conf, &panel));
    BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_panel_reset(panel));
    BSP_ERROR_CHECK_RETURN_ERR(esp_lcd_panel_init(panel));
    bsp_lcd_clear_framebuffers(panel, num_fbs);

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
    s_lcd_panel = panel;
    s_lcd_config = active_config;
    s_lcd_config_valid = true;
#endif

    *ret_panel = panel;
    if (ret_io) {
        *ret_io = NULL;
    }
    ESP_LOGI(TAG, "RGB LCD initialized");
    return ESP_OK;
}

/**************************************************************************************************
 * Touch (GT911)
 **************************************************************************************************/
esp_err_t bsp_touch_new(const bsp_touch_config_t *config, esp_lcd_touch_handle_t *ret_touch)
{
    ESP_RETURN_ON_FALSE(ret_touch, ESP_ERR_INVALID_ARG, TAG, "touch handle output is null");
    (void)config;

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
    if (s_touch) {
        *ret_touch = s_touch;
        return ESP_OK;
    }
#endif

    BSP_ERROR_CHECK_RETURN_ERR(bsp_i2c_init());

    /* Hold RST ourselves: the GT911 driver only waits 10 ms after reset, which
     * is too short on this board (INT is NC so the 50 ms address sequence is skipped). */
    const gpio_config_t rst_cfg = {
        .mode = GPIO_MODE_OUTPUT,
        .pin_bit_mask = 1ULL << BSP_LCD_TOUCH_RST,
    };
    gpio_config(&rst_cfg);
    gpio_set_level(BSP_LCD_TOUCH_RST, 0);
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(BSP_LCD_TOUCH_RST, 1);
    vTaskDelay(pdMS_TO_TICKS(100));

    const esp_lcd_touch_config_t tp_cfg = {
        .x_max = BSP_LCD_H_RES,
        .y_max = BSP_LCD_V_RES,
        .rst_gpio_num = GPIO_NUM_NC,
        .int_gpio_num = BSP_LCD_TOUCH_INT,
        .levels = {
            .reset = 0,
            .interrupt = 0,
        },
        .flags = {
            .swap_xy = 0,
            .mirror_x = 0,
            .mirror_y = 0,
        },
    };

    const uint8_t addrs[] = {
        ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS,
        ESP_LCD_TOUCH_IO_I2C_GT911_ADDRESS_BACKUP,
    };
    esp_err_t tp_ret = ESP_FAIL;
    for (size_t i = 0; i < sizeof(addrs) / sizeof(addrs[0]); i++) {
        esp_lcd_panel_io_handle_t tp_io_handle = NULL;
        esp_lcd_panel_io_i2c_config_t tp_io_config = ESP_LCD_TOUCH_IO_I2C_GT911_CONFIG();
        tp_io_config.dev_addr = addrs[i];
        tp_io_config.scl_speed_hz = CONFIG_BSP_I2C_CLK_SPEED_HZ;
        tp_ret = esp_lcd_new_panel_io_i2c(bsp_i2c_get_handle(), &tp_io_config, &tp_io_handle);
        if (tp_ret != ESP_OK) {
            ESP_LOGW(TAG, "GT911 IO 0x%02X failed (%s)", addrs[i], esp_err_to_name(tp_ret));
            continue;
        }
        tp_ret = esp_lcd_touch_new_i2c_gt911(tp_io_handle, &tp_cfg, ret_touch);
        if (tp_ret == ESP_OK) {
            ESP_LOGI(TAG, "GT911 found at 0x%02X", addrs[i]);
            break;
        }
        ESP_LOGW(TAG, "GT911 0x%02X probe failed (%s)", addrs[i], esp_err_to_name(tp_ret));
        if (tp_io_handle) {
            esp_lcd_panel_io_del(tp_io_handle);
        }
    }
    if (tp_ret != ESP_OK) {
        ESP_LOGE(TAG, "GT911 init failed (%s)", esp_err_to_name(tp_ret));
        return tp_ret;
    }

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
    s_touch = *ret_touch;
#endif
    ESP_LOGI(TAG, "GT911 touch initialized");
    return ESP_OK;
}

/**************************************************************************************************
 * LVGL via esp_lvgl_adapter
 **************************************************************************************************/
#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
lv_display_t *bsp_display_start(void)
{
    return bsp_display_start_with_config(NULL);
}

lv_display_t *bsp_display_start_with_config(const bsp_display_config_t *config)
{
    if (s_lvgl_display) {
        return s_lvgl_display;
    }

    bsp_display_config_t active_config = config ? *config : bsp_display_get_default_config();

    esp_lcd_panel_io_handle_t panel_io = NULL;
    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_new(&active_config, &s_lcd_panel, &panel_io));
    if (s_lcd_config_valid) {
        active_config = s_lcd_config;
    }

    if (!esp_lv_adapter_is_initialized()) {
        esp_lv_adapter_config_t adapter_config = ESP_LV_ADAPTER_DEFAULT_CONFIG();
        adapter_config.task_stack_size = active_config.task_stack_size > 0
                                         ? active_config.task_stack_size
                                         : (32 * 1024);
        adapter_config.task_core_id = 0;
        ESP_LOGI(TAG, "LVGL task stack=%lu bytes, core=%d",
                 (unsigned long)adapter_config.task_stack_size, adapter_config.task_core_id);
        BSP_ERROR_CHECK_RETURN_NULL(esp_lv_adapter_init(&adapter_config));
    }

    esp_lv_adapter_display_config_t display_config = ESP_LV_ADAPTER_DISPLAY_RGB_DEFAULT_CONFIG(
                                                         s_lcd_panel, panel_io, BSP_LCD_H_RES, BSP_LCD_V_RES,
                                                         active_config.rotation);
    display_config.tear_avoid_mode = active_config.tear_avoid_mode;
    if (active_config.buffer_height > 0) {
        display_config.profile.buffer_height = active_config.buffer_height;
    }

    s_lvgl_display = esp_lv_adapter_register_display(&display_config);
    if (!s_lvgl_display) {
        ESP_LOGE(TAG, "register RGB display to LVGL adapter failed");
        return NULL;
    }

    if (!active_config.enable_touch) {
        ESP_LOGW(TAG, "Touch init skipped by display configuration");
    } else {
        esp_err_t ret = bsp_touch_new(NULL, &s_touch);
        if (ret != ESP_OK) {
            ESP_LOGW(TAG, "touch init failed (%s), continue without touch", esp_err_to_name(ret));
        } else {
            const esp_lv_adapter_touch_config_t touch_config = ESP_LV_ADAPTER_TOUCH_DEFAULT_CONFIG(s_lvgl_display, s_touch);
            s_touch_indev = esp_lv_adapter_register_touch(&touch_config);
            if (!s_touch_indev) {
                ESP_LOGW(TAG, "register touch to LVGL adapter failed");
            }
        }
    }

    BSP_ERROR_CHECK_RETURN_NULL(esp_lv_adapter_start());
    if (bsp_display_lock(-1)) {
        lv_obj_set_style_bg_color(lv_screen_active(), lv_color_black(), 0);
        lv_obj_invalidate(lv_screen_active());
        bsp_display_unlock();
    }
    vTaskDelay(pdMS_TO_TICKS(40));
    BSP_ERROR_CHECK_RETURN_NULL(bsp_display_backlight_on());

    ESP_LOGI(TAG, "Display registered to esp_lvgl_adapter: %dx%d", BSP_LCD_H_RES, BSP_LCD_V_RES);
    return s_lvgl_display;
}

lv_display_t *bsp_display_get(void)
{
    return s_lvgl_display;
}

esp_lcd_panel_handle_t bsp_display_get_panel(void)
{
    return s_lcd_panel;
}

lv_indev_t *bsp_display_get_input_dev(void)
{
    return s_touch_indev;
}

bool bsp_display_lock(int32_t timeout_ms)
{
    return esp_lv_adapter_lock(timeout_ms == 0 ? -1 : timeout_ms) == ESP_OK;
}

void bsp_display_unlock(void)
{
    esp_lv_adapter_unlock();
}

void bsp_display_rotate(lv_display_t *disp, lv_display_rotation_t rotation)
{
    if (disp) {
        lv_display_set_rotation(disp, rotation);
    }
}
#endif

/**************************************************************************************************
 * SPIFFS
 **************************************************************************************************/
esp_err_t bsp_spiffs_mount(void)
{
    esp_vfs_spiffs_conf_t conf = {
        .base_path = CONFIG_BSP_SPIFFS_MOUNT_POINT,
        .partition_label = CONFIG_BSP_SPIFFS_PARTITION_LABEL,
        .max_files = CONFIG_BSP_SPIFFS_MAX_FILES,
#ifdef CONFIG_BSP_SPIFFS_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
    };
    esp_err_t ret = esp_vfs_spiffs_register(&conf);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SPIFFS (%s)", esp_err_to_name(ret));
        return ret;
    }

    size_t total = 0;
    size_t used = 0;
    ret = esp_spiffs_info(conf.partition_label, &total, &used);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to get SPIFFS partition info (%s)", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "SPIFFS: %d kB total, %d kB used", (int)(total / 1024), (int)(used / 1024));
    }
    return ret;
}

esp_err_t bsp_spiffs_unmount(void)
{
    return esp_vfs_spiffs_unregister(CONFIG_BSP_SPIFFS_PARTITION_LABEL);
}

/**************************************************************************************************
 * SD card (SPI)
 **************************************************************************************************/
void bsp_sdcard_get_sdspi_host(const int slot, sdmmc_host_t *config)
{
    (void)slot;
    *config = (sdmmc_host_t)SDSPI_HOST_DEFAULT();
    config->slot = BSP_SD_SPI_NUM;
    config->max_freq_khz = 10000;
}

void bsp_sdcard_sdspi_get_slot(const spi_host_device_t spi_host, sdspi_device_config_t *config)
{
    *config = (sdspi_device_config_t)SDSPI_DEVICE_CONFIG_DEFAULT();
    config->gpio_cs = BSP_SD_SPI_CS;
    config->host_id = spi_host;
}

static esp_err_t bsp_sdspi_bus_init(void)
{
    if (sdspi_bus_initialized) {
        return ESP_OK;
    }

    const spi_bus_config_t bus_cfg = {
        .mosi_io_num = BSP_SD_SPI_MOSI,
        .miso_io_num = BSP_SD_SPI_MISO,
        .sclk_io_num = BSP_SD_SPI_SCLK,
        .quadwp_io_num = GPIO_NUM_NC,
        .quadhd_io_num = GPIO_NUM_NC,
        .max_transfer_sz = 4000,
    };
    ESP_RETURN_ON_ERROR(spi_bus_initialize(BSP_SD_SPI_NUM, &bus_cfg, SDSPI_DEFAULT_DMA),
                        TAG, "SD SPI bus init failed");
    gpio_set_pull_mode(BSP_SD_SPI_MOSI, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(BSP_SD_SPI_MISO, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(BSP_SD_SPI_SCLK, GPIO_PULLUP_ONLY);
    gpio_set_pull_mode(BSP_SD_SPI_CS, GPIO_PULLUP_ONLY);
    sdspi_bus_initialized = true;
    return ESP_OK;
}

esp_err_t bsp_sdcard_sdspi_mount(bsp_sdcard_cfg_t *cfg)
{
    ESP_RETURN_ON_ERROR(bsp_sdspi_bus_init(), TAG, "SD SPI bus init failed");

    sdmmc_host_t default_host;
    sdspi_device_config_t default_slot;
    esp_vfs_fat_sdmmc_mount_config_t default_mount = {
#ifdef CONFIG_BSP_SD_FORMAT_ON_MOUNT_FAIL
        .format_if_mount_failed = true,
#else
        .format_if_mount_failed = false,
#endif
        .max_files = 5,
        .allocation_unit_size = 16 * 1024,
    };

    bsp_sdcard_get_sdspi_host(BSP_SD_SPI_NUM, &default_host);
    bsp_sdcard_sdspi_get_slot(BSP_SD_SPI_NUM, &default_slot);

    const sdmmc_host_t *host = (cfg && cfg->host) ? cfg->host : &default_host;
    const sdspi_device_config_t *slot = (cfg && cfg->slot) ? cfg->slot : &default_slot;
    const esp_vfs_fat_sdmmc_mount_config_t *mount = (cfg && cfg->mount) ? cfg->mount : &default_mount;
    sdmmc_card_t **card = (cfg && cfg->card) ? cfg->card : &bsp_sdcard;

    esp_err_t ret = esp_vfs_fat_sdspi_mount(BSP_SD_MOUNT_POINT, host, slot, mount, card);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to mount SD card (%s)", esp_err_to_name(ret));
        return ret;
    }
    bsp_sdcard = *card;
    ESP_LOGI(TAG, "SD card mounted at %s", BSP_SD_MOUNT_POINT);
    sdmmc_card_print_info(stdout, bsp_sdcard);
    return ESP_OK;
}

esp_err_t bsp_sdcard_mount(void)
{
    esp_err_t ret = ESP_FAIL;
    for (int i = 0; i < 3; i++) {
        ret = bsp_sdcard_sdspi_mount(NULL);
        if (ret == ESP_OK) {
            return ESP_OK;
        }
        ESP_LOGW(TAG, "SD mount retry %d (%s)", i + 1, esp_err_to_name(ret));
        vTaskDelay(pdMS_TO_TICKS(200));
    }
    return ret;
}

esp_err_t bsp_sdcard_unmount(void)
{
    if (!bsp_sdcard) {
        return ESP_OK;
    }
    esp_err_t ret = esp_vfs_fat_sdcard_unmount(BSP_SD_MOUNT_POINT, bsp_sdcard);
    bsp_sdcard = NULL;
    if (sdspi_bus_initialized) {
        spi_bus_free(BSP_SD_SPI_NUM);
        sdspi_bus_initialized = false;
    }
    return ret;
}

sdmmc_card_t *bsp_sdcard_get_handle(void)
{
    return bsp_sdcard;
}

/**************************************************************************************************
 * Buttons
 **************************************************************************************************/
esp_err_t bsp_iot_button_create(button_handle_t btn_array[], int *btn_cnt, int btn_array_size)
{
    ESP_RETURN_ON_FALSE(btn_array && (btn_array_size >= BSP_BUTTON_NUM), ESP_ERR_INVALID_ARG,
                        TAG, "button array is too small");

    const button_config_t btn_cfg = {0};
    const button_gpio_config_t gpio_cfg = {
        .gpio_num = BSP_BUTTON_BOOT_IO,
        .active_level = 0,
    };
    BSP_ERROR_CHECK_RETURN_ERR(iot_button_new_gpio_device(&btn_cfg, &gpio_cfg, &btn_array[0]));
    if (btn_cnt) {
        *btn_cnt = 1;
    }
    return ESP_OK;
}

/**************************************************************************************************
 * Wi-Fi
 **************************************************************************************************/
static void bsp_wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
    (void)arg;
    if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_START) {
        esp_wifi_connect();
    } else if (event_base == WIFI_EVENT && event_id == WIFI_EVENT_STA_DISCONNECTED) {
        s_wifi_connected = false;
        ESP_LOGW(TAG, "Wi-Fi disconnected, retrying");
        esp_wifi_connect();
    } else if (event_base == IP_EVENT && event_id == IP_EVENT_STA_GOT_IP) {
        const ip_event_got_ip_t *event = (const ip_event_got_ip_t *)event_data;
        s_wifi_connected = true;
        ESP_LOGI(TAG, "Wi-Fi got IP: " IPSTR, IP2STR(&event->ip_info.ip));
    }
}

esp_err_t bsp_wifi_init(void)
{
    ESP_RETURN_ON_ERROR(bsp_net_stack_init(), TAG, "net stack init failed");

    if (!esp_netif_get_handle_from_ifkey("WIFI_STA_DEF")) {
        esp_netif_create_default_wifi_sta();
    }

    if (!s_wifi_started) {
        wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
        ESP_RETURN_ON_ERROR(esp_wifi_init(&cfg), TAG, "wifi init failed");
    }

    if (!s_wifi_handlers_registered) {
        ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(WIFI_EVENT, ESP_EVENT_ANY_ID,
                                                                &bsp_wifi_event_handler, NULL, NULL),
                            TAG, "wifi event register failed");
        ESP_RETURN_ON_ERROR(esp_event_handler_instance_register(IP_EVENT, IP_EVENT_STA_GOT_IP,
                                                                &bsp_wifi_event_handler, NULL, NULL),
                            TAG, "ip event register failed");
        s_wifi_handlers_registered = true;
    }
    return ESP_OK;
}

esp_err_t bsp_wifi_connect(const char *ssid, const char *password)
{
    ESP_RETURN_ON_FALSE(ssid && ssid[0], ESP_ERR_INVALID_ARG, TAG, "ssid is empty");
    ESP_RETURN_ON_ERROR(bsp_wifi_init(), TAG, "wifi init failed");

    wifi_config_t wifi_config = {0};
    strlcpy((char *)wifi_config.sta.ssid, ssid, sizeof(wifi_config.sta.ssid));
    if (password) {
        strlcpy((char *)wifi_config.sta.password, password, sizeof(wifi_config.sta.password));
    }

    ESP_RETURN_ON_ERROR(esp_wifi_set_mode(WIFI_MODE_STA), TAG, "set wifi mode failed");
    ESP_RETURN_ON_ERROR(esp_wifi_set_config(WIFI_IF_STA, &wifi_config), TAG, "set wifi config failed");
    if (!s_wifi_started) {
        ESP_RETURN_ON_ERROR(esp_wifi_start(), TAG, "wifi start failed");
        s_wifi_started = true;
    } else {
        ESP_RETURN_ON_ERROR(esp_wifi_connect(), TAG, "wifi connect failed");
    }
    ESP_LOGI(TAG, "Wi-Fi STA connecting to \"%s\"", ssid);
    return ESP_OK;
}

esp_err_t bsp_wifi_disconnect(void)
{
    s_wifi_connected = false;
    return esp_wifi_disconnect();
}

esp_err_t bsp_wifi_stop(void)
{
    s_wifi_connected = false;
    if (!s_wifi_started) {
        return ESP_OK;
    }
    esp_err_t ret = esp_wifi_stop();
    s_wifi_started = false;
    return ret;
}

bool bsp_wifi_is_connected(void)
{
    return s_wifi_connected;
}

/**************************************************************************************************
 * Bluetooth
 **************************************************************************************************/
esp_err_t bsp_bluetooth_init(void)
{
#if !CONFIG_BT_ENABLED
    ESP_LOGE(TAG, "Bluetooth is disabled in sdkconfig (CONFIG_BT_ENABLED=n)");
    return ESP_ERR_NOT_SUPPORTED;
#else
    ESP_RETURN_ON_ERROR(bsp_nvs_init(), TAG, "nvs init failed");

    /* Free Classic BT memory before controller init (BLE-only board). */
    esp_err_t mem_ret = esp_bt_controller_mem_release(ESP_BT_MODE_CLASSIC_BT);
    if (mem_ret != ESP_OK && mem_ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGW(TAG, "classic BT mem release: %s", esp_err_to_name(mem_ret));
    }

    esp_bt_controller_config_t bt_cfg = BT_CONTROLLER_INIT_CONFIG_DEFAULT();
    esp_err_t ret = esp_bt_controller_init(&bt_cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "BT controller init failed (%s)", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bt_controller_enable(ESP_BT_MODE_BLE);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "BT controller enable failed (%s)", esp_err_to_name(ret));
        return ret;
    }

    esp_bluedroid_config_t bluedroid_cfg = BT_BLUEDROID_INIT_CONFIG_DEFAULT();
    ret = esp_bluedroid_init_with_cfg(&bluedroid_cfg);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Bluedroid init failed (%s)", esp_err_to_name(ret));
        return ret;
    }

    ret = esp_bluedroid_enable();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        ESP_LOGE(TAG, "Bluedroid enable failed (%s)", esp_err_to_name(ret));
        return ret;
    }

    ESP_LOGI(TAG, "Bluetooth BLE + Bluedroid initialized");
    ESP_LOGW(TAG, "Wi-Fi and BLE share RF on ESP32-S3; they cannot TX/RX simultaneously");
    return ESP_OK;
#endif
}

esp_err_t bsp_bluetooth_deinit(void)
{
#if !CONFIG_BT_ENABLED
    return ESP_ERR_NOT_SUPPORTED;
#else
    esp_err_t ret = esp_bluedroid_disable();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }
    ret = esp_bluedroid_deinit();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }
    ret = esp_bt_controller_disable();
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        return ret;
    }
    return esp_bt_controller_deinit();
#endif
}
