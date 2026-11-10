#include "lvgl.h"
#include "ui.h"
#include "esp_log.h"


static const char *TAG = "actions";

void action_game1(lv_event_t * e) {
    // Логика для первой кнопки (например, запуск игры)
    ESP_LOGI(TAG, "btn1 pressed, starting game...");
}

void action_button2(lv_event_t * e) {
    // Действие для второй кнопки
    ESP_LOGI(TAG, "btn2 pressed, starting game...");
}

void action_button3(lv_event_t * e) {
    // Действие для третьей кнопки
    ESP_LOGI(TAG, "btn3 pressed, starting game...");
}

