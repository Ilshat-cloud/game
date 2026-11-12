#include "games.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_random.h"
#include <led_strip.h>

#define ONBOARD_LED_PIN 0  // Твой пин IO0

static const char *TAG = "games_engine";
// Инициализация единственного светодиода на IO0
led_strip_handle_t onboard_led_init(void) {

    led_strip_config_t strip_config = {
        .strip_gpio_num = ONBOARD_LED_PIN,   // The GPIO that connected to the LED strip's data line
        .max_leds = 1,        // The number of LEDs in the strip,
        .led_pixel_format = LED_PIXEL_FORMAT_GRB, // Pixel format of your LED strip
        .led_model = LED_MODEL_WS2812,            // LED strip model
        .flags.invert_out = false,                // whether to invert the output signal
    };

    led_strip_rmt_config_t rmt_config = {
        .clk_src = RMT_CLK_SRC_DEFAULT,        // different clock source can lead to different power consumption
        .resolution_hz = 10 * 1000 * 1000, // 10 MHz
        .flags.with_dma = false,  
    };

    // LED Strip object handle
    led_strip_handle_t led_strip;
    ESP_ERROR_CHECK(led_strip_new_rmt_device(&strip_config, &rmt_config, &led_strip));
    ESP_LOGI(TAG, "Created LED strip object with RMT backend");
    // Гасим при старте
    return led_strip;
}





// Текущее состояние игры
static game_state_t current_game = GAME_STATE_MENU;
static uint8_t game_rounds_setting = 30;

// Заглушка опроса 10 кнопок (возвращает 16-битную маску)
uint16_t read_arcade_buttons(void) {
    //TODO Здесь в будущем будет опрос пионов GPIO
    return 0; 
}

void write_arcade_leds(uint16_t led_mask) {
    // TODO: Вывод маски на физические светодиоды (через GPIO или SPI/I2C драйвер)
}


void UpdateLed(led_strip_handle_t led_strip);

// Реализация Игры 2: «Гашение соседей»
// total_rounds - общее количество раундов
// frequency_ms - частота опроса в миллисекундах (например, 50)
static void run_neighbor_quench_game(uint8_t total_rounds, uint32_t frequency_ms) {
    ESP_LOGI(TAG, "Starting Game 2: Neighbor Quench (Total rounds: %u, Freq: %lums)", total_rounds, frequency_ms);

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(frequency_ms);


    round_state_t r_state = ROUND_IDLE;
    uint16_t target_mask = 0; // Маска погасших кнопок, которые нужно нажать
    int tick_counter = 0;
    
    // 2 секунды на реакцию, пересчитанные под текущую частоту тика
    int timeout_ticks = 2000 / frequency_ms; 
    
    int score = 0;
    uint8_t current_round = 0; // Локальный счетчик сыгранных раундов

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
                
                // Проверяем, не отыграли ли все раунды
                if (current_round >= total_rounds) {
                    r_state = GAME_OVER;
                    break;
                }

                // Пауза перед новым раундом (~0,2 секунды = 4 тика при 50мс)
                if (++tick_counter > (200 / frequency_ms)) {
                    tick_counter = 0;
                    current_round++; // Инкрементируем текущий раунд!
                    
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
                        ESP_LOGI(TAG, "Hit! Score: %d (Round %u/%u)", score, current_round, total_rounds);
                        write_arcade_leds(0x03FF); // Успех: подсвечиваем всё
                        r_state = ROUND_IDLE;
                        tick_counter = 0;
                    } else {
                        ESP_LOGI(TAG, "Wrong buttons pressed!");
                    }
                }

                // Таймер истек — игрок не успел среагировать
                if (++tick_counter > timeout_ticks) {
                    ESP_LOGI(TAG, "Timeout in round %u! Missed target.", current_round);
                    r_state = ROUND_IDLE;
                    tick_counter = 0;
                }
                break;

            case GAME_OVER:
                ESP_LOGI(TAG, "Game Over! Final Score: %u / %u", score, total_rounds);
                write_arcade_leds(0x0000); // Гасим всё
                
                vTaskDelay(pdMS_TO_TICKS(GAME_OVER_TIMEOUT_MS)); // Показываем результат
                current_game = GAME_STATE_MENU;  
                break;
        }

        // Задержка до следующего тика игрового цикла
        vTaskDelayUntil(&last_wake_time, frequency);
    }
}


// Реализация Игры 3: «Вспышка»
// rounds - число мишеней, frequency_ms - период опроса в миллисекундах (например, 10 или 20 мс)
static void run_flash_game(uint8_t rounds, uint32_t frequency_ms) {
    ESP_LOGI(TAG, "Starting Game 3: Flash (Rounds: %u, Tick: %lums)", rounds, frequency_ms);

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(frequency_ms);

    flash_state_t f_state = FLASH_SPAWN;
    uint16_t current_target_mask = 0;
    uint8_t current_round = 0;
    
    uint32_t round_ticks_counter = 0; // Считаем тики на конкретную мишень (для точного времени)
    uint32_t total_game_ticks = 0;    // Суммарные тики на всю игру

    // Максимальное время ожидания одной вспышки (например, 5 секунды = 5000 мс)
    // Переводим в тики с учетом frequency_ms
    const uint32_t max_round_ticks = 5000 / frequency_ms; 

    while (current_game == GAME_STATE_FLASH) {
        uint16_t pressed_buttons = read_arcade_buttons();

        switch (f_state) {
            case FLASH_SPAWN:
                if (current_round >= rounds) {
                    f_state = FLASH_GAME_OVER;
                    break;
                }

                current_round++;
                round_ticks_counter = 0;

                // Выбираем 1 случайный светодиод из 10 (биты 0..9)
                int random_idx = esp_random() % 10;
                current_target_mask = (1 << random_idx);

                // Зажигаем только эту мишень, остальное выключено
                write_arcade_leds(current_target_mask);
                
                f_state = FLASH_WAIT_HIT;
                ESP_LOGI(TAG, "Target spawned on LED index: %d (Round %u/%u)", random_idx, current_round, rounds);
                break;

            case FLASH_WAIT_HIT:
                round_ticks_counter++;

                // Проверяем, нажата ли именно та кнопка (строгое равенство)
                if (pressed_buttons == current_target_mask) {
                    // Успех! Гасим мишень
                    write_arcade_leds(0x0000);
                    
                    total_game_ticks += round_ticks_counter;
                    uint32_t reaction_ms = round_ticks_counter * frequency_ms;
                    
                    ESP_LOGI(TAG, "Hit! Reaction time: %lu ms", reaction_ms);

                    // Сразу переходим к созданию следующей вспышки
                    f_state = FLASH_SPAWN;
                }
                // 2. Проверяем таймаут раунда (если игрок «завис» или не успел)
                if (round_ticks_counter >= max_round_ticks) {
                    ESP_LOGW(TAG, "Round timeout! Player didn't react within 5 seconds.");
                    f_state = FLASH_GAME_OVER;
                }
                break;

            case FLASH_GAME_OVER: {
                uint32_t total_time_ms = total_game_ticks * frequency_ms;
                ESP_LOGI(TAG, "=== GAME OVER ===");
                ESP_LOGI(TAG, "Completed rounds: %u", rounds);
                ESP_LOGI(TAG, "Total time: %lu ms (%.2f seconds)", total_time_ms, (float)total_time_ms / 1000.0f);
                
                write_arcade_leds(0x0000);
                vTaskDelay(pdMS_TO_TICKS(GAME_OVER_TIMEOUT_MS)); // Пауза перед возвратом в меню
                current_game = GAME_STATE_MENU;
                break;
            }
        }

        vTaskDelayUntil(&last_wake_time, frequency);
    }
}



