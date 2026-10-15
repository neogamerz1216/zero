#include "gui.h"
#include "../drivers/vga.h"
#include "../drivers/keyboard.h"
#include "../drivers/mouse.h"
#include "../drivers/timer.h"
#include "../drivers/rtc.h"
#include "../drivers/net.h"
#include <stdint.h>

/* ----- static desktop state ----- */

static gui_desktop_t desktop;
static int gui_running = 1;
static gui_widget_t widget_pool[32];
static int widget_pool_count;

/* ----- primitive drawing ----- */

static void gui_fill_rect(int x, int y, int w, int h, uint8_t bg, char ch) {
    for (int cy = y; cy < y + h && cy < GUI_HEIGHT; cy++) {
        for (int cx = x; cx < x + w && cx < GUI_WIDTH; cx++) {
            VGA_MEMORY[cy * GUI_WIDTH + cx] = (uint16_t)ch | ((uint16_t)bg << 12);
        }
    }
}

static void gui_draw_text(int x, int y, const char* text, uint8_t fg, uint8_t bg) {
    if (y < 0 || y >= GUI_HEIGHT) return;
    int attr = ((uint8_t)bg << 4) | (uint8_t)fg;
    for (int i = 0; text[i] && x + i < GUI_WIDTH; i++) {
        if (x + i >= 0) {
            VGA_MEMORY[y * GUI_WIDTH + x + i] = (uint16_t)text[i] | ((uint16_t)attr << 8);
        }
    }
}

static void gui_draw_border(gui_window_t* win) {
    /* outer border */
    for (int cx = win->x; cx < win->x + win->w && cx < GUI_WIDTH; cx++) {
        VGA_MEMORY[win->y * GUI_WIDTH + cx] = (uint16_t)'#' | ((uint16_t)GUI_BORDER << 8);
        VGA_MEMORY[(win->y + win->h - 1) * GUI_WIDTH + cx] =
            (uint16_t)'#' | ((uint16_t)GUI_BORDER << 8);
    }
    for (int cy = win->y; cy < win->y + win->h && cy < GUI_HEIGHT; cy++) {
        VGA_MEMORY[cy * GUI_WIDTH + win->x] = (uint16_t)'#' | ((uint16_t)GUI_BORDER << 8);
        VGA_MEMORY[cy * GUI_WIDTH + win->x + win->w - 1] =
            (uint16_t)'#' | ((uint16_t)GUI_BORDER << 8);
    }
    /* title bar */
    gui_fill_rect(win->x + 1, win->y + 1, win->w - 2, 1, GUI_TITLE_BAR, ' ');
    gui_draw_text(win->x + 2, win->y + 1, win->title, GUI_TITLE_TEXT, GUI_TITLE_BAR);
    /* window background */
    gui_fill_rect(win->x + 1, win->y + 2, win->w - 2, win->h - 3, GUI_WINDOW_BG, ' ');
}

/* ----- widgets ----- */

static void gui_label_draw(gui_widget_t* w, int x, int y) {
    int attr = ((uint8_t)GUI_WINDOW_BG << 4) | (uint8_t)GUI_LABEL_TEXT;
    for (int i = 0; w->label[i] && x + w->x + i < GUI_WIDTH; i++) {
        int px = x + w->x + i;
        int py = y + w->y;
        if (px >= 0 && py >= 0 && py < GUI_HEIGHT) {
            VGA_MEMORY[py * GUI_WIDTH + px] = (uint16_t)w->label[i] | ((uint16_t)attr << 8);
        }
    }
}
static int gui_label_key(gui_widget_t* w, char c) {
    (void)w; (void)c;
    return 0;
}

