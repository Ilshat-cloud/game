#include "actions.h"
#include "esp_log.h"
#include "ui.h"

void action_game_start(lv_event_t * e){
    int game_id = (int)lv_event_get_user_data(e);
    //TODO получили номер игры и запустили соответствующую функцию игры
    ESP_LOGI("UI_ACTIONS", "Starting game with ID: %d", game_id);
    lv_scr_load(objects.game);

}
void action_change_screen_to_settings(lv_event_t * e){
    lv_scr_load(objects.settings);

}
void action_back_to_main_window(lv_event_t * e){
    lv_scr_load(objects.main);

}