#include "actions.h"
#include "esp_log.h"
#include "ui.h"
#include <string.h>
#include "vars.h"
#include "games.h"

extern game_state_t current_game;
extern game_settings_t game_settings; // Настройки игры по умолчанию
int game_id=0;
char legend_of_game[200] = { 0 };
char metrics[100] = { 0 };

const char *get_var_metrics() {
    return metrics;
}

void set_var_metrics(const char *value) {
    strncpy(metrics, value, sizeof(metrics) / sizeof(char));
    metrics[sizeof(metrics) / sizeof(char) - 1] = 0;
}

const char *get_var_legend_of_game() {
    return legend_of_game;
}

void set_var_legend_of_game(const char *value) {
    strncpy(legend_of_game, value, sizeof(legend_of_game) / sizeof(char));
    legend_of_game[sizeof(legend_of_game) / sizeof(char) - 1] = 0;
}

void action_game_start(lv_event_t * e){
    game_id = (int)lv_event_get_user_data(e);
    //TODO получили номер игры и запустили соответствующую функцию игры
    ESP_LOGI("UI_ACTIONS", "Starting game with ID: %d", game_id);
    // Закидываем описание в зависимости от ID игры через твою функцию set_var_legend_of_game
    switch (game_id) {
        case 2: // GAME_STATE_NEIGHBOR_QUENCH
            set_var_legend_of_game("Гашение соседей:\nГасите соседние ячейки, чтобы полностью выровнять игровое поле.");
            break;
        case 3: // GAME_STATE_FLASH
            set_var_legend_of_game("Вспышка:\nРеагируйте мгновенно на световую вспышку!");
            break;
        case 4: // GAME_STATE_SIMON
            set_var_legend_of_game("Память (Simon):\nВнимательно следите и повторяйте заученную комбинацию.");
            break;
        case 5: // GAME_STATE_WHACK_MOLE
            set_var_legend_of_game("Бей крота:\nУспевайте нажимать на загорающиеся элементы быстрее, чем они погаснут.");
            break;
        case 6: // GAME_STATE_CANDLE_SHOOT
            set_var_legend_of_game("Меняй свечу:\nУспейте заменить свечу на новую, пока старая полностью не прогорела!");
            break;
        case 7: // GAME_STATE_SECRET_CODE
            set_var_legend_of_game("Тайный код:\nВзломайте секретную комбинацию (Mastermind) за минимальное число попыток.");
            break;
        case 8: // GAME_STATE_MIRROR
            set_var_legend_of_game("Зеркальное отражение:\nПовторяйте зеркальные движения или световые паттерны в ответ.");
            break;
        case 9: // GAME_STATE_TRAFFIC_LIGHT
            set_var_legend_of_game("Светофор:\nНажимайте на зеленый свет и вовремя останавливайтесь на красный (Go/No-Go).");
            break;
        case 10: // GAME_STATE_ELECTRIC_CIRCUIT
            set_var_legend_of_game("Электрическая цепь:\nВыстройте правильный паттерн соединения для подачи питания.");
            break;
        case 11: // GAME_STATE_WAVE_CHASE
            set_var_legend_of_game("Погоня за волной:\nУспевайте вовремя следовать за быстро движущейся световой волной.");
            break;
        case 12: // GAME_STATE_PILOT_NAVIGATOR
            set_var_legend_of_game("Штурман и Пилот:\nИгра на идеальную координацию действий и скорость.");
            break;
        default:
            set_var_legend_of_game("Что-то пошло не так. Неизвестная игра. Нажмите назад, чтобы вернуться в меню.");
            break;
    }
    lv_label_set_text(objects.ui_label_legend, get_var_legend_of_game());
    lv_scr_load(objects.game);

}
void action_change_screen_to_settings(lv_event_t * e){
    lv_scr_load(objects.settings);

}
void action_back_to_main_window(lv_event_t * e){
    lv_scr_load(objects.main);

}

