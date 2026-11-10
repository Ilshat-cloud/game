# API 说明

| :1234: [能力](#能力) | :pager: [显示与触摸](#显示与触摸) | :floppy_disk: [SD 与 SPIFFS](#sd-与-spiffs) | :radio_button: [按键](#按键) | :satellite: [Wi-Fi 与 BLE](#wi-fi-与-ble) |
| :---: | :---: | :---: | :---: | :---: |

## 概述

本文说明 **UEDX80480043E-WB** 实现的 ESP-BSP API，风格对齐乐鑫官方 BSP。

头文件二选一：

```c
#include "bsp/esp-bsp.h"
/* 或 */
#include "bsp/bsp_uedx80480043_wb_a.h"
```

能力宏为 `BSP_CAPS_*`。引脚宏为 `BSP_LCD_*`、`BSP_I2C_*`、`BSP_SD_SPI_*`。

更换 4.3" 屏幕、设置 Wi-Fi 名称/密码、I2C 速率及其他 Kconfig 选项见 [README_CN.md 配置](README_CN.md#配置)。

## 能力

| 宏 | 值 |
| --- | --- |
| `BSP_CAPS_DISPLAY` | 1 |
| `BSP_CAPS_TOUCH` | 1 |
| `BSP_CAPS_BUTTONS` | 1 |
| `BSP_CAPS_SDCARD` | 1 |
| `BSP_CAPS_WIFI` | 1 |
| `BSP_CAPS_BLUETOOTH` | 1 |
| `BSP_CAPS_AUDIO` | 0 |
| `BSP_CAPS_IMU` | 0 |

板名：`BSP_BOARD_UEDX80480043_WB_A`。

随面板选项变化的 LCD 宏（`idf.py menuconfig` → Component config → Board Support Package → Display → **4.3" LCD panel**）：

* `BSP_LCD_H_RES` / `BSP_LCD_V_RES`
* `BSP_LCD_PANEL_NAME` — `"UEDX80480043"` 或 `"UEDX48270043"`
* `BSP_LCD_PIXEL_CLOCK_HZ`、`BSP_LCD_BOUNCE_BUFFER_HEIGHT`

不要在应用里写死 800×480 或 480×272。分辨率、RGB 时序、PCLK、bounce 高度会随面板选项自动匹配。详见 [更换屏幕](README_CN.md#更换屏幕800480--480272)。

## I2C

GT911 使用 BSP I2C。`bsp_touch_new()` 与 `bsp_display_start()` 会按需初始化。也可自行调用：

```c
bsp_i2c_init();
i2c_master_bus_handle_t i2c = bsp_i2c_get_handle();
/* ... */
bsp_i2c_deinit();
```

初始化可重复调用。默认速率为 `CONFIG_BSP_I2C_CLK_SPEED_HZ`（100 kHz）。

## 显示与触摸

### 启动 LVGL（推荐）

```c
lv_display_t *disp = bsp_display_start();
if (disp == NULL) {
    return;
}

if (bsp_display_lock(-1)) {
    /* 持锁期间才能调用 LVGL */
    lv_obj_t *label = lv_label_create(lv_screen_active());
    lv_label_set_text(label, "Hello");
    lv_obj_center(label);
    bsp_display_unlock();
}
```

`bsp_display_start()` 会：

1. 创建 RGB 面板（`esp_lcd_new_rgb_panel`）
2. 注册到 `esp_lvgl_adapter`
3. 启动 GT911 并注册为 LVGL indev（除非关闭触摸）
4. 启动 LVGL 任务
5. 打开背光

自定义：

```c
bsp_display_config_t cfg = BSP_DISPLAY_DEFAULT_CONFIG();
cfg.enable_touch = true;
cfg.task_stack_size = 32 * 1024;
cfg.tear_avoid_mode = ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_FULL;
lv_display_t *disp = bsp_display_start_with_config(&cfg);
```

| API | 说明 |
| --- | --- |
| `bsp_display_start()` | LCD + LVGL + 触摸 + 背光 |
| `bsp_display_start_with_config()` | 同上，带 `bsp_display_config_t` |
| `bsp_display_new()` | 仅 RGB 面板，不启动 LVGL；背光保持关闭 |
| `bsp_display_get()` / `bsp_display_get_panel()` | LVGL display / RGB panel 句柄 |
| `bsp_display_get_input_dev()` | LVGL 触摸 indev |
| `bsp_display_lock()` / `bsp_display_unlock()` | LVGL 线程安全锁 |
| `bsp_display_rotate()` | LVGL 旋转 |
| `bsp_display_brightness_set(0..100)` | 背光 PWM |
| `bsp_display_backlight_on()` / `off()` | 100% / 0% |
| `bsp_touch_new()` | 仅 GT911 |

`bsp_display_lock(-1)` 永久等待。`0` 为非阻塞。

### 不用 LVGL，只要 RGB 面板

```c
esp_lcd_panel_handle_t panel = NULL;
esp_lcd_panel_io_handle_t io = NULL; /* RGB 无 panel IO，恒为 NULL */
bsp_display_new(NULL, &panel, &io);
bsp_display_backlight_on();
```

## SD 与 SPIFFS

```c
esp_err_t ret = bsp_sdcard_mount();
if (ret == ESP_OK) {
    sdmmc_card_t *card = bsp_sdcard_get_handle();
    /* 文件在 BSP_SD_MOUNT_POINT，默认 /sdcard */
    bsp_sdcard_unmount();
}

bsp_spiffs_mount();
bsp_spiffs_unmount();
```

SD 为 SPI（`BSP_SD_SPI_*`）。挂载点：`CONFIG_BSP_SD_MOUNT_POINT`。

## 按键

BOOT 为 GPIO0：

```c
button_handle_t btn[BSP_BUTTON_NUM];
int btn_cnt = 0;
bsp_iot_button_create(btn, &btn_cnt, BSP_BUTTON_NUM);
```

## Wi-Fi 与 BLE

默认 STA 账号密码是 Kconfig 项（`idf.py menuconfig` → Component config → Board Support Package → Wi-Fi）：

* `CONFIG_BSP_WIFI_SSID`
* `CONFIG_BSP_WIFI_PASSWORD`

```c
bsp_wifi_init();
ESP_ERROR_CHECK(bsp_wifi_connect(CONFIG_BSP_WIFI_SSID, CONFIG_BSP_WIFI_PASSWORD));
/* 也可直接传字符串：bsp_wifi_connect("ssid", "password"); */
while (!bsp_wifi_is_connected()) {
    vTaskDelay(pdMS_TO_TICKS(100));
}

bsp_bluetooth_init();  /* BLE + Bluedroid，内部会初始化 NVS */
```

若程序始终自己传入账号密码，这两项可留空。只支持 **2.4 GHz** 热点。详见 [设置 Wi-Fi 名称和密码](README_CN.md#设置-wi-fi-名称和密码)。

示例 `02_wifi` 使用的是 `CONFIG_EXAMPLE_WIFI_SSID` / `CONFIG_EXAMPLE_WIFI_PASSWORD`（Example Configuration），与 `CONFIG_BSP_WIFI_*` 不是同一组选项。

STA 辅助：`bsp_wifi_disconnect()`、`bsp_wifi_stop()`。

> **注意：** ESP32-S3 的 Wi-Fi 与 BLE 共用射频前端，不能同时收发。
