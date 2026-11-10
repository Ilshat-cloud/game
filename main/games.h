#ifndef GAMES_H
#define GAMES_H

#include <stdint.h>
#include <stdbool.h>

// Перечисление всех игровых режимов
typedef enum {
    GAME_STATE_MENU = 0,
    GAME_STATE_NEIGHBOR_QUENCH, // Игра 2: Гашение соседей
    GAME_STATE_FLASH,           // Игра 3: Вспышка (на будущее)
    GAME_STATE_SIMON,           // Игра 4: Память
    // ... остальные игры
} game_state_t;

typedef enum {
        ROUND_IDLE,
        ROUND_ACTIVE,
        GAME_OVER
    } round_state_t;

// Функция опроса кнопок (заглушка, возвращает битовую маску 10 кнопок)
uint16_t read_arcade_buttons(void);
void write_arcade_leds(uint16_t led_mask);
// Основная задача игр, работающая на ядре 1
void games_task(void *pvParameters);

#endif