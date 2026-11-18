#ifndef EEZ_LVGL_UI_VARS_H
#define EEZ_LVGL_UI_VARS_H

#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// enum declarations

// Flow global variables

enum FlowGlobalVariables {
    FLOW_GLOBAL_VARIABLE_LEGEND_OF_GAME = 0,
    FLOW_GLOBAL_VARIABLE_METRICS = 1
};

// Native global variables

extern const char *get_var_legend_of_game();
extern void set_var_legend_of_game(const char *value);
extern const char *get_var_metrics();
extern void set_var_metrics(const char *value);

#ifdef __cplusplus
}
#endif

#endif /*EEZ_LVGL_UI_VARS_H*/