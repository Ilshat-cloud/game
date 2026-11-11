#ifndef GAMES_H
#define GAMES_H

#include <stdint.h>
#include <stdbool.h>

#define GAME_OVER_TIMEOUT_MS 3000 // Время ожидания перед возвратом в меню после окончания игры
// Перечисление всех игровых режимов
typedef enum {
    GAME_STATE_MENU = 0,
    GAME_STATE_NEIGHBOR_QUENCH, // Игра 2: Гашение соседей
    GAME_STATE_FLASH,           // Игра 3: Вспышка 
    GAME_STATE_SIMON,           // Игра 4: Память
    GAME_STATE_WHACK_MOLE,      // Игра 5: Бей крота
    GAME_STATE_CANDLE_SHOOT,    // Игра 6: Стрельба по свечам
    GAME_STATE_SECRET_CODE,     // Игра 7: Тайный код (Mastermind)
    GAME_STATE_MIRROR,          // Игра 8: Зеркальное отражение
    GAME_STATE_TRAFFIC_LIGHT,    // Игра 9: Светофор (Go/No-Go)
    GAME_STATE_ELECTRIC_CIRCUIT, // Игра 10: Электрическая цепь (Паттерн)
    GAME_STATE_WAVE_CHASE,       // Игра 11: Погоня за волной
    GAME_STATE_PILOT_NAVIGATOR,  // Игра 12: Штурман и Пилот
    // ... остальные игры
} game_state_t;

typedef enum {
        ROUND_IDLE,
        ROUND_ACTIVE,
        GAME_OVER
    } round_state_t;

typedef enum {
        FLASH_SPAWN,
        FLASH_WAIT_HIT,
        FLASH_GAME_OVER
    } flash_state_t;

typedef enum {
        SIMON_ADD_STEP,
        SIMON_PLAY_SEQUENCE,
        SIMON_WAIT_INPUT,
        SIMON_NEXT_LEVEL,
        SIMON_GAME_OVER
    } simon_state_t;

// Состояние 10 свечей
typedef struct {
        bool alive;             // Горит ли еще свеча (жизнь не потеряна)
        bool fading;            // Начала ли гаснуть (мигает)
        uint32_t fade_timer;    // Таймер процесса сгорания
    } candle_t;    

    
typedef enum {
        MIRROR_SPAWN,
        MIRROR_WAIT_INPUT,
        MIRROR_GAME_OVER
    } mirror_state_t;

    
typedef enum {
        TL_NEW_CHAIN_SETUP,
        TL_GO_SPAWN_TARGET,
        TL_GO_WAIT_HIT,
        TL_NO_GO_SIGNAL,
        TL_GAME_OVER
    } tl_state_t;

typedef enum {
        WAVE_STEP_RUN,
        WAVE_ROUND_SUCCESS,
        WAVE_GAME_OVER
    } wave_state_t;
    
// Функция опроса кнопок (заглушка, возвращает битовую маску 10 кнопок)
uint16_t read_arcade_buttons(void);
void write_arcade_leds(uint16_t led_mask);
// Основная задача игр, работающая на ядре 1
void games_task(void *pvParameters);

#endif