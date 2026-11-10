/*
 * SPDX-FileCopyrightText: 2026 VIEWESMART / VIEWE
 * SPDX-License-Identifier: Apache-2.0
 */

/**
 * @file
 * @brief ESP BSP: VIEWE UEDX80480043E-WB 4.3" ESP32-S3
 *
 * Board: UEDX80480043_WB_A silkscreen (WB-A / WB-B software compatible)
 * MCU:   ESP32-S3-WROOM-1 N16R8
 * LCD:   4.3" RGB — UEDX80480043 (800x480, default) or UEDX48270043 (480x272)
 * Touch: GT911
 */

#pragma once

#include "sdkconfig.h"
#include "driver/gpio.h"
#include "driver/i2c_master.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "esp_err.h"
#include "esp_vfs_fat.h"
#include "iot_button.h"
#include "sdmmc_cmd.h"
#include "bsp/config.h"
#include "bsp/display.h"

#if (BSP_CONFIG_NO_GRAPHIC_LIB == 0)
#include "lvgl.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

/**************************************************************************************************
 * BSP Board Name
 **************************************************************************************************/
#define BSP_BOARD_UEDX80480043_WB_A

/**************************************************************************************************
 * BSP Capabilities
 **************************************************************************************************/
#define BSP_CAPS_DISPLAY          1
#define BSP_CAPS_TOUCH            1
#define BSP_CAPS_BUTTONS          1
#define BSP_CAPS_AUDIO            0
#define BSP_CAPS_AUDIO_SPEAKER    0
#define BSP_CAPS_AUDIO_MIC        0
#define BSP_CAPS_SDCARD           1
#define BSP_CAPS_IMU              0
#define BSP_CAPS_WIFI             1
#define BSP_CAPS_BLUETOOTH        1

/**************************************************************************************************
 * Pinout
 **************************************************************************************************/

/* I2C (GT911) */
#define BSP_I2C_SCL               (GPIO_NUM_20)
#define BSP_I2C_SDA               (GPIO_NUM_19)

/* RGB LCD — 16-bit RGB565 */
#define BSP_LCD_PCLK              (GPIO_NUM_42)
#define BSP_LCD_DE                (GPIO_NUM_40)
#define BSP_LCD_VSYNC             (GPIO_NUM_41)
#define BSP_LCD_HSYNC             (GPIO_NUM_39)
#define BSP_LCD_DISP              (GPIO_NUM_NC)
#define BSP_LCD_DATA0             (GPIO_NUM_8)    /* B0 */
#define BSP_LCD_DATA1             (GPIO_NUM_3)    /* B1 */
#define BSP_LCD_DATA2             (GPIO_NUM_46)   /* B2 */
#define BSP_LCD_DATA3             (GPIO_NUM_9)    /* B3 */
#define BSP_LCD_DATA4             (GPIO_NUM_1)    /* B4 */
#define BSP_LCD_DATA5             (GPIO_NUM_5)    /* G0 */
#define BSP_LCD_DATA6             (GPIO_NUM_6)    /* G1 */
#define BSP_LCD_DATA7             (GPIO_NUM_7)    /* G2 */
#define BSP_LCD_DATA8             (GPIO_NUM_15)   /* G3 */
#define BSP_LCD_DATA9             (GPIO_NUM_16)   /* G4 */
#define BSP_LCD_DATA10            (GPIO_NUM_4)    /* G5 */
#define BSP_LCD_DATA11            (GPIO_NUM_45)   /* R0 */
#define BSP_LCD_DATA12            (GPIO_NUM_48)   /* R1 */
#define BSP_LCD_DATA13            (GPIO_NUM_47)   /* R2 */
#define BSP_LCD_DATA14            (GPIO_NUM_21)   /* R3 */
#define BSP_LCD_DATA15            (GPIO_NUM_14)   /* R4 */
#define BSP_LCD_BACKLIGHT         (GPIO_NUM_2)
#define BSP_LCD_BACKLIGHT_ON_LEVEL (1)

