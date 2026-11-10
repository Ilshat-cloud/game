# API Reference

| :1234: [Capabilities](#capabilities) | :pager: [Display and touch](#display-and-touch) | :floppy_disk: [SD card and SPIFFS](#sd-card-and-spiffs) | :radio_button: [Buttons](#buttons) | :satellite: [Wi-Fi and BLE](#wi-fi-and-ble) |
| :---: | :---: | :---: | :---: | :---: |

## Overview

This document describes the ESP-BSP API as implemented by **UEDX80480043E-WB**. The API follows the Espressif BSP style so application code can stay close to other official boards.

Include either header:

```c
#include "bsp/esp-bsp.h"
/* or */
#include "bsp/bsp_uedx80480043_wb_a.h"
```

Capability macros (`BSP_CAPS_*`) show what this board actually provides. Pin macros (`BSP_LCD_*`, `BSP_I2C_*`, `BSP_SD_SPI_*`) are the default pinout.

How to switch the 4.3" LCD module, set Wi-Fi SSID/password, I2C speed and other Kconfig options: [README.md Configuration](README.md#configuration).

## Capabilities

| Macro | Value |
| --- | --- |
| `BSP_CAPS_DISPLAY` | 1 |
| `BSP_CAPS_TOUCH` | 1 |
| `BSP_CAPS_BUTTONS` | 1 |
| `BSP_CAPS_SDCARD` | 1 |
| `BSP_CAPS_WIFI` | 1 |
| `BSP_CAPS_BLUETOOTH` | 1 |
| `BSP_CAPS_AUDIO` | 0 |
| `BSP_CAPS_IMU` | 0 |

Board name: `BSP_BOARD_UEDX80480043_WB_A`.

LCD size after panel selection (`idf.py menuconfig` → Component config → Board Support Package → Display → **4.3" LCD panel**):

* `BSP_LCD_H_RES` / `BSP_LCD_V_RES`
* `BSP_LCD_PANEL_NAME` — `"UEDX80480043"` or `"UEDX48270043"`
* `BSP_LCD_PIXEL_CLOCK_HZ`, `BSP_LCD_BOUNCE_BUFFER_HEIGHT`

Do not hardcode 800×480 or 480×272. Resolution, RGB timing, PCLK and bounce height follow the panel choice. Details: [Change the LCD panel](README.md#change-the-lcd-panel-800x480--480x272).

## I2C

GT911 uses the BSP I2C bus. `bsp_touch_new()` and `bsp_display_start()` initialize it if needed. You can also do it yourself:

```c
bsp_i2c_init();
i2c_master_bus_handle_t i2c = bsp_i2c_get_handle();
/* ... */
bsp_i2c_deinit();
```

Init is idempotent. Default speed is `CONFIG_BSP_I2C_CLK_SPEED_HZ` (100 kHz).

## Display and touch

### Start LVGL (recommended)

```c
lv_display_t *disp = bsp_display_start();
if (disp == NULL) {
    return;
}

if (bsp_display_lock(-1)) {
    /* LVGL calls only while holding the lock */
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello");
    lv_obj_center(label);
    bsp_display_unlock();
}
```

`bsp_display_start()`:

1. Creates the RGB panel (`esp_lcd_new_rgb_panel`)
2. Registers it with `esp_lvgl_adapter`
3. Starts GT911 and registers it as an LVGL indev (unless disabled)
4. Starts the LVGL task
5. Turns the backlight on

Custom options:

```c
bsp_display_config_t cfg = BSP_DISPLAY_DEFAULT_CONFIG();
cfg.enable_touch = true;
cfg.task_stack_size = 32 * 1024;
cfg.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_FULL;
lv_display_t *disp = bsp_display_start_with_config(&cfg);
```

| API | Description |
| --- | --- |
| `bsp_display_start()` | LCD + LVGL + touch + backlight |
| `bsp_display_start_with_config()` | Same, with `bsp_display_config_t` |
| `bsp_display_new()` | RGB panel only, no LVGL; backlight stays off |
| `bsp_display_get()` / `bsp_display_get_panel()` | LVGL display / RGB panel handle |
| `bsp_display_get_input_dev()` | LVGL touch indev |
| `bsp_display_lock()` / `bsp_display_unlock()` | LVGL thread-safety |
| `bsp_display_rotate()` | LVGL rotation |
| `bsp_display_brightness_set(0..100)` | Backlight PWM |
| `bsp_display_backlight_on()` / `off()` | 100% / 0% |
| `bsp_touch_new()` | GT911 only |

`bsp_display_lock(-1)` waits forever. `0` is non-blocking.

### RGB panel without LVGL

```c
esp_lcd_panel_handle_t panel = NULL;
esp_lcd_panel_io_handle_t io = NULL; /* always NULL for RGB */
bsp_display_new(NULL, &panel, &io);
bsp_display_backlight_on();
```

## SD card and SPIFFS

```c
esp_err_t ret = bsp_sdcard_mount();
if (ret == ESP_OK) {
    sdmmc_card_t *card = bsp_sdcard_get_handle();
    /* files under BSP_SD_MOUNT_POINT, default /sdcard */
    bsp_sdcard_unmount();
}

bsp_spiffs_mount();
bsp_spiffs_unmount();
```

SD is SPI (`BSP_SD_SPI_*`). Mount point: `CONFIG_BSP_SD_MOUNT_POINT`.

## Buttons

BOOT is GPIO0:

```c
button_handle_t btn[BSP_BUTTON_NUM];
int btn_cnt = 0;
bsp_iot_button_create(btn, &btn_cnt, BSP_BUTTON_NUM);
```

## Wi-Fi and BLE

Default STA credentials are Kconfig symbols (`idf.py menuconfig` → Component config → Board Support Package → Wi-Fi):

* `CONFIG_BSP_WIFI_SSID`
* `CONFIG_BSP_WIFI_PASSWORD`

```c
bsp_wifi_init();
ESP_ERROR_CHECK(bsp_wifi_connect(CONFIG_BSP_WIFI_SSID, CONFIG_BSP_WIFI_PASSWORD));
/* or pass strings: bsp_wifi_connect("ssid", "password"); */
while (!bsp_wifi_is_connected()) {
    vTaskDelay(pdMS_TO_TICKS(100));
}

bsp_bluetooth_init();  /* BLE + Bluedroid, NVS included */
```

Leave both Kconfig strings empty if the application always passes credentials itself. Only 2.4 GHz APs are supported. See [Wi-Fi SSID and password](README.md#wi-fi-ssid-and-password).

The `02_wifi` example uses `CONFIG_EXAMPLE_WIFI_SSID` / `CONFIG_EXAMPLE_WIFI_PASSWORD` (Example Configuration), not `CONFIG_BSP_WIFI_*`.

Wi-Fi STA helpers: `bsp_wifi_disconnect()`, `bsp_wifi_stop()`.

> **Note:** ESP32-S3 Wi-Fi and BLE share the RF front-end. Do not expect simultaneous TX/RX.
