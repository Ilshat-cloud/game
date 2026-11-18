#include "actions.h"
#include "esp_log.h"
#include "ui.h"
#include <string.h>
#include "vars.h"

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
    int game_id = (int)lv_event_get_user_data(e);
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
    // Считываем значение со слайдера раундов
    // int rounds_value = lv_slider_get_value(ui_slider_rounds); 
    // game_rounds_setting = rounds_value; 
    
    // // Запускаем игру на Core 1
    // current_game = selected_game_id; 
}

// Функция для кнопки "Стоп / Пауза" (вызывается из интерфейса)
void action_stop_current_game(lv_event_t * e) {
    // // Возвращаем в меню ожидания
    // current_game = GAME_STATE_MENU; 
}