/* Touch GT911 */
#define BSP_LCD_TOUCH_RST         (GPIO_NUM_38)
#define BSP_LCD_TOUCH_INT         (GPIO_NUM_NC)   /* GPIO18 is wired but unused on this board */

/* SD card (SPI) */
#define BSP_SD_SPI_NUM            (SPI2_HOST)
#define BSP_SD_SPI_MOSI           (GPIO_NUM_11)
#define BSP_SD_SPI_MISO           (GPIO_NUM_13)
#define BSP_SD_SPI_SCLK           (GPIO_NUM_12)
#define BSP_SD_SPI_CS             (GPIO_NUM_10)
#define BSP_SD_DET                (GPIO_NUM_NC)

/* UART0 */
#define BSP_UART_TX               (GPIO_NUM_43)
#define BSP_UART_RX               (GPIO_NUM_44)

/* Buttons */
#define BSP_BUTTON_BOOT_IO        (GPIO_NUM_0)

/* Free IO / onboard WS2812B (4-pin header, remove LED if RF interference) */
#define BSP_RGB_LED               (GPIO_NUM_17)
#define BSP_IO_FREE               (GPIO_NUM_17)

typedef enum {
    BSP_BUTTON_BOOT = 0,
    BSP_BUTTON_NUM
} bsp_button_t;

/**************************************************************************************************
 * I2C
 **************************************************************************************************/
#define BSP_I2C_NUM               (CONFIG_BSP_I2C_NUM)

esp_err_t bsp_i2c_init(void);
esp_err_t bsp_i2c_deinit(void);
i2c_master_bus_handle_t bsp_i2c_get_handle(void);

/**************************************************************************************************
 * SPIFFS
 **************************************************************************************************/
#define BSP_SPIFFS_MOUNT_POINT    CONFIG_BSP_SPIFFS_MOUNT_POINT

esp_err_t bsp_spiffs_mount(void);
esp_err_t bsp_spiffs_unmount(void);

/**************************************************************************************************
 * SD card (SPI)
 **************************************************************************************************/
#define BSP_SD_MOUNT_POINT        CONFIG_BSP_SD_MOUNT_POINT

typedef struct {
    const esp_vfs_fat_sdmmc_mount_config_t *mount;
    sdmmc_card_t **card;
    const sdmmc_host_t *host;
    const void *slot;
} bsp_sdcard_cfg_t;

esp_err_t bsp_sdcard_mount(void);
esp_err_t bsp_sdcard_unmount(void);
sdmmc_card_t *bsp_sdcard_get_handle(void);
void bsp_sdcard_get_sdspi_host(const int slot, sdmmc_host_t *config);
void bsp_sdcard_sdspi_get_slot(const spi_host_device_t spi_host, sdspi_device_config_t *config);
esp_err_t bsp_sdcard_sdspi_mount(bsp_sdcard_cfg_t *cfg);

/**************************************************************************************************
 * Buttons
 **************************************************************************************************/
esp_err_t bsp_iot_button_create(button_handle_t btn_array[], int *btn_cnt, int btn_array_size);

/**************************************************************************************************
 * Wi-Fi (STA)
 *
 * ESP32-S3 Wi-Fi and BLE share one RF front-end and cannot TX/RX at the same time.
 *
 * Default SSID/password: menuconfig → Component config → Board Support Package → Wi-Fi
 * (`CONFIG_BSP_WIFI_SSID` / `CONFIG_BSP_WIFI_PASSWORD`), then
 * bsp_wifi_connect(CONFIG_BSP_WIFI_SSID, CONFIG_BSP_WIFI_PASSWORD).
 **************************************************************************************************/
esp_err_t bsp_wifi_init(void);
esp_err_t bsp_wifi_connect(const char *ssid, const char *password);
esp_err_t bsp_wifi_disconnect(void);
esp_err_t bsp_wifi_stop(void);
bool bsp_wifi_is_connected(void);

/**************************************************************************************************
 * Bluetooth (BLE controller + Bluedroid)
 **************************************************************************************************/
esp_err_t bsp_bluetooth_init(void);
esp_err_t bsp_bluetooth_deinit(void);

#ifdef __cplusplus
}
#endif