static void gui_button_draw(gui_widget_t* w, int x, int y) {
    uint8_t bg = w->focused ? GUI_BUTTON_FOCUS : GUI_BUTTON_BG;
    uint8_t fg = GUI_BUTTON_TEXT;
    int attr = ((uint8_t)bg << 4) | (uint8_t)fg;
    /* button face */
    int px = x + w->x;
    int py = y + w->y;
    for (int cx = px; cx < px + w->w && cx < GUI_WIDTH; cx++) {
        if (cx >= 0 && py >= 0 && py < GUI_HEIGHT) {
            VGA_MEMORY[py * GUI_WIDTH + cx] = (uint16_t)' ' | ((uint16_t)attr << 8);
        }
    }
    /* center text */
    int tlen = 0;
    while (w->label[tlen]) tlen++;
    int tx = px + (w->w - tlen) / 2;
    if (tx < px) tx = px;
    for (int i = 0; w->label[i] && tx + i < px + w->w; i++) {
        if (tx + i >= 0 && py >= 0 && py < GUI_HEIGHT) {
            VGA_MEMORY[py * GUI_WIDTH + tx + i] = (uint16_t)w->label[i] | ((uint16_t)attr << 8);
        }
    }
}
static int gui_button_key(gui_widget_t* w, char c) {
    if (c == '\n' || c == '\r' || c == ' ') {
        vga_puts("\n  >> Button activated: ", COLOR_LIGHT_CYAN);
        vga_puts(w->label, COLOR_LIGHT_GREEN);
        vga_putc('\n', COLOR_WHITE);
        return 1;
    }
    return 0;
}

gui_widget_t* gui_label_new(int x, int y, const char* text) {
    if (widget_pool_count >= 32) return 0;
    gui_widget_t* w = &widget_pool[widget_pool_count++];
    w->x = x; w->y = y; w->w = 0; w->h = 1;
    w->focusable = 0; w->focused = 0;
    w->draw = gui_label_draw; w->key = gui_label_key;
    for (int i = 0; i < 31 && text[i]; i++) w->label[i] = text[i];
    w->label[31] = 0;
    w->data = 0; w->next = 0;
    return w;
}

gui_widget_t* gui_button_new(int x, int y, int wd, const char* text) {
    if (widget_pool_count >= 32) return 0;
    gui_widget_t* w = &widget_pool[widget_pool_count++];
    w->x = x; w->y = y; w->w = wd; w->h = 1;
    w->focusable = 1; w->focused = 0;
    w->draw = gui_button_draw; w->key = gui_button_key;
    for (int i = 0; i < 31 && text[i]; i++) w->label[i] = text[i];
    w->label[31] = 0;
    w->data = 0; w->next = 0;
    return w;
}

void gui_window_add(gui_window_t* win, gui_widget_t* w) {
    w->next = win->widgets;
    win->widgets = w;
    if (!win->focus && w->focusable) {
        win->focus = w;
        w->focused = 1;
    }
}

/* ----- window drawing ----- */

static void gui_window_draw(gui_window_t* win) {
    if (!win->visible) return;
    gui_draw_border(win);
    gui_widget_t* w = win->widgets;
    while (w) {
        w->draw(w, win->x + 1, win->y + 2);
        w = w->next;
    }
}

/* ----- desktop ----- */