static void run_simon_game(uint8_t max_levels, uint32_t frequency_ms) {
    ESP_LOGI(TAG, "Starting Game 4: Simon Says (Max levels: %u)", max_levels);

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(frequency_ms);

    simon_state_t s_state = SIMON_ADD_STEP;
    
    uint8_t sequence[20]; // Массив последовательности до 20 шагов
    uint8_t current_level = 0;
    uint8_t input_index = 0;

    int tick_counter = 0;
    int step_display_index = 0;
    
    // Таймаут 10 секунд на ввод игрока (переведенный в тики)
    const uint32_t input_timeout_ticks = 10000 / frequency_ms;
    uint32_t input_idle_ticks = 0;

    bool last_button_state_pressed = false;

    while (current_game == GAME_STATE_SIMON) {
        uint16_t pressed_buttons = read_arcade_buttons();

        switch (s_state) {
            case SIMON_ADD_STEP:
                if (current_level >= max_levels) {
                    ESP_LOGI(TAG, "CONGRATULATIONS! ALL %u LEVELS COMPLETED!", max_levels);
                    s_state = SIMON_GAME_OVER;
                    break;
                }

                // Добавляем случайный светодиод (0..9) в цепочку
                sequence[current_level] = (uint8_t)(esp_random() % 10);
                current_level++;
                
                step_display_index = 0;
                tick_counter = 0;
                s_state = SIMON_PLAY_SEQUENCE;
                ESP_LOGI(TAG, "Level %u generated", current_level);
                break;

            case SIMON_PLAY_SEQUENCE:
                tick_counter++;
                
                // Скользящее окно: если уровень > 10, показываем только последние 10 шагов ("тушение" старых)
                int start_index = (current_level > 10) ? (current_level - 10) : 0;
                int effective_steps = current_level - start_index;

                if (tick_counter == 1) {
                    // Зажигаем текущий светодиод из окна воспроизведения
                    int led_to_light = sequence[start_index + step_display_index];
                    write_arcade_leds(1 << led_to_light);
                } 
                else if (tick_counter == 20) { // ~1 секунда горит светодиод (при frequency = 50мс)
                    // Гасим светодиоды между шагами
                    write_arcade_leds(0x0000);
                } 
                else if (tick_counter == 30) { // Пауза перед следующим шагом (~0.5 сек)
                    tick_counter = 0;
                    step_display_index++;
                    
                    if (step_display_index >= effective_steps) {
                        // Показ завершен, переходим к вводу игрока
                        input_index = 0;
                        input_idle_ticks = 0;
                        s_state = SIMON_WAIT_INPUT;
                        ESP_LOGI(TAG, "Your turn! Enter sequence.");
                    }
                }
                break;

            case SIMON_WAIT_INPUT:
                input_idle_ticks++;

                // Таймаут 10 секунд бездействия
                if (input_idle_ticks >= input_timeout_ticks) {
                    ESP_LOGW(TAG, "Timeout! Player didn't press anything for 10 seconds.");
                    s_state = SIMON_GAME_OVER;
                    break;
                }

                // Проверяем нажатия по фронту (чтобы палец нужно было отпускать)
                if (pressed_buttons > 0 && !last_button_state_pressed) {
                    last_button_state_pressed = true;
                    input_idle_ticks = 0; // Сбрасываем таймер при каждом нажатии

                    // Определяем номер нажатой кнопки (ищем бит от 0 до 9)
                    int pressed_idx = -1;
                    for (int i = 0; i < 10; i++) {
                        if (pressed_buttons & (1 << i)) {
                            pressed_idx = i;
                            break;
                        }
                    }

                    // Для скользящего окна правильный индекс с учетом смещения
                    int window_start = (current_level > 10) ? (current_level - 10) : 0;
                    int expected_led = sequence[window_start + input_index];

                    if (pressed_idx == expected_led) {
                        // Верно! Подсвечиваем нажатый диод в подтверждение
                        write_arcade_leds(1 << pressed_idx);
                        input_index++;

                        // Если игрок ввел всю текущую последовательность
                        int effective_len = (current_level > 10) ? 10 : current_level;
                        if (input_index >= effective_len) {
                            vTaskDelay(pdMS_TO_TICKS(300)); // Пауза успеха
                            write_arcade_leds(0x0000);
                            s_state = SIMON_NEXT_LEVEL;
                        }
                    } else {
                        // Ошибка в последовательности!
                        ESP_LOGI(TAG, "Wrong button! Expected %d, got %d", expected_led, pressed_idx);
                        s_state = SIMON_GAME_OVER;
                    }
                } 
                else if (pressed_buttons == 0) {
                    last_button_state_pressed = false;
                    write_arcade_leds(0x0000); // Гасим подтверждение
                }
                break;

            case SIMON_NEXT_LEVEL:
                vTaskDelay(pdMS_TO_TICKS(500)); // Пауза перед следующим раундом
                s_state = SIMON_ADD_STEP;
                break;

            case SIMON_GAME_OVER:
                ESP_LOGI(TAG, "=== GAME OVER ===");
                ESP_LOGI(TAG, "Reached Level: %u / %u", current_level, max_levels);
                
                write_arcade_leds(0x03FF); // Мигаем всем для индикации проигрыша
                vTaskDelay(pdMS_TO_TICKS(GAME_OVER_TIMEOUT_MS)); 
                
                write_arcade_leds(0x0000);
                current_game = GAME_STATE_MENU;
                break;
        }

        vTaskDelayUntil(&last_wake_time, frequency);
    }
}

// Реализация Игры 5: «Whack-a-Mole»
// duration_seconds - время раунда (например, 30 секунд), frequency_ms - период тика (например, 50 мс)
static void run_whack_mole_game(uint32_t duration_seconds, uint32_t frequency_ms) {
    ESP_LOGI(TAG, "Starting Game 5: Whack-a-Mole (Duration: %lu sec, Tick: %lums)", duration_seconds, frequency_ms);

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(frequency_ms);

    typedef enum {
        MOLE_SPAWN,
        MOLE_WAIT_HIT,
        MOLE_GAME_OVER
    } mole_state_t;

    mole_state_t m_state = MOLE_SPAWN;
    
    uint16_t current_mole_mask = 0;
    uint32_t score = 0;

    // Расчет общего количества тиков на весь раунд
    uint32_t total_ticks_limit = (duration_seconds * 1000) / frequency_ms;
    uint32_t elapsed_ticks = 0;

    bool last_button_state_pressed = false;

    while (current_game == GAME_STATE_WHACK_MOLE) {
        uint16_t pressed_buttons = read_arcade_buttons();

        // Общий таймер раунда тикает постоянно
        elapsed_ticks++;
        if (elapsed_ticks >= total_ticks_limit) {
            m_state = MOLE_GAME_OVER;
        }

        switch (m_state) {
            case MOLE_SPAWN: {
                // Выбираем нового случайного крота (0..9)
                int mole_idx = esp_random() % 10;
                current_mole_mask = (1 << mole_idx);

                // Зажигаем светодиод-крота
                write_arcade_leds(current_mole_mask);
                
                m_state = MOLE_WAIT_HIT;
                // ESP_LOGI(TAG, "Mole spawned at LED: %d", mole_idx);
                break;
            }

            case MOLE_WAIT_HIT:
                // Проверяем нажатия по фронту (чтобы нельзя было просто зажать кнопку)
                if (pressed_buttons > 0 && !last_button_state_pressed) {
                    last_button_state_pressed = true;

                    // Если нажали ровно на того крота, который горит
                    if (pressed_buttons == current_mole_mask) {
                        score++;
                        // ESP_LOGI(TAG, "Whack! Score: %lu", score);

                        // Крот мгновенно исчезает и появляется в новом месте
                        write_arcade_leds(0x0000);
                        m_state = MOLE_SPAWN;
                    } else {
                        // Ошибка (промах по кроту, можно добавить штраф при желании)
                        // ESP_LOGI(TAG, "Miss!");
                    }
                } 
                else if (pressed_buttons == 0) {
                    last_button_state_pressed = false;
                }
                break;

            case MOLE_GAME_OVER:
                ESP_LOGI(TAG, "=== GAME OVER ===");
                ESP_LOGI(TAG, "Time's up! Total score (Moles whacked): %lu", score);
                
                write_arcade_leds(0x03FF); // Мигаем всем по окончанию
                vTaskDelay(pdMS_TO_TICKS(GAME_OVER_TIMEOUT_MS));
                
                write_arcade_leds(0x0000);
                current_game = GAME_STATE_MENU;
                break;
        }

        vTaskDelayUntil(&last_wake_time, frequency);
    }
}


