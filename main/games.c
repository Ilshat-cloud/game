#include "games.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"

static const char *TAG = "games_engine";

// Текущее состояние игры
static game_state_t current_game = GAME_STATE_NEIGHBOR_QUENCH;
static uint8_t game_rounds_setting = 10;

// Заглушка опроса 10 кнопок (возвращает 16-битную маску)
uint16_t read_arcade_buttons(void) {
    //TODO Здесь в будущем будет опрос пионов GPIO
    return 0; 
}

void write_arcade_leds(uint16_t led_mask) {
    // TODO: Вывод маски на физические светодиоды (через GPIO или SPI/I2C драйвер)
}

// Главная задача диспетчер игр (работает на ядре 1)
void games_task(void *pvParameters) {
    (void)pvParameters; ESP_LOGI(TAG, "Games task started on Core %d", xPortGetCoreID());

    while (1) {
        switch (current_game) {
            case GAME_STATE_MENU:
                // Ожидание выбора игры в меню
                vTaskDelay(pdMS_TO_TICKS(100));
                break;

            case GAME_STATE_NEIGHBOR_QUENCH:
                run_neighbor_quench_game(game_rounds_setting);
                break;

            default:
                current_game = GAME_STATE_MENU;
                break;
        }
    }
}


// Реализация Игры 2: «Гашение соседей»
static void run_neighbor_quench_game(uint8_t total_rounds) {
    ESP_LOGI(TAG, "Starting Game: Neighbor Quench");

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(50); // 20 FPS опрос логики

    round_state_t r_state = ROUND_IDLE;
    uint16_t target_mask = 0; // Маска погасших кнопок, которые нужно нажать
    int tick_counter = 0;
    int timeout_ticks = 20;   // 40 * 50мс = 2 секунды на реакцию
    int score = 0;

    while (current_game == GAME_STATE_NEIGHBOR_QUENCH) {
        // 1. Опрашиваем кнопки
        uint16_t pressed_buttons = read_arcade_buttons();
        if (pressed_buttons > 0) {
            ESP_LOGI(TAG, "Buttons pressed mask: 0x%04X", pressed_buttons);
        }

        switch (r_state) {
            case ROUND_IDLE:
                // Включаем все 10 светодиодов (биты 0..9)
                write_arcade_leds(0x03FF); 
                if (current_round >= total_rounds) {
                    r_state = GAME_OVER;
                    break;
                }
                // Пауза перед новым раундом (~0,2 секунды)
                if (++tick_counter > 4) {
                    tick_counter = 0;
                    
                    // Выбираем 2 случайных светодиода: один из левой зоны (0-4), один из правой (5-9)
                    int left_idx = esp_random() % 5;
                    int right_idx = 5 + (esp_random() % 5);
                    
                    target_mask = (1 << left_idx) | (1 << right_idx);
                    
                    // Гасим выбранные светодиоды (инвертируем биты в маске)
                    write_arcade_leds(0x03FF & ~target_mask);
                    
                    r_state = ROUND_ACTIVE;
                    tick_counter = 0;
                }
                break;

            case ROUND_ACTIVE:
                // Проверяем нажатия игрока
                if (pressed_buttons > 0) {
                    if (pressed_buttons == target_mask) {
                        score++;
                        ESP_LOGI(TAG, "Hit! Score: %d", score);
                        write_arcade_leds(0x03FF); // Успех: подсвечиваем всё
                        r_state = ROUND_IDLE;
                        tick_counter = 0;
                    } else {
                        ESP_LOGI(TAG, "Wrong buttons pressed!");
                    }
                }

                // Таймер истек — игрок не успел среагировать
                if (++tick_counter > timeout_ticks) {
                    ESP_LOGI(TAG, "Timeout! Resetting score. Final: %d", score);
                    score = 0;
                    r_state = ROUND_IDLE;
                    tick_counter = 0;
                }
                break;
            case GAME_OVER:
                ESP_LOGI(TAG, "Game Over! Final Score: %u / %u", score, total_rounds);
                write_arcade_leds(0x0000); // Гасим всё
                
                // Переходим в меню или ждем рестарта
                vTaskDelay(pdMS_TO_TICKS(3000)); // Показываем результат 3 секунды
                current_game = GAME_STATE_MENU;  
                break;
        }
        // Задержка до следующего тика игрового цикла
        vTaskDelayUntil(&last_wake_time, frequency);
    }
}