static void gui_desktop_draw(void) {
    gui_fill_rect(0, 0, GUI_WIDTH, GUI_HEIGHT, GUI_BG, ' ');
    /* desktop title banner */
    gui_draw_text(2, 0, "ZeroGUI Desktop  |  ZeroOS Native Windowing System", COLOR_WHITE, GUI_BG);
    gui_draw_text(2, 1, "TAB=focus  ENTER=activate  Mouse=click  ESC=shell",
                  COLOR_LIGHT_GREY, GUI_BG);
    /* clock */
    rtc_time_t t = rtc_get_time();
    /* build "HH:MM:SS" */
    char tmp[9];
    tmp[0] = '0' + t.hour / 10; tmp[1] = '0' + t.hour % 10; tmp[2] = ':';
    tmp[3] = '0' + t.minute / 10; tmp[4] = '0' + t.minute % 10; tmp[5] = ':';
    tmp[6] = '0' + t.second / 10; tmp[7] = '0' + t.second % 10; tmp[8] = 0;
    gui_draw_text(GUI_WIDTH - 12, 1, tmp, COLOR_LIGHT_GREEN, GUI_BG);
    /* draw windows */
    for (int i = 0; i < desktop.window_count; i++) {
        gui_window_draw(&desktop.windows[i]);
    }
    gui_fill_rect(0, GUI_HEIGHT - 1, GUI_WIDTH, 1, COLOR_DARK_GREY, ' ');
    gui_draw_text(1, GUI_HEIGHT - 1, "ZeroOS", COLOR_WHITE, COLOR_DARK_GREY);
    gui_draw_text(10, GUI_HEIGHT - 1, "Mouse: click windows and buttons", COLOR_LIGHT_GREY, COLOR_DARK_GREY);
    if (desktop.focus_win >= 0 && desktop.focus_win < desktop.window_count) {
        gui_draw_text(57, GUI_HEIGHT - 1, desktop.windows[desktop.focus_win].title,
                      COLOR_LIGHT_CYAN, COLOR_DARK_GREY);
    }
    int mouse_x, mouse_y;
    mouse_get_state(&mouse_x, &mouse_y, 0);
    VGA_MEMORY[mouse_y * GUI_WIDTH + mouse_x] = (uint16_t)'X' |
        ((uint16_t)((COLOR_BLACK << 4) | COLOR_WHITE) << 8);
}

static void gui_desktop_build(void) {
    widget_pool_count = 0;
    desktop.window_count = 0;
    desktop.focus_win = 0;

    /* Window 1: Welcome */
    gui_window_t* w1 = &desktop.windows[desktop.window_count++];
    w1->x = 10; w1->y = 4; w1->w = 44; w1->h = 9;
    w1->visible = 1; w1->focused = 1;
    w1->widgets = 0; w1->focus = 0;
    for (int i = 0; i < 15 && "Welcome to ZeroGUI"[i]; i++) w1->title[i] = "Welcome to ZeroGUI"[i];
    w1->title[15] = 0;
    gui_window_add(w1, gui_label_new(2, 2, "Welcome to ZeroOS ZeroGUI."));
    gui_window_add(w1, gui_label_new(2, 3, "A native text-mode windowing system."));
    gui_window_add(w1, gui_label_new(2, 4, "Not Linux, not Unix - ZeroOS."));
    gui_window_add(w1, gui_button_new(2, 6, 14, " About "));

    /* Window 2: System */
    gui_window_t* w2 = &desktop.windows[desktop.window_count++];
    w2->x = 58; w2->y = 4; w2->w = 22; w2->h = 9;
    w2->visible = 1; w2->focused = 0;
    w2->widgets = 0; w2->focus = 0;
    for (int i = 0; i < 12 && "System Info"[i]; i++) w2->title[i] = "System Info"[i];
    w2->title[12] = 0;
    gui_window_add(w2, gui_label_new(1, 2, "x86_64 Long Mode"));
    gui_window_add(w2, gui_label_new(1, 3, "Ring 0 Supervised"));
    gui_window_add(w2, gui_label_new(1, 4, "ZeroFS + ZeroNet"));
    gui_window_add(w2, gui_button_new(1, 6, 10, " Refresh "));

    /* Window 3: Network */
    gui_window_t* w3 = &desktop.windows[desktop.window_count++];
    w3->x = 10; w3->y = 14; w3->w = 44; w3->h = 8;
    w3->visible = 1; w3->focused = 0;
    w3->widgets = 0; w3->focus = 0;
    for (int i = 0; i < 16 && "Network Status"[i]; i++) w3->title[i] = "Network Status"[i];
    w3->title[16] = 0;
    gui_window_add(w3, gui_label_new(2, 2, "lo:   127.0.0.1  [SIM]"));
    gui_window_add(w3, gui_label_new(2, 3, "vnet: 10.0.0.2   [SIM]"));
    gui_window_add(w3, gui_label_new(2, 4, "vnet: no hardware link"));
    gui_window_add(w3, gui_button_new(2, 6, 12, " netstat "));
}