// Реализация Игры 6: «Свечной тир»
// burn_time_ms - время до полного сгорания свечи (например, 2000 мс)
// game_duration_sec - общая длительность игры в секундах (например, 30 сек)
static void run_candle_shoot_game(uint32_t burn_time_ms, uint32_t game_duration_sec) {
    ESP_LOGI(TAG, "Starting Game 6: Candle Shoot (Burn time: %lums, Duration: %lus)", burn_time_ms, game_duration_sec);

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(1); // Тактирование 1 мс для программного ШИМ


    candle_t candles[10];
    for (int i = 0; i < 10; i++) {
        candles[i].alive = true;
        candles[i].fading = false;
        candles[i].fade_timer = 0;
    }

    uint32_t total_game_ms = game_duration_sec * 1000;
    uint32_t game_timer_ms = 0;

    // Таймер случайного появления новой затухающей свечи (от 1 до 2100 мс)
    uint32_t next_spawn_target_ms = 100 + (esp_random() % 2000);
    uint32_t spawn_timer_ms = 0;

    int active_fading_candle = -1; // Какая свеча сейчас мигает (-1 если нет)
    uint32_t pwm_counter = 0;

    bool last_button_state_pressed = false;

    while (current_game == GAME_STATE_CANDLE_SHOOT) {
        uint16_t pressed_buttons = read_arcade_buttons();

        // 1. Общие таймеры игры
        game_timer_ms++;
        spawn_timer_ms++;

        if (game_timer_ms >= total_game_ms) {
            // Время вышло, победа! Подсчитываем сколько свечей выжило
            int survived = 0;
            for (int i = 0; i < 10; i++) {
                if (candles[i].alive) survived++;
            }
            ESP_LOGI(TAG, "=== GAME OVER ===");
            ESP_LOGI(TAG, "Time's up! Survived candles: %d / 10", survived);
            
            write_arcade_leds(0x03FF);
            vTaskDelay(pdMS_TO_TICKS(GAME_OVER_TIMEOUT_MS));
            write_arcade_leds(0x0000);
            current_game = GAME_STATE_MENU;
            break;
        }

        // 2. Генерация нового угасания свечи в случайный момент времени (1-3000 мс)
        if (active_fading_candle == -1 && spawn_timer_ms >= next_spawn_target_ms) {
            spawn_timer_ms = 0;
            next_spawn_target_ms = 1000 + (esp_random() % 2000);

            // Выбираем случайную живую свечу, которая еще не гаснет
            int alive_indices[10];
            int count = 0;
            for (int i = 0; i < 10; i++) {
                if (candles[i].alive && !candles[i].fading) {
                    alive_indices[count++] = i;
                }
            }

            if (count > 0) {
                int chosen = alive_indices[esp_random() % count];
                candles[chosen].fading = true;
                candles[chosen].fade_timer = 0;
                active_fading_candle = chosen;
                ESP_LOGI(TAG, "Candle #%d is dying! Save it!", chosen);
            }
        }

        // 3. Обработка сгорания активной свечи
        if (active_fading_candle != -1) {
            candles[active_fading_candle].fade_timer++;

            // Если игрок не успел нажать за burn_time_ms — свеча тухнет навсегда
            if (candles[active_fading_candle].fade_timer >= burn_time_ms) {
                ESP_LOGW(TAG, "Candle #%d burned out completely! (-1 life)", active_fading_candle);
                candles[active_fading_candle].alive = false;
                candles[active_fading_candle].fading = false;
                active_fading_candle = -1;

                // Проверка: если все 10 свечей потухли — проигрыш
                bool any_alive = false;
                for (int i = 0; i < 10; i++) {
                    if (candles[i].alive) any_alive = true;
                }
                if (!any_alive) {
                    ESP_LOGI(TAG, "=== GAME OVER ===");
                    ESP_LOGW(TAG, "All candles burned out!");
                    write_arcade_leds(0x0000);
                    vTaskDelay(pdMS_TO_TICKS(GAME_OVER_TIMEOUT_MS));
                    current_game = GAME_STATE_MENU;
                    break;
                }
            }
        }

        // 4. Обработка нажатий игрока (спасение свечи)
        if (pressed_buttons > 0 && !last_button_state_pressed) {
            last_button_state_pressed = true;

            for (int i = 0; i < 10; i++) {
                if (pressed_buttons & (1 << i)) {
                    // Если нажали на ту самую свечу, которая гасла — спасли её!
                    if (i == active_fading_candle) {
                        ESP_LOGI(TAG, "Candle #%d saved!", i);
                        candles[i].fading = false;
                        candles[i].fade_timer = 0;
                        active_fading_candle = -1;
                    }
                }
            }
        } 
        else if (pressed_buttons == 0) {
            last_button_state_pressed = false;
        }

        // 5. Формирование маски светодиодов и Программный ШИМ для мигания
        pwm_counter++;
        if (pwm_counter >= 100) { // Период ШИМ 100 мс (~10 Гц мигание)
            pwm_counter = 0;
        }
        uint16_t current_led_mask = 0; // ОБЯЗАТЕЛЬНО обнуляем маску в начале каждого тика!
        for (int i = 0; i < 10; i++) {
            if (candles[i].alive) {
                if (candles[i].fading) {
                    // Вычисляем процент сгорания свечи (от 0.0 до 1.0)
                    // fade_timer растет от 0 до burn_time_ms
                    float burn_progress = (float)candles[i].fade_timer / (float)burn_time_ms;
                    if (burn_progress > 1.0f) burn_progress = 1.0f;

                    // Яркость падает от 90% в начале до 10% в конце сгорания
                    // 0.9 - (0.8 * burn_progress)
                    float target_brightness = 0.9f - (0.8f * burn_progress);

                    // Программный ШИМ: пороговое сравнение счетчика с яркостью (от 0 до 100)
                    int threshold = (int)(target_brightness * 100.0f);
                    if (pwm_counter < threshold) {
                        current_led_mask |= (1 << i);
                    }
                } else {
                    // Обычная живая свеча горит на 100% яркости
                    current_led_mask |= (1 << i);
                }
            }
        }

        write_arcade_leds(current_led_mask);

        vTaskDelayUntil(&last_wake_time, frequency);
    }
}

