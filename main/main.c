
#include "esp_log.h"
#include "bsp/bsp_uedx80480043_wb_a.h"
#include "ui.h"

static const char *TAG = "lvgl_port";

void app_main(void)
{
    ESP_LOGI(TAG, "display + LVGL via esp_lvgl_adapter  %dx%d", BSP_LCD_H_RES, BSP_LCD_V_RES);

    lv_display_t *disp = bsp_display_start();     //запускает на CORE 0 дисплей и LVGL, возвращает указатель на дисплей
    if (disp == NULL) {
        ESP_LOGE(TAG, "bsp_display_start failed");
        return;
    }

    if (bsp_display_lock(-1)) {
        ui_init();
        bsp_display_unlock();
    }

    ESP_LOGI(TAG, "LVGL widgets running, touch GT911 to interact");
    // Создаем задачу игр и привязываем её строго к ЯДРУ 1 (xCoreID = 1)
    // Параметры: задача, имя, размер стека, аргументы, приоритет, дескриптор, номер ядра
    BaseType_t ret = xTaskCreatePinnedToCore(games_task,"games_task", 4096, NULL,5,NULL,
        1 // <--- Ядро 1 (графика крутится на ядре 0 в адаптере LVGL)
    );

    if (ret != pdPASS) {
        ESP_LOGE(TAG, "Failed to create games_task!");
    }
}