static int gui_widget_invoke_key(gui_widget_t* w, char c) {
    if (w->key) return w->key(w, c);
    return 0;
}

static void gui_mouse_click(int mouse_x, int mouse_y) {
    for (int i = desktop.window_count - 1; i >= 0; i--) {
        gui_window_t* win = &desktop.windows[i];
        if (!win->visible || mouse_x < win->x || mouse_x >= win->x + win->w ||
            mouse_y < win->y || mouse_y >= win->y + win->h) continue;

        desktop.focus_win = i;
        for (gui_widget_t* w = win->widgets; w; w = w->next) {
            w->focused = 0;
            int left = win->x + 1 + w->x;
            int top = win->y + 2 + w->y;
            if (w->focusable && mouse_x >= left && mouse_x < left + w->w &&
                mouse_y >= top && mouse_y < top + w->h) {
                w->focused = 1;
                win->focus = w;
                gui_widget_invoke_key(w, '\n');
                break;
            }
        }
        return;
    }
}

static void gui_cycle_focus(int forward) {
    /* find current focused widget across all windows */
    gui_widget_t* cur = 0;
    if (desktop.focus_win >= 0 && desktop.focus_win < desktop.window_count) {
        cur = desktop.windows[desktop.focus_win].focus;
    }
    /* gather focusable widgets of current window */
    gui_window_t* win = 0;
    if (desktop.focus_win >= 0 && desktop.focus_win < desktop.window_count)
        win = &desktop.windows[desktop.focus_win];

    if (!win) return;

    /* count and list focusable widgets */
    static gui_widget_t* foc[32];
    int n = 0;
    gui_widget_t* w = win->widgets;
    while (w && n < 32) {
        if (w->focusable) foc[n++] = w;
        w = w->next;
    }
    if (n == 0) return;

    int idx = -1;
    for (int i = 0; i < n; i++) {
        if (foc[i] == cur) { idx = i; break; }
    }
    if (forward) idx = (idx + 1) % n;
    else idx = (idx + n - 1) % n;
    if (idx < 0) idx = 0;

    /* unfocus all in this window */
    w = win->widgets;
    while (w) { w->focused = 0; w = w->next; }
    foc[idx]->focused = 1;
    win->focus = foc[idx];
}

void gui_desktop(void) {
    gui_running = 1;
    vga_clear(GUI_BG);
    vga_enable_cursor(0, 0); /* hide cursor during GUI */
    gui_desktop_build();
    uint8_t previous_buttons = 0;

    while (gui_running) {
        gui_desktop_draw();
        int mouse_x, mouse_y;
        uint8_t buttons;
        mouse_get_state(&mouse_x, &mouse_y, &buttons);
        if ((buttons & 1) && !(previous_buttons & 1)) {
            gui_mouse_click(mouse_x, mouse_y);
        }
        previous_buttons = buttons;

        char c;
        if (!keyboard_try_getchar(&c)) {
            timer_sleep_ms(16);
            continue;
        }
        switch (c) {
        case 27:  /* ESC */
            gui_running = 0;
            break;
        case '\t': /* TAB */
            gui_cycle_focus(1);
            break;
        case '\b': /* Shift-TAB-ish (backspace) */
            gui_cycle_focus(0);
            break;
        case '\n':
        case '\r':
        case ' ': {
            /* activate focused widget */
            if (desktop.focus_win >= 0 && desktop.focus_win < desktop.window_count) {
                gui_window_t* win = &desktop.windows[desktop.focus_win];
                if (win->focus) gui_widget_invoke_key(win->focus, c);
            }
            break;
        }
        default:
            /* pass alphanumeric to focused widget */
            if (desktop.focus_win >= 0 && desktop.focus_win < desktop.window_count) {
                gui_window_t* win = &desktop.windows[desktop.focus_win];
                if (win->focus) gui_widget_invoke_key(win->focus, c);
            }
            break;
        }
        timer_sleep_ms(30);
    }

    vga_clear(COLOR_BLACK);
    vga_enable_cursor(14, 15);
    vga_set_cursor(0, 0);
}