// Реализация Игры 7: «Тайный код» (Mastermind)
// code_length - длина секретного кода (1-7)
// max_inputs_per_round - сколько кнопок игрок может нажать за одну попытку
// game_duration_sec - общее время игры в секундах (например, 60)
// frequency_ms - частота опроса (например, 50 мс)
static void run_secret_code_game(uint8_t code_length, uint8_t max_inputs_per_round, uint32_t game_duration_sec, uint32_t frequency_ms) {
    ESP_LOGI(TAG, "Starting Game 7: Secret Code (Length: %u, Max inputs: %u, Duration: %lus)", 
             code_length, max_inputs_per_round, game_duration_sec);

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(frequency_ms);

    typedef enum {
        CODE_GENERATE_NEW,
        CODE_WAIT_INPUT,
        CODE_SHOW_FEEDBACK,
        CODE_WIN,
        CODE_GAME_OVER
    } code_state_t;

    code_state_t c_state = CODE_GENERATE_NEW;
    code_length = (code_length > 7) ? 7 : code_length; // Ограничение длины кода до 7
    uint8_t secret_code[10];    // Массив секретного кода (значения 0..9 для кнопок)
    uint8_t player_input[10];   // Массив текущего ввода игрока
    uint8_t input_count = 0;

    uint32_t total_game_ms = game_duration_sec * 1000;
    uint32_t elapsed_game_ms = 0;
    uint32_t feedback_timer_ticks = 0;

    bool last_button_state_pressed = false;
    uint16_t feedback_led_mask = 0;
    uint16_t blinking_led_mask = 0;
    
    int pwm_counter = 0;
    bool pwm_state = false;

    while (current_game == GAME_STATE_SECRET_CODE) {
        uint16_t pressed_buttons = read_arcade_buttons();

        // Общий таймер игры
        elapsed_game_ms += frequency_ms;
        if (elapsed_game_ms >= total_game_ms) {
            ESP_LOGI(TAG, "=== GAME OVER (TIMEOUT) ===");
            c_state = CODE_GAME_OVER;
        }

        switch (c_state) {
            case CODE_GENERATE_NEW: {
                // Генерируем уникальный секретный код из code_length элементов (без повторов)
                bool used[10] = {false};
                for (int i = 0; i < code_length; i++) {
                    int val;
                    do {
                        val = esp_random() % 10;
                    } while (used[val]);
                    used[val] = true;
                    secret_code[i] = (uint8_t)val;
                }
                
                ESP_LOGI(TAG, "New secret code generated! (Length: %u)", code_length);
                // Для отладки можно вывести в лог:
                // ESP_LOGI(TAG, "Secret: %d %d %d...", secret_code[0], secret_code[1], secret_code[2]);

                input_count = 0;
                for(int i=0; i<code_length; i++) player_input[i] = 255;
                
                write_arcade_leds(0x0000);
                c_state = CODE_WAIT_INPUT;
                break;
            }

            case CODE_WAIT_INPUT:
                // Ждем пока игрок наберет ровно max_inputs_per_round кнопок
                if (pressed_buttons > 0 && !last_button_state_pressed) {
                    last_button_state_pressed = true;

                    // Определяем номер нажатой кнопки
                    int pressed_idx = -1;
                    for (int i = 0; i < 10; i++) {
                        if (pressed_buttons & (1 << i)) {
                            pressed_idx = i;
                            break;
                        }
                    }

                    if (pressed_idx != -1 && input_count < max_inputs_per_round) {
                        player_input[input_count] = (uint8_t)pressed_idx;
                        input_count++;
                        ESP_LOGI(TAG, "Input added: %d (Total: %u/%u)", pressed_idx, input_count, max_inputs_per_round);

                        // Подсвечиваем то, что уже ввел игрок
                        uint16_t current_mask = 0;
                        for(int i=0; i<input_count; i++) {
                            current_mask |= (1 << player_input[i]);
                        }
                        write_arcade_leds(current_mask);

                        // Если набрали нужное количество — переходим к проверке
                        if (input_count >= max_inputs_per_round) {
                            c_state = CODE_SHOW_FEEDBACK;
                            feedback_timer_ticks = 0;
                        }
                    }
                } 
                else if (pressed_buttons == 0) {
                    last_button_state_pressed = false;
                }
                break;

            case CODE_SHOW_FEEDBACK: {
                // Анализируем совпадения (Mastermind логика)
                feedback_led_mask = 0;   // Точные попадания (горят постоянно)
                blinking_led_mask = 0;   // Угаданы, но не на своих местах (будут мигать)

                bool secret_matched[10] = {false};
                bool input_matched[10] = {false};

                // 1. Проверяем точные совпадения (на своих местах)
                for (int i = 0; i < max_inputs_per_round && i < code_length; i++) {
                    if (player_input[i] == secret_code[i]) {
                        feedback_led_mask |= (1 << player_input[i]);
                        secret_matched[i] = true;
                        input_matched[i] = true;
                    }
                }

                // Проверка на победу (если все элементы угаданы точно)
                int exact_hits = 0;
                for(int i=0; i<code_length; i++) {
                    if ((feedback_led_mask & (1 << secret_code[i]))) exact_hits++;
                }

                if (exact_hits == code_length) {
                    ESP_LOGI(TAG, "CODE CRACKED! Generating next code...");
                    c_state = CODE_WIN;
                    break;
                }

                // 2. Проверяем частичные совпадения (не на своих местах)
                for (int i = 0; i < max_inputs_per_round; i++) {
                    if (input_matched[i]) continue;
                    for (int j = 0; j < code_length; j++) {
                        if (secret_matched[j]) continue;
                        if (player_input[i] == secret_code[j]) {
                            blinking_led_mask |= (1 << player_input[i]);
                            secret_matched[j] = true;
                            input_matched[i] = true;
                            break;
                        }
                    }
                }

                // Показываем подсказки в течение 2 секунд (при frequency_ms = 50мс это 40 тиков)
                feedback_timer_ticks++;
                
                pwm_counter++;
                if (pwm_counter >= 10) { // Частота мигания
                    pwm_counter = 0;
                    pwm_state = !pwm_state;
                }

                uint16_t render_mask = feedback_led_mask;
                if (pwm_state) {
                    render_mask |= blinking_led_mask;
                }
                write_arcade_leds(render_mask);

                if (feedback_timer_ticks > (2000 / frequency_ms)) {
                    // Сбрасываем попытку и даем ввести заново
                    input_count = 0;
                    for(int i=0; i<code_length; i++) player_input[i] = 255;
                    write_arcade_leds(0x0000);
                    c_state = CODE_WAIT_INPUT;
                }
                break;
            }

            case CODE_WIN:
                // Успешный подбор — короткая пауза и генерация нового кода
                write_arcade_leds(0x03FF);
                vTaskDelay(pdMS_TO_TICKS(500));
                c_state = CODE_GENERATE_NEW;
                break;

            case CODE_GAME_OVER:
                ESP_LOGI(TAG, "=== GAME OVER ===");
                write_arcade_leds(0x0000);
                vTaskDelay(pdMS_TO_TICKS(GAME_OVER_TIMEOUT_MS));
                current_game = GAME_STATE_MENU;
                break;
        }

        vTaskDelayUntil(&last_wake_time, frequency);
    }
}

