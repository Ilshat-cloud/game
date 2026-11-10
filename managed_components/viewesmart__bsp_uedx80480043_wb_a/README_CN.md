# BSP: VIEWE UEDX80480043E-WB

[English](README.md) | [中文](README_CN.md)

| [硬件资料](https://github.com/VIEWESMART/UEDX80480043ESP32-4.3inch-Touch-Display) | [API 说明](API_CN.md) | [配置](#配置) | [示例](#compatible-bsp-examples) | [![Component Registry](https://components.espressif.com/components/viewesmart/bsp_uedx80480043_wb_a/badge.svg)](https://components.espressif.com/components/viewesmart/bsp_uedx80480043_wb_a) | ![maintenance-status](https://img.shields.io/badge/maintenance-actively--developed-brightgreen.svg) |
| --- | --- | --- | --- | --- | --- |

## 概述

UEDX80480043E-WB 是 [VIEWE](https://viewedisplay.com/) 的 4.3 寸 ESP32-S3 智能屏。本 Board Support Package（BSP）对齐乐鑫 BSP API，用于初始化 LCD、触摸、SD、Wi-Fi 和 BLE，无需再写一套板级初始化。

PCB 丝印为 **UEDX80480043_WB_A**（WB-A / WB-B 软件兼容）。同一块 PCB 也可搭载 480×272 模组，对外型号为 **UEDX48270043**。组件名为 `bsp_uedx80480043_wb_a`，换屏只需切换 LCD 面板选项。

**板载特性：**

* ESP32-S3-WROOM-1 **N16R8**（16 MB Flash + 8 MB OPI PSRAM）
* 4.3" RGB LCD，RGB565
  * **UEDX80480043**：800×480（默认）
  * **UEDX48270043**：480×272（同一 PCB 的兼容模组）
* 电容触摸：GT911（I2C）
* MicroSD（SPI）
* Wi-Fi 2.4 GHz + Bluetooth 5 LE（共用射频前端）
* BOOT 按键（GPIO0）
* USB Type-C：供电、下载、串口

> [!NOTE]
> WB-A 与 WB-B 无软件差异。新设计建议使用 WB-B。

![开发板](https://raw.githubusercontent.com/VIEWESMART/UEDX80480043ESP32-4.3inch-Touch-Display/main/image/Layout.png)

## LCD 面板

同一块 PCB 可装两种 4.3" 模组。切换方法见 [配置](#更换屏幕800480--480272)；分辨率、RGB 时序、PCLK、bounce 高度会自动跟上。

| 面板 | 分辨率 | PCLK | HSYNC（脉宽 / 后沿 / 前沿） | VSYNC（脉宽 / 后沿 / 前沿） | Bounce 高度 |
| --- | --- | --- | --- | --- | --- |
| UEDX80480043（默认） | 800×480 | 16 MHz | 4 / 40 / 20 | 4 / 16 / 16 | 24 |
| UEDX48270043 | 480×272 | 12 MHz | 4 / 43 / 8 | 4 / 12 / 8 | 17 |

两款屏均为 `pclk_active_neg`。RGB bounce buffer 在内部 SRAM，帧缓冲在 PSRAM。不要手改 bounce 高度：必须能整除 `V_RES`。

LVGL 通过 [`espressif/esp_lvgl_adapter`](https://components.espressif.com/components/espressif/esp_lvgl_adapter) 接入。本板默认：

* 防撕裂：`ESP_LV_ADAPTER_TEAR_AVOID_MODE_TRIPLE_FULL`
* LVGL 任务栈：32 KB，绑定 CPU0

## 能力与依赖

| 支持 | 能力 | 控制器/编解码 | 组件 | 版本 |
| --- | --- | --- | --- | --- |
| :heavy_check_mark: | :pager: DISPLAY | ST7262 / RGB | idf | >=5.5 |
| :heavy_check_mark: | :black_circle: LVGL | | [espressif/esp_lvgl_adapter](https://components.espressif.com/components/espressif/esp_lvgl_adapter) [lvgl/lvgl](https://components.espressif.com/components/lvgl/lvgl) | ^0.6 ^9 |
| :heavy_check_mark: | :point_up: TOUCH | GT911 | [espressif/esp_lcd_touch_gt911](https://components.espressif.com/components/espressif/esp_lcd_touch_gt911) | ^1 |
| :heavy_check_mark: | :radio_button: BUTTONS | | [espressif/button](https://components.espressif.com/components/espressif/button) | ^4 |
| :x: | :white_circle: KNOB | | | |
| :x: | :musical_note: AUDIO | | | |
| :heavy_check_mark: | :floppy_disk: SDCARD | SPI | idf | >=5.5 |
| :heavy_check_mark: | :bulb: LED | WS2812B（GPIO17） | | |
| :x: | :camera: CAMERA | | | |
| :x: | :video_game: IMU | | | |

## 引脚

| 功能 | GPIO |
| --- | --- |
| LCD PCLK / DE / HSYNC / VSYNC | 42 / 40 / 39 / 41 |
| LCD DATA0–15（RGB565） | 8, 3, 46, 9, 1, 5, 6, 7, 15, 16, 4, 45, 48, 47, 21, 14 |
| 背光 | 2（LEDC PWM，高电平点亮） |
| 触摸 SDA / SCL / RST | 19 / 20 / 38（INT 未使用） |
| SD MOSI / MISO / SCLK / CS | 11 / 13 / 12 / 10 |
| UART0 TX / RX | 43 / 44 |
| BOOT | 0 |
| 空闲 IO / WS2812B | 17 |

GT911 的 I2C 默认 **100 kHz**。

## 硬件资料

* [UEDX80480043（800×480）GitHub](https://github.com/VIEWESMART/UEDX80480043ESP32-4.3inch-Touch-Display)
* [UEDX48270043（480×272）GitHub](https://github.com/VIEWESMART/UEDX48270043E-ESP32-4.3inch-Touch-Display)

## 使用

需要 **ESP-IDF >= 5.5**。

```bash
idf.py add-dependency "viewesmart/bsp_uedx80480043_wb_a^1.0.0"
```

或在工程 `idf_component.yml` 中：

```yaml
dependencies:
  viewesmart/bsp_uedx80480043_wb_a: "^1.0.0"
```

最小示例：

```c
#include "bsp/esp-bsp.h"

void app_main(void)
{
    lv_display_t *disp = bsp_display_start();
    assert(disp);

    if (bsp_display_lock(-1)) {
        lv_obj_t *label = lv_label_create(lv_screen_active());
        lv_label_set_text(label, "UEDX80480043");
        lv_obj_center(label);
        bsp_display_unlock();
    }
}
```

调用 LVGL API 时必须夹在 `bsp_display_lock()` 与 `bsp_display_unlock()` 之间。显示、触摸、SD、Wi-Fi、BLE、按键见 [API_CN.md](API_CN.md)。更换屏幕、设置 Wi-Fi 名称/密码、I2C 速率等见 [配置](#配置)。

> [!IMPORTANT]
> ESP32-S3 的 Wi-Fi 与 BLE 共用射频前端，不能同时收发。

## 配置

BSP 选项在 menuconfig 的 **Component config → Board Support Package** 菜单下：

```bash
idf.py menuconfig
```

也可以把同样的 `CONFIG_*` 写进工程的 `sdkconfig.defaults`。

> [!IMPORTANT]
> 如果已经生成过 `sdkconfig`，只改 `sdkconfig.defaults` **不会生效**。请删除 `sdkconfig`（以及 `sdkconfig.old`），再执行 `idf.py reconfigure` 或 `idf.py build`。

### 更换屏幕（800×480 / 480×272）

**menuconfig：** Component config → Board Support Package → Display → **4.3" LCD panel**

- `UEDX80480043 800x480`（默认）
- `UEDX48270043 480x272`

或在 `sdkconfig.defaults` 中：

```
# 800x480（默认）
CONFIG_BSP_LCD_PANEL_800X480=y

# 480x272：改用下面这一行，并去掉上面那一行
# CONFIG_BSP_LCD_PANEL_480X272=y
```

选定后下列参数会自动匹配（不要手改 bounce 高度）：

| 选项 | `BSP_LCD_H_RES` × `V_RES` | PCLK | Bounce |
| --- | --- | --- | --- |
| `CONFIG_BSP_LCD_PANEL_800X480` | 800×480 | 16 MHz | 24 |
| `CONFIG_BSP_LCD_PANEL_480X272` | 480×272 | 12 MHz | 17 |

应用代码请使用 `BSP_LCD_H_RES` / `BSP_LCD_V_RES`，不要写死分辨率。

### 设置 Wi-Fi 名称和密码

**menuconfig：** Component config → Board Support Package → Wi-Fi → **Default STA SSID** / **Default STA password**

或在 `sdkconfig.defaults` 中：

```
CONFIG_BSP_WIFI_SSID="你家的WiFi"
CONFIG_BSP_WIFI_PASSWORD="密码"
```

应用里这样连接：

```c
#include "bsp/esp-bsp.h"

void app_main(void)
{
    ESP_ERROR_CHECK(bsp_wifi_connect(CONFIG_BSP_WIFI_SSID, CONFIG_BSP_WIFI_PASSWORD));
}
```

若程序自己传入 SSID/密码，这两项可留空。只支持 **2.4 GHz** 热点。

示例 `02_wifi` 把账号密码放在 **Example Configuration**（`CONFIG_EXAMPLE_WIFI_SSID` / `CONFIG_EXAMPLE_WIFI_PASSWORD`），方便在该示例里直接改，与 `CONFIG_BSP_WIFI_*` 不是同一组选项。

### 其他 BSP 选项

下表路径均位于 **Component config → Board Support Package**。

| menuconfig | `CONFIG_*` | 默认 | 说明 |
| --- | --- | --- | --- |
| Display → 4.3" LCD panel | `BSP_LCD_PANEL_800X480` / `BSP_LCD_PANEL_480X272` | 800×480 | 见上文 |
| Display → Backlight LEDC channel | `BSP_DISPLAY_BRIGHTNESS_LEDC_CH` | 1 | GPIO2 背光 PWM 通道 |
| I2C → I2C peripheral index | `BSP_I2C_NUM` | 0 | GT911 总线 |
| I2C → I2C clock speed (Hz) | `BSP_I2C_CLK_SPEED_HZ` | 100000 | 建议 100 kHz |
| SD card → mount point | `BSP_SD_MOUNT_POINT` | `/sdcard` | `bsp_sdcard_mount()` 使用 |
| SD card → Format if mount fails | `BSP_SD_FORMAT_ON_MOUNT_FAIL` | n | 会清空卡上数据 |
| SPIFFS → mount point | `BSP_SPIFFS_MOUNT_POINT` | `/spiffs` | 需要 `storage` 分区 |
| SPIFFS → partition label | `BSP_SPIFFS_PARTITION_LABEL` | `storage` | |
| Wi-Fi → Default STA SSID | `BSP_WIFI_SSID` | 空 | 见上文 |
| Wi-Fi → Default STA password | `BSP_WIFI_PASSWORD` | 空 | 见上文 |
| Enable error check in BSP | `BSP_ERROR_CHECK` | y | BSP API 出错时中止 |

### RGB + PSRAM（建议）

这些是**工程** sdkconfig 项，不是 BSP Kconfig。示例工程已经写好：

```
CONFIG_SPIRAM=y
CONFIG_SPIRAM_MODE_OCT=y
CONFIG_SPIRAM_XIP_FROM_PSRAM=y
CONFIG_ESP32S3_DATA_CACHE_LINE_64B=y
CONFIG_ESPTOOLPY_FLASHSIZE_16MB=y
CONFIG_ESP_DEFAULT_CPU_FREQ_MHZ_240=y
```

未开 OPI PSRAM 和 64 字节 cache line 时，RGB 屏可能花屏/漂移。

## Compatible BSP Examples

示例与本组件位于[GitHub](https://github.com/VIEWESMART/UEDX80480043ESP32-4.3inch-Touch-Display/tree/main/examples/esp_idf)：

| 示例 | 说明 |
| --- | --- |
| `01_i2c_scan` | 扫描 I2C，确认 GT911（0x5D / 0x14） |
| `02_wifi` | Wi-Fi STA（SSID/密码在该示例中配置） |
| `03_ble` | BLE GATT Server，Just Works 配对 |
| `04_sd` | SPI SD 挂载、列目录、读写 |
| `05_display_lvgl_port` | RGB + GT911 + `esp_lvgl_adapter`，LVGL Widgets |
| `06_album` | 从 SD 读 `jpg` / `png` / `bmp` 做电子相册 |

```bat
cd examples/05_display_lvgl_port
idf.py set-target esp32s3
idf.py build flash monitor
```

480×272 模组：按 [配置](#更换屏幕800480--480272) 选择对应面板。
