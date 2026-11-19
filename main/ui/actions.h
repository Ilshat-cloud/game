#ifndef EEZ_LVGL_UI_EVENTS_H
#define EEZ_LVGL_UI_EVENTS_H

#include <lvgl.h>

#ifdef __cplusplus
extern "C" {
#endif

extern void action_game_start(lv_event_t * e);
extern void action_change_screen_to_settings(lv_event_t * e);
extern void action_back_to_main_window(lv_event_t * e);
extern void action_round_cnt_changed(lv_event_t * e);
extern void action_start_selected_game(lv_event_t * e);
extern void action_stop_current_game(lv_event_t * e);


void set_var_metrics(const char *value);
const char *get_var_metrics(void);
#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_EVENTS_H*/