// Реализация Игры 8: «Зеркальное отражение»
// rounds - количество раундов
// timeout_sec - таймер ожидания ответа на раунд (например, 3 секунды)
// frequency_ms - частота опроса (например, 50 мс)
static void run_mirror_game(uint8_t rounds, uint32_t timeout_sec, uint32_t frequency_ms) {
    ESP_LOGI(TAG, "Starting Game 8: Mirror Reflection (Rounds: %u, Timeout: %lus)", rounds, timeout_sec);

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(frequency_ms);


    mirror_state_t m_state = MIRROR_SPAWN;

    uint8_t current_round = 0;
    uint16_t target_display_mask = 0;  // Что горит на экране
    uint16_t target_expected_mask = 0; // Что должен нажать игрок

    uint32_t round_ticks_counter = 0;
    uint32_t max_round_ticks = (timeout_sec * 1000) / frequency_ms;
    uint32_t score = 0;

    while (current_game == GAME_STATE_MIRROR) {
        uint16_t pressed_buttons = read_arcade_buttons();

        switch (m_state) {
            case MIRROR_SPAWN: {
                if (current_round >= rounds) {
                    m_state = MIRROR_GAME_OVER;
                    break;
                }

                current_round++;
                round_ticks_counter = 0;

                // 1. Выбираем сторону источника: 0 = левая зона (биты 0..4), 1 = правая зона (биты 5..9)
                int source_side = esp_random() % 2;
                
                // 2. Выбираем случайную точку (индекс 0..4 внутри креста «X»)
                uint8_t point_idx = esp_random() % 5;

                target_display_mask = 0;
                target_expected_mask = 0;

                if (source_side == 0) {
                    // Источник слева (0..4), значит зеркальный ответ ждем справа (5..9)
                    target_display_mask = (1 << point_idx);
                    target_expected_mask = (1 << (5 + point_idx));
                } else {
                    // Источник справа (5..9), значит зеркальный ответ ждем слева (0..4)
                    target_display_mask = (1 << (5 + point_idx));
                    target_expected_mask = (1 << point_idx);
                }

                // Зажигаем точку на панели
                write_arcade_leds(target_display_mask);
                m_state = MIRROR_WAIT_INPUT;
                
                ESP_LOGI(TAG, "Round %u/%u: Light on %s side, index %u", 
                         current_round, rounds, source_side == 0 ? "LEFT" : "RIGHT", point_idx);
                break;
            }

            case MIRROR_WAIT_INPUT:
                round_ticks_counter++;

                // Проверяем нажатия игрока (строгое совпадение маски зеркальной точки)
                if (pressed_buttons > 0) {
                    if (pressed_buttons == target_expected_mask) {
                        score++;
                        ESP_LOGI(TAG, "Correct! Score: %u", score);
                        
                        // Краткая подсветка успеха (мигаем всем)
                        write_arcade_leds(0x03FF);
                        vTaskDelay(pdMS_TO_TICKS(150));
                        
                        m_state = MIRROR_SPAWN;
                    }
                }

                // Таймаут раунда истек
                if (round_ticks_counter >= max_round_ticks) {
                    ESP_LOGW(TAG, "Round %u timeout!", current_round);
                    m_state = MIRROR_SPAWN; 
                }
                break;

            case MIRROR_GAME_OVER:
                ESP_LOGI(TAG, "=== GAME OVER ===");
                ESP_LOGI(TAG, "Final Score: %u / %u", score, rounds);
                
                write_arcade_leds(0x03FF);
                vTaskDelay(pdMS_TO_TICKS(GAME_OVER_TIMEOUT_MS));
                
                write_arcade_leds(0x0000);
                current_game = GAME_STATE_MENU;
                break;
        }

        vTaskDelayUntil(&last_wake_time, frequency);
    }
}


// Реализация Игры 8: «Светофор» (Go/No-Go)
// rounds - количество раундов (цепочек мишеней)
// hit_timeout_sec - время ожидания нажатия на зеленую мишень
// frequency_ms - частота опроса (например, 50 мс)
static void run_traffic_light_game(uint8_t rounds, uint32_t hit_timeout_sec, uint32_t frequency_ms) {
    ESP_LOGI(TAG, "Starting Game 8: Traffic Light (Rounds: %u, Hit timeout: %lus)", rounds, hit_timeout_sec);

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(frequency_ms);


    tl_state_t tl_state = TL_NEW_CHAIN_SETUP;

    uint8_t current_round = 0;
    uint8_t current_chain_length = 0; // Рандомная длина от 3 до 8
    uint8_t current_step_in_chain = 0;
    
    uint16_t current_target_mask = 0;
    
    uint32_t step_ticks_counter = 0;
    uint32_t max_step_ticks = (hit_timeout_sec * 1000) / frequency_ms;
    
    uint32_t score = 0;
    uint32_t penalties = 0;

    bool last_button_state_pressed = false;
    
    int pwm_counter = 0;
    bool pwm_state = false;

    while (current_game == GAME_STATE_TRAFFIC_LIGHT) {
        uint16_t pressed_buttons = read_arcade_buttons();

        switch (tl_state) {
            case TL_NEW_CHAIN_SETUP: {
                if (current_round >= rounds) {
                    tl_state = TL_GAME_OVER;
                    break;
                }

                current_round++;
                current_step_in_chain = 0;
                
                // Рандомная длина цепочки от 3 до 8
                current_chain_length = 3 + (esp_random() % 6);
                
                ESP_LOGI(TAG, "--- Round %u/%u: Chain length %u ---", current_round, rounds, current_chain_length);
                tl_state = TL_GO_SPAWN_TARGET;
                break;
            }

            case TL_GO_SPAWN_TARGET: {
                if (current_step_in_chain >= current_chain_length) {
                    // Цепочка успешно завершена, переходим к следующему раунду
                    vTaskDelay(pdMS_TO_TICKS(300));
                    tl_state = TL_NEW_CHAIN_SETUP;
                    break;
                }

                current_step_in_chain++;
                step_ticks_counter = 0;

                // С вероятностью 20% на любом шаге может внезапно выпасть «Красный свет» (No-Go)
                // кроме самого первого шага цепочки
                if (current_step_in_chain > 1 && (esp_random() % 100 < 20)) {
                    tl_state = TL_NO_GO_SIGNAL;
                    ESP_LOGW(TAG, "NO-GO SIGNAL! Stop pressing!");
                    break;
                }

                // Выбираем случайный светодиод от 0 до 9 (обе панели «X»)
                int target_idx = esp_random() % 10;
                current_target_mask = (1 << target_idx);

                write_arcade_leds(current_target_mask);
                tl_state = TL_GO_WAIT_HIT;
                break;
            }

            case TL_GO_WAIT_HIT: {
                step_ticks_counter++;

                // Фиксация нажатия по фронту
                if (pressed_buttons > 0 && !last_button_state_pressed) {
                    last_button_state_pressed = true;

                    if (pressed_buttons == current_target_mask) {
                        // Успех! Попали в зеленую цель
                        score++;
                        write_arcade_leds(0x0000);
                        tl_state = TL_GO_SPAWN_TARGET; // Идем к следующей точке цепочки
                    } else {
                        // Ошибка: нажали не ту кнопку
                        penalties++;
                        ESP_LOGI(TAG, "Wrong button hit! Penalties: %u", penalties);
                    }
                } 
                else if (pressed_buttons == 0) {
                    last_button_state_pressed = false;
                }

                // Таймаут ожидания реакции
                if (step_ticks_counter >= max_step_ticks) {
                    ESP_LOGW(TAG, "Hit timeout! Missed target.");
                    penalties++;
                    tl_state = TL_GO_SPAWN_TARGET; // Пропускаем шаг
                }
                break;
            }

            case TL_NO_GO_SIGNAL: {
                // Режим «Стой / Не нажимай»: все диоды часто мигают (сигнал запрета) на 1.5 секунды (30 тиков по 50мс)
                step_ticks_counter++;
                
                pwm_counter++;
                if (pwm_counter >= 5) { // Частое мигание
                    pwm_counter = 0;
                    pwm_state = !pwm_state;
                }

                write_arcade_leds(pwm_state ? 0x03FF : 0x0000);

                // Если игрок по инерции нажал хоть одну кнопку во время запрета — штраф!
                if (pressed_buttons > 0 && !last_button_state_pressed) {
                    last_button_state_pressed = true;
                    penalties += 2; // Двойной штраф за нарушение стоп-сигнала
                    ESP_LOGW(TAG, "FOUL! Pressed during NO-GO! Penalties: %u", penalties);
                } 
                else if (pressed_buttons == 0) {
                    last_button_state_pressed = false;
                }

                // Длительность сигнала No-Go вышла (например, 1500 мс = 30 тиков при 50мс)
                if (step_ticks_counter >= (1500 / frequency_ms)) {
                    write_arcade_leds(0x0000);
                    vTaskDelay(pdMS_TO_TICKS(200));
                    tl_state = TL_GO_SPAWN_TARGET; // Возвращаемся к обычному режиму
                }
                break;
            }

            case TL_GAME_OVER: {
                ESP_LOGI(TAG, "=== GAME OVER ===");
                ESP_LOGI(TAG, "Successful hits: %u", score);
                ESP_LOGI(TAG, "Total penalties: %u", penalties);

                write_arcade_leds(0x03FF);
                vTaskDelay(pdMS_TO_TICKS(GAME_OVER_TIMEOUT_MS));

                write_arcade_leds(0x0000);
                current_game = GAME_STATE_MENU;
                break;
            }
        }

        vTaskDelayUntil(&last_wake_time, frequency);
    }
}