// Функция для кнопки "Старт" (вызывается из интерфейса)
void action_start_selected_game(lv_event_t * e) {
    (void)e;

    int32_t rounds_val     = lv_slider_get_value(objects.roun_cnt_slider);
    int32_t speed_val      = lv_slider_get_value(objects.speed_cnt_slider);
    int32_t diff_val       = lv_slider_get_value(objects.difficulty_cnt_slider);
    int32_t duration_val   = lv_slider_get_value(objects.duriation_cnt_slider);

    // Защита от отрицательных значений и приведение к типам структуры
    if (rounds_val < 1)   rounds_val = 1;
    if (speed_val < 1)    speed_val = 1;
    if (diff_val < 1)     diff_val = 1;
    if (duration_val < 1) duration_val = 1;

    // 1. Раунды (uint8_t — следим, чтобы не переполнить 255)
    game_settings.rounds = (rounds_val > 255) ? 255 : (uint8_t)rounds_val;

    // 2. Период опроса / скорость в мс (например, 0..100 -> 1000..50 мс: чем выше скорость, тем меньше период)
    // Если нужен просто прямой масштаб 0..100 -> 0..1000 мс, замени формулу на: (uint32_t)speed_val * 10
    game_settings.frequency_ms = 1000U - ((uint32_t)speed_val * 990U) / 100U;

    // 3. Длительность в секундах (например, 0..100 -> 0..120 сек)
    game_settings.game_duriation_sec = ((uint32_t)duration_val * 120U) / 100U;

    // 4. Оба уровня сложности меняются от difficulty_cnt_slider (0..100),
    // а внутри конкретной игры уже трактуются по-своему

    uint32_t d = (uint32_t)diff_val; // 0..100

    switch (game_id) {
        case GAME_STATE_CANDLE_SHOOT:
            // diff1: время до сгорания свечи в мс (например, от 3000 мс на 0 до 800 мс на 100)
            game_settings.difficulty_level1 = 3000U - (d * 2200U) / 100U;
            break;

        case GAME_STATE_SECRET_CODE:
            // diff1: длина кода / число элементов (например, 2..5 элементов)
            game_settings.difficulty_level1 = 2U + (d * 3U) / 100U;
            // diff2: макс. число попыток или сложность подсказок (например, 8..2 попыток)
            game_settings.difficulty_level2 = 8U - (d * 6U) / 100U;
            break;

        case GAME_STATE_TRAFFIC_LIGHT:
            // diff1: таймаут реакции в мс (например, от 2500 мс на 0 до 700 мс на 100)
            game_settings.difficulty_level1 = 2500U - (d * 1800U) / 100U;
            break;

        case GAME_STATE_MIRROR:
            // diff1: количество активных элементов / длина паттерна (например, 2..6)
            game_settings.difficulty_level1 = 2U + (d * 3U) / 100U;
            break;

        case GAME_STATE_ELECTRIC_CIRCUIT:
            // diff1: число диодов / узлов в паттерне (например, 3..7)
            game_settings.difficulty_level1 = 3U + (d * 4U) / 100U;
            break;

        case GAME_STATE_WAVE_CHASE:
            // diff1: шаг ускорения волны в тиках (например, 1..6 тиков)
            game_settings.difficulty_level1 = 1U + (d * 5U) / 100U;
            // diff2: ширина волны в диодах (например, от 3 на легком до 1 на сложном)
            game_settings.difficulty_level2 = 3U - (d * 2U) / 100U;
            break;

        case GAME_STATE_PILOT_NAVIGATOR:
            // diff1: интервал смены задачи Пилота в мс (от 4000 мс до 1500 мс)
            game_settings.difficulty_level1 = 4000U - (d * 2500U) / 100U;
            // diff2: интервал вспышек Штурмана в мс (от 1800 мс до 600 мс)
            game_settings.difficulty_level2 = 1800U - (d * 1200U) / 100U;
            break;

        default:
            // Для игр без доп. параметров сложности (NEIGHBOR_QUENCH, FLASH, SIMON, WHACK_MOLE)
            game_settings.difficulty_level1 = d;
            game_settings.difficulty_level2 = d;
            break;
    }
    ESP_LOGI("UI_ACTIONS",
             "Game ID %d settings saved: rounds=%u, freq_ms=%lu, dur_sec=%lu, diff1=%lu, diff2=%lu",
             game_id,
             game_settings.rounds,
             (unsigned long)game_settings.frequency_ms,
             (unsigned long)game_settings.game_duriation_sec,
             (unsigned long)game_settings.difficulty_level1,
             (unsigned long)game_settings.difficulty_level2);

    current_game = (game_state_t)game_id;
    lv_obj_clear_state(objects.btn_stop, LV_STATE_DISABLED);
    lv_obj_add_state(objects.btn_start, LV_STATE_DISABLED);
    //TODO после gmae over разблокировать кнопку старт и заблокировать кнопку стоп
}

// Функция для кнопки "Стоп / Пауза" (вызывается из интерфейса)
void action_stop_current_game(lv_event_t * e) {
    //Возвращаем в меню ожидания
    current_game = GAME_STATE_MENU; 
    lv_obj_clear_state(objects.btn_start, LV_STATE_DISABLED);
    lv_obj_add_state(objects.btn_stop, LV_STATE_DISABLED);
}