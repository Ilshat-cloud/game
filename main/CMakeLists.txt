file(GLOB_RECURSE UI_SRCS "ui/*.c" "ui/*.cpp")

idf_component_register(
    SRCS "main.c" "games.c" ${UI_SRCS}
    INCLUDE_DIRS "." "ui"
    PRIV_REQUIRES lvgl__lvgl
)