// Функция сложной трансформации состояния системы по нажатию кнопки (0..9)
static uint16_t apply_circuit_transformation(uint8_t btn_idx, uint16_t current_state) {
    uint16_t new_state = current_state;

    switch (btn_idx) {
        case 0: // Левый верхний: инвертирует сам себя
            new_state ^= (1 << 0);
            break;

        case 1: { // Правый верхний: меняет местами бит 0 и бит 4
            bool b0 = (new_state & (1 << 0)) != 0;
            bool b4 = (new_state & (1 << 4)) != 0;
            if (b4) new_state |= (1 << 0); else new_state &= ~(1 << 0);
            if (b0) new_state |= (1 << 4); else new_state &= ~(1 << 4);
            break;
        }

        case 2: { // Центральный: циклический сдвиг битов 0..4 на 1 позицию влево
            uint16_t low_nibble = new_state & 0x001F; // маска первых 5 диодов
            low_nibble = (low_nibble << 1) | (low_nibble >> 4);
            new_state = (new_state & 0xFFE0) | (low_nibble & 0x001F);
            break;
        }

        case 3: { // Левый нижний: меняет местами бит 2 и бит 4
            bool b2 = (new_state & (1 << 2)) != 0;
            bool b4 = (new_state & (1 << 4)) != 0;
            if (b4) new_state |= (1 << 2); else new_state &= ~(1 << 2);
            if (b2) new_state |= (1 << 4); else new_state &= ~(1 << 4);
            break;
        }

        case 4: { // Правый нижний (сложная кнопка): зажигает или тушит случайную пару диодов
            int p1 = esp_random() % 10;
            int p2 = esp_random() % 10;
            new_state ^= (1 << p1);
            new_state ^= (1 << p2);
            break;
        }

        default: {
            // Для остальных кнопок (5-9 на второй панели) — зеркальные или крестовые переключения соседей
            int target = btn_idx % 10;
            new_state ^= (1 << target);
            if (target > 0) new_state ^= (1 << (target - 1));
            if (target < 9) new_state ^= (1 << (target + 1));
            break;
        }
    }

    return new_state;
}

// Реализация Игры 12: «Электрическая цепь»
// target_count - количество диодов в целевом паттерне (например, 4)
// timeout_sec - общее время на решение головоломки (например, 30 секунд)
// frequency_ms - частота опроса (например, 50 мс)
static void run_electric_circuit_game(uint8_t target_count, uint32_t timeout_sec, uint32_t frequency_ms) {
    ESP_LOGI(TAG, "Starting Game 12: Electric Circuit (Target count: %u, Timeout: %lus)", target_count, timeout_sec);

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(frequency_ms);

    typedef enum {
        CIRCUIT_SHOW_TARGET,
        CIRCUIT_PLAYING,
        CIRCUIT_VICTORY,
        CIRCUIT_GAME_OVER
    } circuit_state_t;

    circuit_state_t c_state = CIRCUIT_SHOW_TARGET;

    uint16_t target_mask = 0;
    uint16_t player_mask = 0;

    uint32_t show_timer_ticks = 0;
    uint32_t max_show_ticks = 3000 / frequency_ms; // 3 секунды показа паттерна

    uint32_t game_timer_ticks = 0;
    uint32_t max_game_ticks = (timeout_sec * 1000) / frequency_ms;

    bool last_button_state_pressed = false;

    while (current_game == GAME_STATE_ELECTRIC_CIRCUIT) {
        uint16_t pressed_buttons = read_arcade_buttons();

        switch (c_state) {
            case CIRCUIT_SHOW_TARGET: {
                // 1. Генерируем случайный целевой паттерн из target_count диодов
                target_mask = 0;
                int generated = 0;
                while (generated < target_count && target_count <= 10) {
                    int idx = esp_random() % 10;
                    if (!(target_mask & (1 << idx))) {
                        target_mask |= (1 << idx);
                        generated++;
                    }
                }

                ESP_LOGI(TAG, "Target pattern generated. Memorize it for 3 seconds!");
                write_arcade_leds(target_mask);

                show_timer_ticks = 0;
                player_mask = 0; // Игрок начинает с пустой панели
                c_state = CIRCUIT_PLAYING; // Переходим в фазу игры, но первые 3 сек показываем цель
                break;
            }

            case CIRCUIT_PLAYING: {
                show_timer_ticks++;
                game_timer_ticks++;

                // Первые 3 секунды держим подсказку (целевой паттерн) горящей
                if (show_timer_ticks <= max_show_ticks) {
                    write_arcade_leds(target_mask);
                    break; // Пока идет показ, нажимать нельзя
                } else if (show_timer_ticks == max_show_ticks + 1) {
                    // Время показа вышло — гасим всё, игрок должен восстановить по памяти
                    ESP_LOGI(TAG, "Pattern hidden! Solve the circuit.");
                    write_arcade_leds(player_mask);
                }

                // Общий таймаут игры исчерпан
                if (game_timer_ticks >= max_game_ticks) {
                    ESP_LOGW(TAG, "Time's up! Circuit failed.");
                    c_state = CIRCUIT_GAME_OVER;
                    break;
                }

                // Обработка нажатий кнопок игрока по фронту
                if (pressed_buttons > 0 && !last_button_state_pressed) {
                    last_button_state_pressed = true;

                    // Находим нажатую кнопку (бит 0..9)
                    int pressed_btn = -1;
                    for (int i = 0; i < 10; i++) {
                        if (pressed_buttons & (1 << i)) {
                            pressed_btn = i;
                            break;
                        }
                    }

                    if (pressed_btn != -1) {
                        // Применяем сложную трансформацию схемы
                        player_mask = apply_circuit_transformation(pressed_btn, player_mask);
                        write_arcade_leds(player_mask);
                        ESP_LOGI(TAG, "Button %d pressed. Circuit updated.", pressed_btn);

                        // Проверяем, совпал ли текущий узор игрока с целевым паттерном
                        if (player_mask == target_mask) {
                            ESP_LOGI(TAG, "CIRCUIT SOLVED SUCCESSFULLY!");
                            c_state = CIRCUIT_VICTORY;
                        }
                    }
                } 
                else if (pressed_buttons == 0) {
                    last_button_state_pressed = false;
                }
                break;
            }

            case CIRCUIT_VICTORY: {
                // Победное мигание
                write_arcade_leds(0x03FF);
                vTaskDelay(pdMS_TO_TICKS(500));
                write_arcade_leds(0x0000);
                vTaskDelay(pdMS_TO_TICKS(500));
                
                // Возврат в меню или новый раунд
                current_game = GAME_STATE_MENU;
                break;
            }

            case CIRCUIT_GAME_OVER: {
                ESP_LOGI(TAG, "=== GAME OVER ===");
                write_arcade_leds(0x03FF); // Индикация поражения
                vTaskDelay(pdMS_TO_TICKS(GAME_OVER_TIMEOUT_MS));

                write_arcade_leds(0x0000);
                current_game = GAME_STATE_MENU;
                break;
            }
        }

        vTaskDelayUntil(&last_wake_time, frequency);
    }
}

