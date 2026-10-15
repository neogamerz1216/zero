#ifndef GUI_H
#define GUI_H

#include <stdint.h>

#define GUI_WIDTH  80
#define GUI_HEIGHT 25

/* GUI palette (VGA text attributes) */
#define GUI_BG         COLOR_LIGHT_BLUE
#define GUI_TITLE_BAR  COLOR_LIGHT_CYAN
#define GUI_TITLE_TEXT COLOR_BLACK
#define GUI_BORDER     COLOR_DARK_GREY
#define GUI_WINDOW_BG  COLOR_LIGHT_GREY
#define GUI_LABEL_TEXT COLOR_BLACK
#define GUI_BUTTON_BG  COLOR_LIGHT_GREY
#define GUI_BUTTON_TEXT COLOR_BLACK
#define GUI_BUTTON_FOCUS COLOR_WHITE
#define GUI_DESKTOP_TEXT COLOR_WHITE

typedef struct gui_widget gui_widget_t;

typedef void (*gui_widget_draw_fn)(gui_widget_t* w, int x, int y);
typedef int  (*gui_widget_key_fn)(gui_widget_t* w, char c);

struct gui_widget {
    int x, y;            /* position relative to window content */
    int w, h;
    int focusable;
    int focused;
    gui_widget_draw_fn draw;
    gui_widget_key_fn  key;
    char label[32];
    void* data;
    gui_widget_t* next;
};

typedef struct {
    int x, y;            /* window top-left on screen */
    int w, h;
    char title[32];
    int visible;
    int focused;
    gui_widget_t* widgets;
    gui_widget_t* focus;
} gui_window_t;

typedef struct {
    char title[64];
    gui_window_t* windows;
    int window_count;
    int focus_win;
} gui_desktop_t;

/* Launch the ZeroGUI desktop. Returns when the user presses ESC. */
void gui_desktop(void);

/* Widget helpers */
gui_widget_t* gui_label_new(int x, int y, const char* text);
gui_widget_t* gui_button_new(int x, int y, int w, const char* text);
void gui_window_add(gui_window_t* win, gui_widget_t* w);

#endif