// Реализация Игры 13: «Погоня за волной» (Wave Chase)
// rounds - общее число успешных раундов для победы
// initial_speed_ticks - начальная скорость (сколько тиков горит один диод, например, 30 тиков по 10мс = 300мс)
// speed_step_ticks - на сколько тиков ускоряется волна в каждом новом раунде
// width - ширина волны (от 1 до 3 диодов)
static void run_wave_chase_game(uint8_t rounds, uint32_t initial_speed_ticks, uint32_t speed_step_ticks,uint8_t width) {
    ESP_LOGI(TAG, "Starting Game 13: Wave Chase (Rounds: %u, Init speed: %lu ticks)", rounds, initial_speed_ticks);
    if (width < 1) width = 1;
    if (width > 3) width = 3;
    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(10); // Базовый тик логики 10 мс для плавной волны


    wave_state_t w_state = WAVE_STEP_RUN;

    uint8_t current_round = 1;
    int8_t current_led_idx = 0;   // Голова волны (0 до 9)
    int8_t direction = 1;         // 1 = движение вправо (0->9), -1 = движение влево (9->0)
    uint32_t step_ticks_counter = 0;
    
    int32_t current_speed = (int32_t)initial_speed_ticks;
    uint32_t score = 0;
    uint32_t fails_count = 0;
    bool last_button_state_pressed = false;

    while (current_game == GAME_STATE_WAVE_CHASE) {
        uint16_t pressed_buttons = read_arcade_buttons();

        // Формируем маску волны с учетом ширины (1-3 диода) и направления хвоста
        uint16_t wave_mask = 0;
        for (uint8_t i = 0; i < width; i++) {
            int idx = current_led_idx - (direction * i);
            if (idx >= 0 && idx <= 9) {
                wave_mask |= (1 << idx);
            }
        }

        switch (w_state) {
            case WAVE_STEP_RUN: {
                step_ticks_counter++;

                // Проверяем попадание игрока в текущую волну
                if (pressed_buttons > 0 && !last_button_state_pressed) {
                    last_button_state_pressed = true;

                    // Если нажата кнопка, входящая в активную зону волны
                    if ((pressed_buttons & wave_mask) != 0 && (pressed_buttons & ~wave_mask) == 0) {
                        score++;
                        ESP_LOGI(TAG, "Hit wave! Score: %u", score);
                    } else {
                        fails_count++;
                        ESP_LOGW(TAG, "Miss or wrong button! Fails: %u", fails_count);
                        write_arcade_leds(0x03FF); // Индикация ошибки миганием
                        vTaskDelay(pdMS_TO_TICKS(100));
                    }
                } 
                else if (pressed_buttons == 0) {
                    last_button_state_pressed = false;
                }

                // Автоматическое перемещение волны по таймеру скорости
                if (step_ticks_counter >= (uint32_t)current_speed) {
                    step_ticks_counter = 0;
                    current_led_idx += direction;

                    // Если дошли до правого края (9) — разворачиваемся налево
                    if (current_led_idx > 9) {
                        current_led_idx = 9;
                        direction = -1;
                        ESP_LOGI(TAG, "Wave reached end (9), turning back left.");
                    } 
                    // Если после разворота вернулись в начало (0) — цикл завершен!
                    else if (current_led_idx < 0) {
                        current_led_idx = 0;
                        direction = 1;
                        w_state = WAVE_ROUND_SUCCESS;
                        break;
                    }
                }

                // Отображаем текущую волну на светодиодах
                write_arcade_leds(wave_mask);
                break;
            }

            case WAVE_ROUND_SUCCESS: {
                ESP_LOGI(TAG, "Round %u completed (Full ping-pong cycle)!", current_round);
                
                write_arcade_leds(0x03FF);
                vTaskDelay(pdMS_TO_TICKS(300));

                current_round++;
                if (current_round > rounds) {
                    w_state = WAVE_GAME_OVER;
                    break;
                }

                // Ускоряем волну в новом раунде (но не быстрее 5 тиков)
                current_speed -= (int32_t)speed_step_ticks;
                if (current_speed < 5) current_speed = 5;

                current_led_idx = 0;
                direction = 1;
                step_ticks_counter = 0;
                w_state = WAVE_STEP_RUN;
                break;
            }

            case WAVE_GAME_OVER: {
                ESP_LOGI(TAG, "=== GAME OVER ===");
                ESP_LOGI(TAG, "Completed all %u rounds! Score: %u, Fails: %u", rounds, score, fails_count);

                write_arcade_leds(0x03FF);
                vTaskDelay(pdMS_TO_TICKS(GAME_OVER_TIMEOUT_MS));

                write_arcade_leds(0x0000);
                current_game = GAME_STATE_MENU;
                break;
            }
        }

        vTaskDelayUntil(&last_wake_time, frequency);
    }
}


// Генерация маски ровно из 2 случайных уникальных диодов в пределах заданного диапазона (offset: 0 или 5)
static uint16_t generate_two_random_dots(uint8_t offset) {
    uint8_t d1 = esp_random() % 5;
    uint8_t d2;
    do {
        d2 = esp_random() % 5;
    } while (d2 == d1);

    return (1 << (offset + d1)) | (1 << (offset + d2));
}

// Реализация Игры 14: «Штурман и Пилот» (Pilot & Navigator)
// rounds - количество успешных этапов Пилота для победы
// pilot_interval_ms - как часто у Пилота меняется требуемая комбинация (например, 3000 мс)
// navigator_interval_ms - как часто у Штурмана вспыхивают новые мишени (например, 1000 мс — чаще)
// frequency_ms - частота опроса задачи (например, 50 мс)
static void run_pilot_navigator_game(uint8_t rounds, uint32_t pilot_interval_ms, uint32_t navigator_interval_ms, uint32_t frequency_ms) {
    ESP_LOGI(TAG, "Starting Game 14: Pilot & Navigator (Rounds: %u, Pilot int: %lums, Nav int: %lums)", 
             rounds, pilot_interval_ms, navigator_interval_ms);

    TickType_t last_wake_time = xTaskGetTickCount();
    const TickType_t frequency = pdMS_TO_TICKS(frequency_ms);

    typedef enum {
        PN_PLAYING,
        PN_GAME_OVER
    } pn_state_t;

    pn_state_t pn_state = PN_PLAYING;

    uint8_t pilot_rounds_completed = 0;
    uint32_t pilot_ticks_counter = 0;
    uint32_t nav_ticks_counter = 0;

    uint32_t pilot_limit_ticks = pilot_interval_ms / frequency_ms;
    uint32_t nav_limit_ticks = navigator_interval_ms / frequency_ms;

    // Целевые маски для левой (Пилот 0-4) и правой (Штурман 5-9) панелей
    uint16_t pilot_target_mask = generate_two_random_dots(0);
    uint16_t nav_target_mask = generate_two_random_dots(5);

    uint32_t pilot_fails = 0;
    uint32_t nav_successes = 0;

    bool last_button_state_pressed = false;

    while (current_game == GAME_STATE_PILOT_NAVIGATOR) {
        uint16_t pressed_buttons = read_arcade_buttons();

        switch (pn_state) {
            case PN_PLAYING: {
                pilot_ticks_counter++;
                nav_ticks_counter++;

                // 1. Таймер Штурмана (правая панель 5-9) срабатывает чаще
                if (nav_ticks_counter >= nav_limit_ticks) {
                    nav_ticks_counter = 0;
                    // Штурман получает новую комбинацию из 2 точек справа
                    nav_target_mask = generate_two_random_dots(5);
                    // ESP_LOGI(TAG, "Navigator target updated");
                }

                // 2. Таймер Пилота (левая панель 0-4)
                if (pilot_ticks_counter >= pilot_limit_ticks) {
                    pilot_ticks_counter = 0;

                    // Проверяем, удерживал ли игрок нужную комбинацию Пилота всё это время
                    uint16_t left_pressed = pressed_buttons & 0x001F; // Биты 0-4
                    if (left_pressed == pilot_target_mask) {
                        pilot_rounds_completed++;
                        ESP_LOGI(TAG, "Pilot stage success! (%u/%u)", pilot_rounds_completed, rounds);
                    } else {
                        pilot_fails++;
                        ESP_LOGW(TAG, "Pilot failed to hold balance! Fails: %u", pilot_fails);
                    }

                    // Проверка завершения игры по раундам Пилота
                    if (pilot_rounds_completed >= rounds) {
                        pn_state = PN_GAME_OVER;
                        break;
                    }

                    // Пилот получает новую задачу баланса слева
                    pilot_target_mask = generate_two_random_dots(0);
                }

                // 3. Интерактивная обработка Штурмана (если игрок нажал правую цель)
                uint16_t right_pressed = pressed_buttons & 0x03E0; // Биты 5-9
                if (right_pressed > 0 && !last_button_state_pressed) {
                    last_button_state_pressed = true;

                    if (right_pressed == nav_target_mask) {
                        nav_successes++;
                        // ESP_LOGI(TAG, "Navigator hit! Successes: %u", nav_successes);
                        // Штурман мгновенно получает новую задачу при успехе
                        nav_target_mask = generate_two_random_dots(5);
                        nav_ticks_counter = 0;
                    }
                } 
                else if (pressed_buttons == 0) {
                    last_button_state_pressed = false;
                }

                // 4. Объединяем вывод обеих задач на светодиоды
                // Левая часть (0-4) показывает задачу Пилота, правая (5-9) — вспышки Штурмана
                uint16_t combined_display_mask = pilot_target_mask | nav_target_mask;
                write_arcade_leds(combined_display_mask);

                break;
            }

            case PN_GAME_OVER: {
                ESP_LOGI(TAG, "=== GAME OVER ===");
                ESP_LOGI(TAG, "Pilot completed rounds: %u/%u (Fails: %u)", pilot_rounds_completed, rounds, pilot_fails);
                ESP_LOGI(TAG, "Navigator total hits: %u", nav_successes);

                write_arcade_leds(0x03FF);
                vTaskDelay(pdMS_TO_TICKS(GAME_OVER_TIMEOUT_MS));

                write_arcade_leds(0x0000);
                current_game = GAME_STATE_MENU;
                break;
            }
        }

        vTaskDelayUntil(&last_wake_time, frequency);
    }
}



// Главная задача диспетчер игр (работает на ядре 1)
void games_task(void *pvParameters) {
    (void)pvParameters; 
    ESP_LOGI(TAG, "Games task started on Core %d", xPortGetCoreID());
    led_strip_handle_t led_strip =onboard_led_init();
    
    led_strip_set_pixel(led_strip, 0, 123, 123, 123);
    led_strip_refresh(led_strip);
    

    while (1) {
        UpdateLed(led_strip); // Обновляем цвет светодиода на плате
        switch (current_game) {
            case GAME_STATE_MENU:
                // Ожидание выбора игры в меню
                vTaskDelay(pdMS_TO_TICKS(10));
                break;

            case GAME_STATE_NEIGHBOR_QUENCH:
                run_neighbor_quench_game(game_rounds_setting,50);
                break;
            case GAME_STATE_FLASH:
                // Запускаем на 15 раундов с периодом опроса логики в 10 мс (высокая точность таймера)
                run_flash_game(15, 10); 
                break;    
            case GAME_STATE_SIMON:
                // Запускаем Саймона до 20 уровней с тиком игрового цикла 50 мс
                run_simon_game(20, 50); 
                break;
            case GAME_STATE_WHACK_MOLE:
                // Раунд на 30 секунд, опрос логики каждые 50 мс
                run_whack_mole_game(30, 50); 
                break;
            case GAME_STATE_CANDLE_SHOOT:
                // Время до сгорания свечи 2000 мс (2 сек), общая длительность игры 30 секунд
                run_candle_shoot_game(2000, 30); 
                break;
            case GAME_STATE_SECRET_CODE:
                // Код из 4 элементов, за один раз игрок вводит 4 кнопки, игра идет 60 секунд, опрос каждые 50 мс
                run_secret_code_game(4, 4, 60, 50); 
                break;
            case GAME_STATE_TRAFFIC_LIGHT:
                // 10 раундов цепочек, таймаут нажатия 2 секунды, опрос каждые 50 мс
                run_traffic_light_game(10, 2, 50); 
                break;

            case GAME_STATE_MIRROR:
                run_mirror_game(15, 3, 50);
                break;
            case GAME_STATE_ELECTRIC_CIRCUIT:
                // 4 диода в паттерне, 60 секунд таймаут, опрос каждые 50 мс
                run_electric_circuit_game(4, 60, 50); 
                break;
            case GAME_STATE_WAVE_CHASE:
                // 5 раундов, скорость 25 тиков, ускорение на 3 тика, ширина волны = 2 диода
                run_wave_chase_game(5, 25, 3,2); 
                break;
            case GAME_STATE_PILOT_NAVIGATOR:
                // 10 раундов Пилота, смена задачи Пилота каждые 3000 мс, вспышки Штурмана каждые 1200 мс, опрос 50 мс
                run_pilot_navigator_game(10, 3000, 1200, 50); 
                break;
            default:
                current_game = GAME_STATE_MENU;
                break;
        }
    }
}

void UpdateLed(led_strip_handle_t led_strip ){
        
    static uint8_t red = 0;
    static uint8_t green = 85;
    static uint8_t blue = 170; // (исправим тип на uint8_t для простоты)

    // Увеличиваем каждый цвет с разной скоростью, чтобы они смешивались
    red += 1;
    green += 2;
    blue += 3;
    // Обновляем единственный светодиод на пине IO0
    led_strip_set_pixel(led_strip, 0, red, green, blue);
    led_strip_refresh(led_strip);

}