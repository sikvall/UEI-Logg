/**
 * @file tui.c
 * @brief Terminal User Interface (TUI) med ncurses, färgscheman (inkl. gråskala och ambra),
 * rymdvädersbox från hf.avitec.se, loggradshantering och hjälpskärmar.
 */

#include "tui.h"
#include "dxcc.h"
#include "logg.h"
#include "qrb.h"
#include "space_weather.h"
#include "util.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Färgscheman inspirerade av Borland Turbo Pascal, Midnight Commander, ren gråskala och ambra */
const char * const THEME_NAMES[] = {
    "Turbo Pascal (MC Default)",
    "MC Darkfar (Svart/Blå)",
    "MC Gotar (Svart/Gul)",
    "MC Sand (Ljusgrå/Blå)",
    "MC Matrix (Fosforgrön)",
    "Ljust (Grå/Orange/Blå)",
    "Monokrom gråskala",
    "Ambra (Retro CRT)"
};

/**
 * @brief Initierar curses-läget, konfigurerar tangentbord och definierar specialtangenter.
 */
void tui_init(void) {
    initscr();
    set_escdelay(25);
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    define_key("\033[1;5H", KEY_CTRL_HOME);
    define_key("\033[1;5F", KEY_CTRL_END);
    define_key("\033[1;6H", KEY_CTRL_HOME);
    define_key("\033[1;6F", KEY_CTRL_END);
    define_key("\033[7^",   KEY_CTRL_HOME);
    define_key("\033[8^",   KEY_CTRL_END);
    define_key("\033[7;5~", KEY_CTRL_HOME);
    define_key("\033[8;5~", KEY_CTRL_END);
    define_key("\033[H",    KEY_HOME);
    define_key("\033[F",    KEY_END);
    define_key("\033OH",    KEY_HOME);
    define_key("\033OF",    KEY_END);
    define_key("\033[1~",   KEY_HOME);
    define_key("\033[4~",   KEY_END);
    define_key("\033[5~",   KEY_PPAGE);
    define_key("\033[6~",   KEY_NPAGE);
    timeout(250);

    /* Sätt terminalens fönstertitel om terminalemulatorn stöder OSC */
    printf("\033]0;UEI-log v1.0\007");
    fflush(stdout);
}

/**
 * @brief Avslutar ncurses och återställer terminalen till normalt läge.
 */
void tui_cleanup(void) {
    endwin();
}

/**
 * @brief Aktiverar och konfigurerar ett av de 8 tillgängliga färgschemana i ncurses.
 * Inkluderar modern gråskala och retro ambra-skärm.
 * @param theme_idx Index på schemat (0..THEME_COUNT-1).
 */
void apply_color_theme(int theme_idx) {
    if (!has_colors()) return;
    start_color();

    if (theme_idx < 0 || theme_idx >= THEME_COUNT) {
        theme_idx = 0;
    }

    switch (theme_idx) {
        case 0: /* 1. Turbo Pascal / MC Default (Klassisk blå bakgrund) */
            init_pair(CP_MAIN_BG,        COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_TOPBAR,         COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_TOPBAR_ACCENT,  COLOR_RED,    COLOR_WHITE);
            init_pair(CP_BORDER,         COLOR_CYAN,   COLOR_BLUE);
            init_pair(CP_BORDER_ACTIVE,  COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_TITLE,          COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_LABEL,          COLOR_CYAN,   COLOR_BLUE);
            init_pair(CP_FIELD_INACTIVE, COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_FIELD_ACTIVE,   COLOR_BLACK,  COLOR_CYAN);
            init_pair(CP_FROZEN_BADGE,   COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_LIVE_BADGE,     COLOR_WHITE,  COLOR_GREEN);
            init_pair(CP_EDIT_BADGE,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_TABLE_HEADER,   COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_LOG_ROW,        COLOR_CYAN,   COLOR_BLUE);
            init_pair(CP_LOG_SELECTED,   COLOR_BLACK,  COLOR_CYAN);
            init_pair(CP_LOG_MARKED,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_FKEY_NUM,       COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_FKEY_LABEL,     COLOR_BLACK,  COLOR_CYAN);
            init_pair(CP_STATUS_OK,      COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_STATUS_WARN,    COLOR_YELLOW, COLOR_RED);
            init_pair(CP_FLAG_NEW,         COLOR_RED,    COLOR_BLUE);
            init_pair(CP_FLAG_HANDLED,     COLOR_GREEN,  COLOR_BLUE);
            init_pair(CP_FLAG_NEW_SEL,     COLOR_RED,    COLOR_CYAN);
            init_pair(CP_FLAG_HANDLED_SEL, COLOR_GREEN,  COLOR_CYAN);
            init_pair(CP_FLAG_NEW_MRK,     COLOR_RED,    COLOR_MAGENTA);
            init_pair(CP_FLAG_HANDLED_MRK, COLOR_GREEN,  COLOR_MAGENTA);
            init_pair(CP_SW_NORMAL,        COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_SW_OK,            COLOR_GREEN,  COLOR_BLUE);
            init_pair(CP_SW_WARN,          COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_SW_BAD,           COLOR_RED,    COLOR_BLUE);
            init_pair(CP_SW_SEVERE,        COLOR_MAGENTA,COLOR_BLUE);
            break;

        case 1: /* 2. MC Darkfar (Svart bakgrund med blå accenter och gråa ramar) */
            init_pair(CP_MAIN_BG,        COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_TOPBAR,         COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_TOPBAR_ACCENT,  COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_BORDER,         COLOR_BLUE,   COLOR_BLACK);
            init_pair(CP_BORDER_ACTIVE,  COLOR_CYAN,   COLOR_BLACK);
            init_pair(CP_TITLE,          COLOR_CYAN,   COLOR_BLACK);
            init_pair(CP_LABEL,          COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_FIELD_INACTIVE, COLOR_CYAN,   COLOR_BLACK);
            init_pair(CP_FIELD_ACTIVE,   COLOR_BLACK,  COLOR_CYAN);
            init_pair(CP_FROZEN_BADGE,   COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_LIVE_BADGE,     COLOR_WHITE,  COLOR_GREEN);
            init_pair(CP_EDIT_BADGE,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_TABLE_HEADER,   COLOR_BLUE,   COLOR_BLACK);
            init_pair(CP_LOG_ROW,        COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_LOG_SELECTED,   COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_LOG_MARKED,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_FKEY_NUM,       COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_FKEY_LABEL,     COLOR_BLACK,  COLOR_CYAN);
            init_pair(CP_STATUS_OK,      COLOR_CYAN,   COLOR_BLACK);
            init_pair(CP_STATUS_WARN,    COLOR_WHITE,  COLOR_RED);
            init_pair(CP_FLAG_NEW,         COLOR_RED,    COLOR_BLACK);
            init_pair(CP_FLAG_HANDLED,     COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_FLAG_NEW_SEL,     COLOR_RED,    COLOR_BLUE);
            init_pair(CP_FLAG_HANDLED_SEL, COLOR_GREEN,  COLOR_BLUE);
            init_pair(CP_FLAG_NEW_MRK,     COLOR_RED,    COLOR_MAGENTA);
            init_pair(CP_FLAG_HANDLED_MRK, COLOR_GREEN,  COLOR_MAGENTA);
            init_pair(CP_SW_NORMAL,        COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_SW_OK,            COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_SW_WARN,          COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_SW_BAD,           COLOR_RED,    COLOR_BLACK);
            init_pair(CP_SW_SEVERE,        COLOR_MAGENTA,COLOR_BLACK);
            break;

        case 2: /* 3. MC Gotar (Svart bakgrund med gula detaljer och gråa ramar) */
            init_pair(CP_MAIN_BG,        COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_TOPBAR,         COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_TOPBAR_ACCENT,  COLOR_RED,    COLOR_YELLOW);
            init_pair(CP_BORDER,         COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_BORDER_ACTIVE,  COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_TITLE,          COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_LABEL,          COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_FIELD_INACTIVE, COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_FIELD_ACTIVE,   COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_FROZEN_BADGE,   COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_LIVE_BADGE,     COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_EDIT_BADGE,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_TABLE_HEADER,   COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_LOG_ROW,        COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_LOG_SELECTED,   COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_LOG_MARKED,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_FKEY_NUM,       COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_FKEY_LABEL,     COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_STATUS_OK,      COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_STATUS_WARN,    COLOR_WHITE,  COLOR_RED);
            init_pair(CP_FLAG_NEW,         COLOR_RED,    COLOR_BLACK);
            init_pair(CP_FLAG_HANDLED,     COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_FLAG_NEW_SEL,     COLOR_RED,    COLOR_YELLOW);
            init_pair(CP_FLAG_HANDLED_SEL, COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_FLAG_NEW_MRK,     COLOR_RED,    COLOR_MAGENTA);
            init_pair(CP_FLAG_HANDLED_MRK, COLOR_BLACK,  COLOR_MAGENTA);
            init_pair(CP_SW_NORMAL,        COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_SW_OK,            COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_SW_WARN,          COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_SW_BAD,           COLOR_RED,    COLOR_BLACK);
            init_pair(CP_SW_SEVERE,        COLOR_MAGENTA,COLOR_BLACK);
            break;

        case 3: /* 4. MC Sand / Gray (Ljusgrå/vit bakgrund med blå och svarta accenter) */
            init_pair(CP_MAIN_BG,        COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_TOPBAR,         COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_TOPBAR_ACCENT,  COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_BORDER,         COLOR_BLUE,   COLOR_WHITE);
            init_pair(CP_BORDER_ACTIVE,  COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_TITLE,          COLOR_BLUE,   COLOR_WHITE);
            init_pair(CP_LABEL,          COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_FIELD_INACTIVE, COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_FIELD_ACTIVE,   COLOR_BLUE,   COLOR_WHITE);
            init_pair(CP_FROZEN_BADGE,   COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_LIVE_BADGE,     COLOR_WHITE,  COLOR_GREEN);
            init_pair(CP_EDIT_BADGE,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_TABLE_HEADER,   COLOR_BLUE,   COLOR_WHITE);
            init_pair(CP_LOG_ROW,        COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_LOG_SELECTED,   COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_LOG_MARKED,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_FKEY_NUM,       COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_FKEY_LABEL,     COLOR_WHITE,  COLOR_CYAN);
            init_pair(CP_STATUS_OK,      COLOR_BLUE,   COLOR_WHITE);
            init_pair(CP_STATUS_WARN,    COLOR_WHITE,  COLOR_RED);
            init_pair(CP_FLAG_NEW,         COLOR_RED,    COLOR_WHITE);
            init_pair(CP_FLAG_HANDLED,     COLOR_GREEN,  COLOR_WHITE);
            init_pair(CP_FLAG_NEW_SEL,     COLOR_RED,    COLOR_BLUE);
            init_pair(CP_FLAG_HANDLED_SEL, COLOR_GREEN,  COLOR_BLUE);
            init_pair(CP_FLAG_NEW_MRK,     COLOR_RED,    COLOR_MAGENTA);
            init_pair(CP_FLAG_HANDLED_MRK, COLOR_GREEN,  COLOR_MAGENTA);
            init_pair(CP_SW_NORMAL,        COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_SW_OK,            COLOR_GREEN,  COLOR_WHITE);
            init_pair(CP_SW_WARN,          COLOR_YELLOW, COLOR_WHITE);
            init_pair(CP_SW_BAD,           COLOR_RED,    COLOR_WHITE);
            init_pair(CP_SW_SEVERE,        COLOR_MAGENTA,COLOR_WHITE);
            break;

        case 4: /* 5. MC Matrix (Klassisk fosforgrön terminalskärm på svart bakgrund) */
            init_pair(CP_MAIN_BG,        COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_TOPBAR,         COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_TOPBAR_ACCENT,  COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_BORDER,         COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_BORDER_ACTIVE,  COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_TITLE,          COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_LABEL,          COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_FIELD_INACTIVE, COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_FIELD_ACTIVE,   COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_FROZEN_BADGE,   COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_LIVE_BADGE,     COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_EDIT_BADGE,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_TABLE_HEADER,   COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_LOG_ROW,        COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_LOG_SELECTED,   COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_LOG_MARKED,     COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_FKEY_NUM,       COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_FKEY_LABEL,     COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_STATUS_OK,      COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_STATUS_WARN,    COLOR_WHITE,  COLOR_RED);
            init_pair(CP_FLAG_NEW,         COLOR_RED,    COLOR_BLACK);
            init_pair(CP_FLAG_HANDLED,     COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_FLAG_NEW_SEL,     COLOR_RED,    COLOR_GREEN);
            init_pair(CP_FLAG_HANDLED_SEL, COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_FLAG_NEW_MRK,     COLOR_RED,    COLOR_GREEN);
            init_pair(CP_FLAG_HANDLED_MRK, COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_SW_NORMAL,        COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_SW_OK,            COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_SW_WARN,          COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_SW_BAD,           COLOR_RED,    COLOR_BLACK);
            init_pair(CP_SW_SEVERE,        COLOR_MAGENTA,COLOR_BLACK);
            break;

        case 5: { /* 6. Ljust färgschema (Ljusgrå bakgrund, svart text, orange ramar, blå highlights) */
            short bg_gray   = (COLORS >= 256) ? 252 : COLOR_WHITE;
            short fg_orange = (COLORS >= 256) ? 208 : COLOR_RED;
            init_pair(CP_MAIN_BG,        COLOR_BLACK,  bg_gray);
            init_pair(CP_TOPBAR,         COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_TOPBAR_ACCENT,  COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_BORDER,         fg_orange,    bg_gray);
            init_pair(CP_BORDER_ACTIVE,  fg_orange,    bg_gray);
            init_pair(CP_TITLE,          fg_orange,    bg_gray);
            init_pair(CP_LABEL,          COLOR_BLACK,  bg_gray);
            init_pair(CP_FIELD_INACTIVE, COLOR_BLACK,  bg_gray);
            init_pair(CP_FIELD_ACTIVE,   COLOR_BLUE,   bg_gray);
            init_pair(CP_FROZEN_BADGE,   COLOR_WHITE,  fg_orange);
            init_pair(CP_LIVE_BADGE,     COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_EDIT_BADGE,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_TABLE_HEADER,   COLOR_BLUE,   bg_gray);
            init_pair(CP_LOG_ROW,        COLOR_BLACK,  bg_gray);
            init_pair(CP_LOG_SELECTED,   COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_LOG_MARKED,     COLOR_WHITE,  fg_orange);
            init_pair(CP_FKEY_NUM,       COLOR_WHITE,  fg_orange);
            init_pair(CP_FKEY_LABEL,     COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_STATUS_OK,      COLOR_BLUE,   bg_gray);
            init_pair(CP_STATUS_WARN,    COLOR_WHITE,  COLOR_RED);
            init_pair(CP_FLAG_NEW,         COLOR_RED,    bg_gray);
            init_pair(CP_FLAG_HANDLED,     COLOR_GREEN,  bg_gray);
            init_pair(CP_FLAG_NEW_SEL,     COLOR_RED,    COLOR_BLUE);
            init_pair(CP_FLAG_HANDLED_SEL, COLOR_GREEN,  COLOR_BLUE);
            init_pair(CP_FLAG_NEW_MRK,     COLOR_WHITE,  fg_orange);
            init_pair(CP_FLAG_HANDLED_MRK, COLOR_GREEN,  fg_orange);
            init_pair(CP_SW_NORMAL,        COLOR_BLACK,  bg_gray);
            init_pair(CP_SW_OK,            COLOR_GREEN,  bg_gray);
            init_pair(CP_SW_WARN,          fg_orange,    bg_gray);
            init_pair(CP_SW_BAD,           COLOR_RED,    bg_gray);
            init_pair(CP_SW_SEVERE,        COLOR_MAGENTA,bg_gray);
            break;
        }

        case 6: { /* 7. Monokrom gråskala (Allt i harmoniska nyanser av grått och vitt på svart) */
            short col_light = (COLORS >= 256) ? 253 : COLOR_WHITE;
            short col_mid   = (COLORS >= 256) ? 246 : COLOR_WHITE;
            short col_dark  = (COLORS >= 256) ? 238 : COLOR_BLACK;
            short col_bg    = (COLORS >= 256) ? 233 : COLOR_BLACK;

            init_pair(CP_MAIN_BG,        col_light,    col_bg);
            init_pair(CP_TOPBAR,         col_bg,       col_light);
            init_pair(CP_TOPBAR_ACCENT,  col_dark,     col_light);
            init_pair(CP_BORDER,         col_mid,      col_bg);
            init_pair(CP_BORDER_ACTIVE,  col_light,    col_bg);
            init_pair(CP_TITLE,          col_light,    col_bg);
            init_pair(CP_LABEL,          col_mid,      col_bg);
            init_pair(CP_FIELD_INACTIVE, col_light,    col_bg);
            init_pair(CP_FIELD_ACTIVE,   col_bg,       col_light);
            init_pair(CP_FROZEN_BADGE,   col_bg,       col_mid);
            init_pair(CP_LIVE_BADGE,     col_bg,       col_light);
            init_pair(CP_EDIT_BADGE,     col_light,    col_dark);
            init_pair(CP_TABLE_HEADER,   col_light,    col_bg);
            init_pair(CP_LOG_ROW,        col_light,    col_bg);
            init_pair(CP_LOG_SELECTED,   col_bg,       col_light);
            init_pair(CP_LOG_MARKED,     col_light,    col_dark);
            init_pair(CP_FKEY_NUM,       col_bg,       col_mid);
            init_pair(CP_FKEY_LABEL,     col_bg,       col_light);
            init_pair(CP_STATUS_OK,      col_light,    col_bg);
            init_pair(CP_STATUS_WARN,    col_bg,       col_light);
            init_pair(CP_FLAG_NEW,         col_light,    col_bg);
            init_pair(CP_FLAG_HANDLED,     col_mid,      col_bg);
            init_pair(CP_FLAG_NEW_SEL,     col_bg,       col_light);
            init_pair(CP_FLAG_HANDLED_SEL, col_dark,     col_light);
            init_pair(CP_FLAG_NEW_MRK,     col_light,    col_dark);
            init_pair(CP_FLAG_HANDLED_MRK, col_mid,      col_dark);
            init_pair(CP_SW_NORMAL,        col_mid,      col_bg);
            init_pair(CP_SW_OK,            col_light,    col_bg);
            init_pair(CP_SW_WARN,          col_light,    col_bg);
            init_pair(CP_SW_BAD,           col_light,    col_bg);
            init_pair(CP_SW_SEVERE,        col_bg,       col_light);
            break;
        }

        case 7: { /* 8. Ambra / Retro CRT (Bärnsten / klassisk ambra monokrom skärm på svart bakgrund) */
            short amb_bright = (COLORS >= 256) ? 214 : COLOR_YELLOW;
            short amb_mid    = (COLORS >= 256) ? 208 : COLOR_YELLOW;
            short amb_dark   = (COLORS >= 256) ? 130 : COLOR_YELLOW;
            short amb_bg     = COLOR_BLACK;

            init_pair(CP_MAIN_BG,        amb_mid,      amb_bg);
            init_pair(CP_TOPBAR,         amb_bg,       amb_bright);
            init_pair(CP_TOPBAR_ACCENT,  amb_dark,     amb_bright);
            init_pair(CP_BORDER,         amb_dark,     amb_bg);
            init_pair(CP_BORDER_ACTIVE,  amb_bright,   amb_bg);
            init_pair(CP_TITLE,          amb_bright,   amb_bg);
            init_pair(CP_LABEL,          amb_mid,      amb_bg);
            init_pair(CP_FIELD_INACTIVE, amb_mid,      amb_bg);
            init_pair(CP_FIELD_ACTIVE,   amb_bg,       amb_bright);
            init_pair(CP_FROZEN_BADGE,   amb_bg,       amb_mid);
            init_pair(CP_LIVE_BADGE,     amb_bg,       amb_bright);
            init_pair(CP_EDIT_BADGE,     amb_bright,   amb_dark);
            init_pair(CP_TABLE_HEADER,   amb_bright,   amb_bg);
            init_pair(CP_LOG_ROW,        amb_mid,      amb_bg);
            init_pair(CP_LOG_SELECTED,   amb_bg,       amb_bright);
            init_pair(CP_LOG_MARKED,     amb_bg,       amb_mid);
            init_pair(CP_FKEY_NUM,       amb_bg,       amb_dark);
            init_pair(CP_FKEY_LABEL,     amb_bg,       amb_bright);
            init_pair(CP_STATUS_OK,      amb_bright,   amb_bg);
            init_pair(CP_STATUS_WARN,    amb_bg,       amb_bright);
            init_pair(CP_FLAG_NEW,         amb_bright,   amb_bg);
            init_pair(CP_FLAG_HANDLED,     amb_dark,     amb_bg);
            init_pair(CP_FLAG_NEW_SEL,     amb_bg,       amb_bright);
            init_pair(CP_FLAG_HANDLED_SEL, amb_dark,     amb_bright);
            init_pair(CP_FLAG_NEW_MRK,     amb_bright,   amb_dark);
            init_pair(CP_FLAG_HANDLED_MRK, amb_bg,       amb_dark);
            init_pair(CP_SW_NORMAL,        amb_mid,      amb_bg);
            init_pair(CP_SW_OK,            amb_mid,      amb_bg);
            init_pair(CP_SW_WARN,          amb_bright,   amb_bg);
            init_pair(CP_SW_BAD,           amb_bright,   amb_bg);
            init_pair(CP_SW_SEVERE,        amb_bg,       amb_bright);
            break;
        }
    }

    bkgd(COLOR_PAIR(CP_MAIN_BG));
}

/**
 * @brief Ritar en inramad ruta (box) med titel och fokusmarkering.
 * @param y Översta raden.
 * @param x Vänstra kolumnen.
 * @param h Höjd i rader.
 * @param w Bredd i kolumner.
 * @param title Titeltext i rutans överkant.
 * @param has_focus 1 om sektionen har tangentbordsfokus.
 */
void draw_box_with_title(int y, int x, int h, int w, const char *title, int has_focus) {
    if (h < 2 || w < 4) return;
    int color = has_focus ? CP_BORDER_ACTIVE : CP_BORDER;
    int attr  = has_focus ? A_BOLD : 0;
    attron(COLOR_PAIR(color) | attr);
    mvaddch(y, x, ACS_ULCORNER);
    mvhline(y, x + 1, ACS_HLINE, w - 2);
    mvaddch(y, x + w - 1, ACS_URCORNER);

    for (int r = y + 1; r < y + h - 1; ++r) {
        mvaddch(r, x, ACS_VLINE);
        mvaddch(r, x + w - 1, ACS_VLINE);
    }

    mvaddch(y + h - 1, x, ACS_LLCORNER);
    mvhline(y + h - 1, x + 1, ACS_HLINE, w - 2);
    mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);
    attroff(COLOR_PAIR(color) | attr);

    if (title != NULL && title[0] != '\0') {
        int tlen = utf8_display_width(title);
        if (tlen + 4 <= w) {
            attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
            mvprintw(y, x + 2, " %s ", title);
            attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
        }
    }
}

/**
 * @brief Ritar översta statuslisten med programnamn, egen anropssignal, QTH och UTC-klocka.
 * @param st Min stations inställningar.
 */
void draw_top_bar(const StationSettings *st) {
    attron(COLOR_PAIR(CP_TOPBAR));
    mvhline(0, 0, ' ', COLS);
    attroff(COLOR_PAIR(CP_TOPBAR));

    attron(COLOR_PAIR(CP_TOPBAR_ACCENT) | A_BOLD);
    mvprintw(0, 1, "UEI-log");
    attroff(COLOR_PAIR(CP_TOPBAR_ACCENT) | A_BOLD);

    attron(COLOR_PAIR(CP_TOPBAR) | A_BOLD);
    mvprintw(0, 9, "v1.0");
    attroff(COLOR_PAIR(CP_TOPBAR) | A_BOLD);

    char station_info[128];
    char clean_call[32] = "";
    if (st != NULL && st->call[0] != '\0') {
        copy_str(clean_call, sizeof(clean_call), st->call);
    }
    const char *my_qth = (st != NULL && st->qth[0] != '\0') ? st->qth : "SM";
    snprintf(station_info, sizeof(station_info), "%s (%s)", clean_call[0] ? clean_call : "SM5UEI", my_qth);

    int info_col = (COLS - utf8_display_width(station_info)) / 2;
    if (info_col > 14) {
        attron(COLOR_PAIR(CP_TOPBAR) | A_BOLD);
        mvprintw(0, info_col, "%s", station_info);
        attroff(COLOR_PAIR(CP_TOPBAR) | A_BOLD);
    }

    char time_str[64];
    get_utc_strings(NULL, 0, NULL, 0, time_str, sizeof(time_str));
    int time_col = COLS - (int)strlen(time_str) - 2;
    if (time_col > info_col + (int)strlen(station_info)) {
        attron(COLOR_PAIR(CP_TOPBAR) | A_BOLD);
        mvprintw(0, time_col, "%s", time_str);
        attroff(COLOR_PAIR(CP_TOPBAR) | A_BOLD);
    }
}

/**
 * @brief Ritar den lilla statistikrutan ovanför QSO-inmatningen (2 rader data i ram).
 * Visar QSO, Stationer, DXCC, Lokatorer, Bandland och Rate (senaste 60 min).
 * Sätter etiketten på första raden och siffran på den andra.
 * @param top_y Översta raden för rutan.
 * @param width Rutans bredd.
 * @param log_list Pekare till logglistan.
 */
void draw_stats_box(int top_y, int width, const QSOList *log_list) {
    draw_box_with_title(top_y, 0, 4, width, "Statistik", 0);

    LogStats stats;
    compute_log_stats(log_list, &stats);

    const char *labels[] = {
        "QSO",
        (width >= 75) ? "Stationer" : "Stat.",
        "DXCC",
        (width >= 75) ? "Lokatorer" : "Lok.",
        "Bandland",
        "Rate"
    };

    int vals[] = {
        stats.total_qsos,
        stats.unique_stations,
        stats.unique_dxcc,
        stats.unique_grids,
        stats.unique_bandland,
        stats.rate_last_hour
    };

    int inner_w = width - 2;
    if (inner_w < 12) return;

    int r_lbl = top_y + 1;
    int r_val = top_y + 2;

    for (int i = 0; i < 6; ++i) {
        int cx = 1 + (inner_w * (2 * i + 1)) / 12;

        /* Etikett på rad 1 */
        int llen = utf8_display_width(labels[i]);
        int lx = cx - llen / 2;
        if (lx < 1) lx = 1;
        attron(COLOR_PAIR(CP_LABEL));
        mvprintw(r_lbl, lx, "%s", labels[i]);
        attroff(COLOR_PAIR(CP_LABEL));

        /* Siffra på rad 2 */
        char buf[32];
        snprintf(buf, sizeof(buf), "%d", vals[i]);
        int vlen = (int)strlen(buf);
        int vx = cx - vlen / 2;
        if (vx < 1) vx = 1;
        attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
        mvprintw(r_val, vx, "%s", buf);
        attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
    }
}

/**
 * @brief Beräknar tillgängligt antal synliga rader för logglistan.
 * Om fönstret har minst 28 rader dras 4 rader av för rymdvädersrutan.
 * Vid färre än 28 rader döljs rymdvädret och loggraderna får hela höjden.
 * @return Antal synliga rader i loggen.
 */
int get_avail_log_rows(void) {
    int top_reserved = 11; /* Topbar (1) + Stats box (4) + Input box (6) */
    int bot_reserved = 1;  /* Endast F-tangentrad (1) */
    int sw_reserved  = (LINES >= 28) ? 4 : 0;
    int log_h = LINES - top_reserved - bot_reserved - sw_reserved;
    int avail = log_h - 2; /* minus ram och kolumnrubriker */
    return (avail > 0) ? avail : 1;
}

/**
 * @brief Bygger en filtrerad array med index för loggrader som matchar sökfilter eller aktiv anropssignal.
 * @param log_list Logglistan.
 * @param search_filter Söktext.
 * @param indices_out Array som fylls med matchande index.
 * @param max_out Maxstorlek på indices_out.
 * @return Antal matchande rader.
 */
int build_filtered_indices(const QSOList *log_list, const char *search_filter,
                           int *indices_out, int max_out) {
    int count = 0;
    if (log_list == NULL || indices_out == NULL || max_out <= 0) return 0;

    for (int i = (int)log_list->count - 1; i >= 0 && count < max_out; --i) {
        if (search_filter == NULL || search_filter[0] == '\0') {
            indices_out[count++] = i;
        } else {
            const QSO *q = &log_list->items[i];
            if (utf8_contains_ci(q->call, search_filter) ||
                utf8_contains_ci(q->name, search_filter) ||
                utf8_contains_ci(q->qth, search_filter) ||
                utf8_contains_ci(q->gridsquare, search_filter) ||
                utf8_contains_ci(q->comment, search_filter) ||
                utf8_contains_ci(q->band, search_filter) ||
                utf8_contains_ci(q->mode, search_filter)) {
                indices_out[count++] = i;
            }
        }
    }
    return count;
}

/**
 * @brief Ritar ett högerställt etiketterat inmatningsfält med markörposition och rullning.
 */
void draw_field_aligned(int row, int colon_col, const char *label, const char *val,
                        int width, int is_active, int cursor_pos,
                        int *cur_y, int *cur_x) {
    attron(COLOR_PAIR(CP_LABEL));
    int llen = utf8_display_width(label);
    mvprintw(row, colon_col - llen, "%s:", label);
    attroff(COLOR_PAIR(CP_LABEL));

    int val_col = colon_col + 2;
    int color = is_active ? CP_FIELD_ACTIVE : CP_FIELD_INACTIVE;
    int attr  = is_active ? A_BOLD : 0;

    attron(COLOR_PAIR(color) | attr);
    for (int i = 0; i < width; ++i) {
        mvaddch(row, val_col + i, ' ');
    }

    int val_w = utf8_display_width(val);
    int scroll_chars = 0;
    if (is_active && cursor_pos >= width) {
        scroll_chars = cursor_pos - width + 1;
    } else if (!is_active && val_w > width) {
        scroll_chars = val_w - width;
    }

    size_t start_byte = utf8_char_to_byte(val, scroll_chars);
    size_t end_byte   = utf8_char_to_byte(val, scroll_chars + width);
    mvprintw(row, val_col, "%.*s", (int)(end_byte - start_byte), val + start_byte);
    attroff(COLOR_PAIR(color) | attr);

    if (is_active) {
        *cur_y = row;
        *cur_x = val_col + (cursor_pos - scroll_chars);
    }
}

/**
 * @brief Ritar det anpassade bandvalsfältet med pilindikator (v eller ^).
 */
void draw_band_selector_aligned(int row, int colon_col, const char *val,
                                int is_active, int band_selecting,
                                int *cur_y, int *cur_x) {
    attron(COLOR_PAIR(CP_LABEL));
    int llen = utf8_display_width("Band");
    mvprintw(row, colon_col - llen, "Band:");
    attroff(COLOR_PAIR(CP_LABEL));

    int val_col = colon_col + 2;
    int width = 8;

    char formatted[16];
    format_band_display(val, formatted, sizeof(formatted));

    int color = is_active ? CP_FIELD_ACTIVE : CP_FIELD_INACTIVE;
    int attr  = is_active ? A_BOLD : 0;
    if (band_selecting) {
        attr |= A_REVERSE;
    }

    const char *arrow = band_selecting ? "^" : "v";

    attron(COLOR_PAIR(color) | attr);
    for (int i = 0; i < width; ++i) {
        mvaddch(row, val_col + i, ' ');
    }
    mvprintw(row, val_col, " %-4s %s", formatted, arrow);
    attroff(COLOR_PAIR(color) | attr);

    if (is_active) {
        *cur_y = row;
        *cur_x = val_col + width - 1;
    }
}

/**
 * @brief Ritar den nedersta funktionsknappsraden (F1..F12) beroende på aktivt fokusläge.
 */
void draw_bottom_fkey_bar(int mode_type, FocusMode focus_mode, const char *search_filter) {
    int y = LINES - 1;
    attron(COLOR_PAIR(CP_TOPBAR));
    mvhline(y, 0, ' ', COLS);
    attroff(COLOR_PAIR(CP_TOPBAR));

    typedef struct {
        const char *fkey;
        const char *label;
    } FKeyItem;

    FKeyItem items[14];
    int count = 0;

    if (mode_type == 1) {
        items[count++] = (FKeyItem){"F2", "Spara"};
        items[count++] = (FKeyItem){"F5", "Import"};
        items[count++] = (FKeyItem){"F6", "Export"};
        items[count++] = (FKeyItem){"F9", "Hjälp"};
        items[count++] = (FKeyItem){"Esc", "Stäng"};
    } else {
        if (focus_mode == FOCUS_LOG) {
            items[count++] = (FKeyItem){"F1", "Rensa"};
            if (search_filter && search_filter[0]) {
                items[count++] = (FKeyItem){"Esc", "Återställ"};
            }
            items[count++] = (FKeyItem){"Enter", "Redigera"};
            items[count++] = (FKeyItem){"F4", "Fokus"};
            items[count++] = (FKeyItem){"F5", "Hanterad"};
            items[count++] = (FKeyItem){"F6", "Ny"};
            items[count++] = (FKeyItem){"F8", "Radera"};
            items[count++] = (FKeyItem){"F12", "Inställn."};
        } else {
            items[count++] = (FKeyItem){"F1", "Rensa"};
            if (search_filter && search_filter[0]) {
                items[count++] = (FKeyItem){"Esc", "Återställ"};
            }
            items[count++] = (FKeyItem){"F2", "Logga"};
            items[count++] = (FKeyItem){"F3", (search_filter && search_filter[0]) ? "Sök*" : "Sök"};
            items[count++] = (FKeyItem){"F4", "Logg"};
            items[count++] = (FKeyItem){"F5", "Uppslag"};
            items[count++] = (FKeyItem){"F6", "QRZ"};
            items[count++] = (FKeyItem){"F9", "Hjälp"};
            items[count++] = (FKeyItem){"F10", "Avsluta"};
            items[count++] = (FKeyItem){"F12", "Inställn."};
        }
    }

    int col = 0;
    for (int i = 0; i < count; ++i) {
        int klen = (int)strlen(items[i].fkey);
        int llen = utf8_display_width(items[i].label);
        if (col + klen + llen + 2 > COLS) break;

        attron(COLOR_PAIR(CP_FKEY_NUM) | A_BOLD);
        mvprintw(y, col, "%s", items[i].fkey);
        attroff(COLOR_PAIR(CP_FKEY_NUM) | A_BOLD);
        col += klen;

        attron(COLOR_PAIR(CP_FKEY_LABEL));
        mvprintw(y, col, "%s", items[i].label);
        attroff(COLOR_PAIR(CP_FKEY_LABEL));
        col += llen;

        mvaddch(y, col++, ' ');
    }
}

/**
 * @brief Hjälpfunktion som mappar SwTone till motsvarande curses-färgpar.
 */
static int sw_tone_to_color_pair(SwTone tone) {
    switch (tone) {
        case SW_TONE_OK:     return CP_SW_OK;
        case SW_TONE_WARN:   return CP_SW_WARN;
        case SW_TONE_BAD:    return CP_SW_BAD;
        case SW_TONE_SEVERE: return CP_SW_SEVERE;
        default:             return CP_SW_NORMAL;
    }
}

/**
 * @brief Hjälpfunktion för att rita ett rymdvädersfält högerjusterat mot ett kolon.
 */
static void print_sw_field(int row, int colon_col, const char *label, const char *val, int color_pair) {
    attron(COLOR_PAIR(CP_LABEL));
    int llen = utf8_display_width(label);
    mvprintw(row, colon_col - llen, "%s:", label);
    attroff(COLOR_PAIR(CP_LABEL));

    attron(COLOR_PAIR(color_pair));
    mvprintw(row, colon_col + 2, "%s", (val && val[0] != '\0') ? val : "-");
    attroff(COLOR_PAIR(color_pair));
}

/**
 * @brief Ritar den inramade rymdvädersrutan under loggraderna med data från https://hf.avitec.se.
 * Fälten är alignade på kolon så att etiketterna linjerar lodrätt mellan rad 1 och rad 2.
 * Visar K-index (Kiruna), MUF, LUF, CME på rad 1 samt Solfackla, Solvind, Norrsken på rad 2.
 * @param top_y Översta raden för rutan.
 * @param width Rutans bredd (COLS).
 */
void draw_space_weather_box(int top_y, int width) {
    draw_box_with_title(top_y, 0, 4, width, "Rymdväder (hf.avitec.se)", 0);

    SpaceWeatherData sw;
    space_weather_get_data(&sw);

    /* Beräkna kolumnpositioner för kolon */
    int colon0 = 12;
    int colon1 = (width >= 90) ? (width * 31 / 100) : 33;
    int colon2 = (width >= 90) ? (width * 55 / 100) : 53;
    int colon3 = (width >= 90) ? (width * 77 / 100) : 69;

    int r1 = top_y + 1;
    int r2 = top_y + 2;

    /* RAD 1: K-index ("K Kiruna"), MUF, LUF, CME */
    print_sw_field(r1, colon0, "K Kiruna", sw.k_kiruna, sw_tone_to_color_pair(sw.k_tone));
    print_sw_field(r1, colon1, "MUF", sw.muf, sw_tone_to_color_pair(sw.muf_tone));
    print_sw_field(r1, colon2, "LUF", sw.luf, sw_tone_to_color_pair(sw.luf_tone));
    print_sw_field(r1, colon3, "CME", sw.cme, sw_tone_to_color_pair(sw.cme_tone));

    /* RAD 2: Solfackla, Solvind, Norrsken */
    print_sw_field(r2, colon0, "Solfackla", sw.solfackla, sw_tone_to_color_pair(sw.solfackla_tone));
    print_sw_field(r2, colon1, "Solvind", sw.solvind, sw_tone_to_color_pair(sw.solvind_tone));
    print_sw_field(r2, colon2, "Norrsken", sw.norrsken, sw_tone_to_color_pair(sw.norrsken_tone));

    /* Tidsstämpel för senaste uppdatering till höger */
    if (width >= 70 && sw.updated_time[0] != '\0') {
        attron(COLOR_PAIR(CP_BORDER));
        mvprintw(r2, width - 15, "(kl %s)", sw.updated_time);
        attroff(COLOR_PAIR(CP_BORDER));
    }
}

/**
 * @brief Ritar hela programmets huvudfönster inklusive inmatningssektion, loggtabell,
 * rymdvädersruta (vid LINES >= 25), statusrad och F-tangentrad.
 */
void draw_main_screen(const StationSettings *st, const QSO *qso,
                      FocusMode focus_mode, int active_field, int band_selecting,
                      int field_cursor_pos, int time_frozen, int editing_index,
                      const QSOList *log_list, int selected_log_pos, int *log_scroll_offset,
                      const char *search_filter, const char *status_msg,
                      const char *qrb_str,
                      int *cur_y, int *cur_x) {
    erase();

    if (COLS < MIN_COLS || LINES < MIN_LINES) {
        mvprintw(LINES / 2, 2, "Fönstret är för litet (%dx%d). Minsta storlek är %dx%d.",
                 COLS, LINES, MIN_COLS, MIN_LINES);
        refresh();
        return;
    }

    draw_top_bar(st);

    /* RITA STATISTIKRUTAN OVANFÖR INMATNINGSRUTAN (rad 1..4) */
    int stat_top = 1;
    int stat_h = 4;
    draw_stats_box(stat_top, COLS, log_list);

    /* RITA INMATNINGSSEKTIONEN (rad 5..10) */
    int input_top = stat_top + stat_h; /* 5 */
    int input_h = 6;
    draw_box_with_title(input_top, 0, input_h, COLS, "QSO-inmatning", (focus_mode == FOCUS_INPUT));

    int badge_w = 13;
    if (editing_index >= 0) {
        char edit_tag[48];
        snprintf(edit_tag, sizeof(edit_tag), "[REDIGERAR QSO #%d]", editing_index + 1);
        badge_w = utf8_display_width(edit_tag) + 3;
        attron(COLOR_PAIR(CP_EDIT_BADGE) | A_BOLD);
        mvprintw(input_top, COLS - badge_w, "%s", edit_tag);
        attroff(COLOR_PAIR(CP_EDIT_BADGE) | A_BOLD);
    } else if (time_frozen) {
        attron(COLOR_PAIR(CP_FROZEN_BADGE) | A_BOLD);
        mvprintw(input_top, COLS - 13, "[TID FRYST]");
        attroff(COLOR_PAIR(CP_FROZEN_BADGE) | A_BOLD);
    } else {
        attron(COLOR_PAIR(CP_LIVE_BADGE) | A_BOLD);
        mvprintw(input_top, COLS - 13, "[LIVE UTC]");
        attroff(COLOR_PAIR(CP_LIVE_BADGE) | A_BOLD);
    }

    /* Statusmeddelande i fönsterlisten: tre tecken till höger om "QSO-inmatning" */
    if (status_msg != NULL && status_msg[0] != '\0') {
        int stat_col = 19;
        int badge_col = COLS - badge_w;
        int max_msg = badge_col - stat_col - 2;
        if (max_msg > 0) {
            int is_err = (strstr(status_msg, "Fel:") != NULL || strstr(status_msg, "VARNING") != NULL);
            int color = is_err ? CP_STATUS_WARN : CP_STATUS_OK;
            attron(COLOR_PAIR(color) | A_BOLD);
            mvprintw(input_top, stat_col, " %.*s ", max_msg, status_msg);
            attroff(COLOR_PAIR(color) | A_BOLD);
        }
    }

    /* Kolumnpositioner för kolon i inmatningsfält */
    int c1 = 9;
    int c2 = (COLS >= 95) ? 33 : (COLS >= 85 ? 31 : 29);
    int c3 = (COLS >= 95) ? 51 : (COLS >= 85 ? 49 : 45);
    int c4 = (COLS >= 95) ? 65 : (COLS >= 85 ? 63 : 59);

    int form_w_name = (COLS >= 88) ? 14 : (COLS - (c4 + 2) - 1);
    if (form_w_name < 6) form_w_name = 6;
    if (form_w_name > 16) form_w_name = 16;

    int form_w_dxcc = (COLS >= 88) ? 14 : (COLS - (c4 + 2) - 1);
    if (form_w_dxcc < 6) form_w_dxcc = 6;
    if (form_w_dxcc > 16) form_w_dxcc = 16;

    int form_w_qth = (COLS >= 88) ? 15 : 14;

    /* Rad 1 (y = 6): Signal, RST T, RST R, Namn */
    draw_field_aligned(input_top + 1, c1, "Signal", qso->call,     10,          (focus_mode == FOCUS_INPUT && active_field == FLD_CALL), field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(input_top + 1, c2, "RST T",  qso->rst_sent,  6,          (focus_mode == FOCUS_INPUT && active_field == FLD_RSTS), field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(input_top + 1, c3, "RST R",  qso->rst_rcvd,  6,          (focus_mode == FOCUS_INPUT && active_field == FLD_RSTR), field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(input_top + 1, c4, "Namn",   qso->name,     form_w_name, (focus_mode == FOCUS_INPUT && active_field == FLD_NAME), field_cursor_pos, cur_y, cur_x);

    /* Rad 2 (y = 7): QTH, Lokator, QRB, DXCC */
    draw_field_aligned(input_top + 2, c1, "QTH",     qso->qth,        form_w_qth,  (focus_mode == FOCUS_INPUT && active_field == FLD_QTH),  field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(input_top + 2, c2, "Lokator", qso->gridsquare, 6,           (focus_mode == FOCUS_INPUT && active_field == FLD_GRID), field_cursor_pos, cur_y, cur_x);

    /* QRB mellan Lokator och DXCC på rad 2 */
    int has_qrb = (qrb_str != NULL && qrb_str[0] != '\0' && strcmp(qrb_str, "-") != 0);
    const char *qrb_val = has_qrb ? qrb_str : "-";
    int lbl_qrb_x = c3 - 3; /* "QRB" är 3 tecken */
    attron(COLOR_PAIR(CP_LABEL));
    mvprintw(input_top + 2, lbl_qrb_x, "QRB:");
    attroff(COLOR_PAIR(CP_LABEL));
    int qrb_color = has_qrb ? CP_STATUS_OK : CP_FIELD_INACTIVE;
    attron(COLOR_PAIR(qrb_color) | (has_qrb ? A_BOLD : 0));
    mvprintw(input_top + 2, c3 + 2, "%-8.8s", qrb_val);
    attroff(COLOR_PAIR(qrb_color) | (has_qrb ? A_BOLD : 0));

    draw_field_aligned(input_top + 2, c4, "DXCC",    qso->dxcc,       form_w_dxcc, (focus_mode == FOCUS_INPUT && active_field == FLD_DXCC), field_cursor_pos, cur_y, cur_x);

    /* Rad 3 (y = 8): Frekvens, Band, Mode */
    draw_field_aligned(input_top + 3, c1, "Frekvens", qso->freq, 10, (focus_mode == FOCUS_INPUT && active_field == FLD_FREQ), field_cursor_pos, cur_y, cur_x);
    draw_band_selector_aligned(input_top + 3, c2, qso->band, (focus_mode == FOCUS_INPUT && active_field == FLD_BAND), band_selecting, cur_y, cur_x);
    draw_field_aligned(input_top + 3, c3, "Mode",     qso->mode,  6, (focus_mode == FOCUS_INPUT && active_field == FLD_MODE), field_cursor_pos, cur_y, cur_x);

    /* Rad 4 (y = 9): Datum, UTC, Notering */
    int comm_w = COLS - (c3 + 2) - 1;
    if (comm_w < 5) comm_w = 5;
    const char *lbl_not = (COLS >= 88) ? "Notering" : "Not";
    draw_field_aligned(input_top + 4, c1, "Datum",    qso->date,     10,     (focus_mode == FOCUS_INPUT && active_field == FLD_DATE), field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(input_top + 4, c2, "UTC",      qso->time_utc,  6,     (focus_mode == FOCUS_INPUT && active_field == FLD_TIME), field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(input_top + 4, c3, lbl_not,    qso->comment,  comm_w, (focus_mode == FOCUS_INPUT && active_field == FLD_COMM), field_cursor_pos, cur_y, cur_x);

    /* RITA LOGGRADSKOMPONENTEN */
    int sw_reserved = (LINES >= 28) ? 4 : 0;
    int log_top = input_top + input_h; /* 11 */
    int log_h = LINES - log_top - 1 - sw_reserved;
    if (log_h < 3) log_h = 3;

    char log_title[64];
    if (search_filter != NULL && search_filter[0] != '\0') {
        snprintf(log_title, sizeof(log_title), "Loggrader [Filter: %s]", search_filter);
    } else {
        snprintf(log_title, sizeof(log_title), "Tidigare QSO (Loggrader)");
    }
    draw_box_with_title(log_top, 0, log_h, COLS, log_title, (focus_mode == FOCUS_LOG));

    /* Kolumnrubriker */
    int col_ny = COLS - 4;
    int max_col = COLS - 5;

    attron(COLOR_PAIR(CP_TABLE_HEADER) | A_BOLD);
    mvhline(log_top + 1, 1, ' ', COLS - 2);
    mvprintw(log_top + 1, 2, "   #");
    mvprintw(log_top + 1, 7, "Datum");
    mvprintw(log_top + 1, 18, "Tid");
    mvprintw(log_top + 1, 24, "Signal");
    mvprintw(log_top + 1, 36, "Frekv (MHz)");
    mvprintw(log_top + 1, 49, "%7s", "Band");
    mvprintw(log_top + 1, 58, "Mode");
    mvprintw(log_top + 1, 64, "RST T");
    mvprintw(log_top + 1, 71, "RST R");

    int w_name = 0, w_qth_col = 0, w_loc = 0, w_dxcc = 0;
    if (max_col >= 78 + 4) {
        w_name = max_col - 78 + 1;
        if (w_name > 16) w_name = 16;
        mvprintw(log_top + 1, 78, "%-*.*s", w_name, w_name, "Namn");
    }
    if (w_name == 16 && max_col >= 96 + 4) {
        w_qth_col = max_col - 96 + 1;
        if (w_qth_col > 16) w_qth_col = 16;
        mvprintw(log_top + 1, 96, "%-*.*s", w_qth_col, w_qth_col, "QTH");
    }
    if (w_qth_col == 16 && max_col >= 114 + 6) {
        w_loc = max_col - 114 + 1;
        if (w_loc > 8) w_loc = 8;
        mvprintw(log_top + 1, 114, "%-*.*s", w_loc, w_loc, "Lokator");
    }
    if (w_loc == 8 && max_col >= 124 + 4) {
        w_dxcc = max_col - 124 + 1;
        if (w_dxcc > 18) w_dxcc = 18;
        mvprintw(log_top + 1, 124, "%-*.*s", w_dxcc, w_dxcc, "DXCC");
    }

    mvprintw(log_top + 1, col_ny, "Ny");
    attroff(COLOR_PAIR(CP_TABLE_HEADER) | A_BOLD);

    /* Bygg lista av filtrerade index */
    int max_entries = (int)log_list->count > 0 ? (int)log_list->count : 1;
    int *filtered_indices = malloc((size_t)max_entries * sizeof(int));
    int filtered_count = filtered_indices ? build_filtered_indices(log_list, search_filter, filtered_indices, max_entries) : 0;

    int log_avail_rows = log_h - 2;
    if (log_avail_rows < 1) log_avail_rows = 1;

    if (focus_mode == FOCUS_LOG && filtered_count > 0) {
        if (selected_log_pos < *log_scroll_offset) {
            *log_scroll_offset = selected_log_pos;
        }
        /* Kontrollera att selected_log_pos ryms på skärmen från log_scroll_offset */
        while (*log_scroll_offset < selected_log_pos) {
            int lines = 0;
            for (int idx = *log_scroll_offset; idx <= selected_log_pos; ++idx) {
                lines++;
                int orig = filtered_indices[idx];
                if (log_list->items[orig].comment[0] != '\0') {
                    lines++;
                }
            }
            if (lines <= log_avail_rows) {
                break;
            }
            (*log_scroll_offset)++;
        }
    }
    if (*log_scroll_offset < 0) *log_scroll_offset = 0;
    if (filtered_count > 0 && *log_scroll_offset >= filtered_count) {
        *log_scroll_offset = filtered_count - 1;
    }

    int cur_screen_row = 0;
    int f_idx = *log_scroll_offset;

    while (cur_screen_row < log_avail_rows && f_idx < filtered_count) {
        int row_y = log_top + 2 + cur_screen_row;
        int actual_idx = filtered_indices[f_idx];
        const QSO *item = &log_list->items[actual_idx];

        int is_selected = (focus_mode == FOCUS_LOG && f_idx == selected_log_pos);
        int is_marked   = item->marked;

        int row_color = is_selected ? CP_LOG_SELECTED : (is_marked ? CP_LOG_MARKED : CP_LOG_ROW);
        int row_attr  = (is_selected || is_marked) ? A_BOLD : 0;

        attron(COLOR_PAIR(row_color) | row_attr);
        mvhline(row_y, 1, ' ', COLS - 2);

        char mark_char = is_marked ? '*' : (is_selected ? '>' : ' ');
        mvprintw(row_y, 2, "%c%3d", mark_char, actual_idx + 1);
        mvprintw(row_y, 7, "%-10s", item->date);
        mvprintw(row_y, 18, "%-5s", item->time_utc);
        mvprintw(row_y, 24, "%-10s", item->call);
        mvprintw(row_y, 36, "%-10s", item->freq);

        char band_dsp[16];
        format_band_display(item->band, band_dsp, sizeof(band_dsp));
        mvprintw(row_y, 49, "%7s", band_dsp);

        mvprintw(row_y, 58, "%-4s", item->mode);
        mvprintw(row_y, 64, "%-5s", item->rst_sent);
        mvprintw(row_y, 71, "%-5s", item->rst_rcvd);

        if (w_name > 0) {
            mvprintw(row_y, 78, "%-*.*s", w_name, w_name, item->name);
        }
        if (w_qth_col > 0) {
            mvprintw(row_y, 96, "%-*.*s", w_qth_col, w_qth_col, item->qth);
        }
        if (w_loc > 0) {
            mvprintw(row_y, 114, "%-*.*s", w_loc, w_loc, item->gridsquare);
        }
        if (w_dxcc > 0) {
            mvprintw(row_y, 124, "%-*.*s", w_dxcc, w_dxcc, item->dxcc);
        }

        attroff(COLOR_PAIR(row_color) | row_attr);

        /* Hanterad statusflagga: grön bock (✓) eller rött kryss (✗) */
        int flag_color = item->handled ?
            (is_selected ? CP_FLAG_HANDLED_SEL : (is_marked ? CP_FLAG_HANDLED_MRK : CP_FLAG_HANDLED)) :
            (is_selected ? CP_FLAG_NEW_SEL : (is_marked ? CP_FLAG_NEW_MRK : CP_FLAG_NEW));

        attron(COLOR_PAIR(flag_color) | A_BOLD);
        mvprintw(row_y, col_ny, item->handled ? " ✓ " : " ✗ ");
        attroff(COLOR_PAIR(flag_color) | A_BOLD);

        if (is_selected) {
            *cur_y = row_y;
            *cur_x = 2;
        }

        cur_screen_row++;

        /* Om det finns en Notering och det finns plats kvar i rutan: visa den på raden under */
        if (item->comment[0] != '\0' && cur_screen_row < log_avail_rows) {
            int note_y = log_top + 2 + cur_screen_row;
            attron(COLOR_PAIR(row_color) | row_attr);
            mvhline(note_y, 1, ' ', COLS - 2);

            int max_note_w = COLS - 19;
            if (max_note_w < 5) max_note_w = 5;

            if (is_selected || is_marked) {
                mvprintw(note_y, 7, "Notering: %.*s", max_note_w, item->comment);
                attroff(COLOR_PAIR(row_color) | row_attr);
            } else {
                attroff(COLOR_PAIR(row_color) | row_attr);
                attron(COLOR_PAIR(CP_LABEL));
                mvprintw(note_y, 7, "Notering: ");
                attroff(COLOR_PAIR(CP_LABEL));
                attron(COLOR_PAIR(CP_LOG_ROW));
                printw("%.*s", max_note_w, item->comment);
                attroff(COLOR_PAIR(CP_LOG_ROW));
            }

            cur_screen_row++;
        }

        f_idx++;
    }

    /* Rensa eventuella resterande rader i loggrutan */
    while (cur_screen_row < log_avail_rows) {
        int clear_y = log_top + 2 + cur_screen_row;
        attron(COLOR_PAIR(CP_MAIN_BG));
        mvhline(clear_y, 1, ' ', COLS - 2);
        attroff(COLOR_PAIR(CP_MAIN_BG));
        cur_screen_row++;
    }
    if (filtered_indices) free(filtered_indices);

    /* RITA RYMDVÄDERSKOMPONENTEN (endast om LINES >= 28) */
    if (LINES >= 28) {
        int sw_top = log_top + log_h;
        draw_space_weather_box(sw_top, COLS);
    }

    /* RITA F-TANGENTRAD ALLRA LÄNGST NED */
    draw_bottom_fkey_bar(0, focus_mode, search_filter);

    move(*cur_y, *cur_x);
    refresh();
}


/**
 * @brief Visar och hanterar en sökdialog i fönstrets nederkant för att filtrera loggrader (F3).
 * @param st Min stations inställningar.
 * @param search_filter Sökfilterbuffer.
 * @param filter_sz Storlek på målbuffer.
 * @return 1 om sökningen bekräftades med Enter, 0 om den avbröts med Esc/F3.
 */
int run_search_bar(const StationSettings *st, char *search_filter, size_t filter_sz) {
    (void)st;
    char buf[64];
    copy_str(buf, sizeof(buf), search_filter);
    int cursor_pos = utf8_display_width(buf);

    while (1) {
        int y = LINES - 2;
        attron(COLOR_PAIR(CP_TOPBAR) | A_BOLD);
        mvhline(y, 0, ' ', COLS);
        mvprintw(y, 2, "Sök i loggen (Esc/F3 avbryter, Enter bekräftar): ");
        int prompt_len = 50;

        attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        int field_w = COLS - prompt_len - 4;
        if (field_w > 30) field_w = 30;
        for (int i = 0; i < field_w; ++i) mvaddch(y, prompt_len + i, ' ');
        mvprintw(y, prompt_len, "%-.*s", field_w, buf);
        attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        attroff(COLOR_PAIR(CP_TOPBAR) | A_BOLD);

        move(y, prompt_len + cursor_pos);
        refresh();

        int ch = getch();
        if (ch == ERR) continue;

        if (ch == 27 || ch == KEY_F(3)) {
            return 0;
        } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            trim_str(buf);
            copy_str(search_filter, filter_sz, buf);
            return 1;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            utf8_backspace_at(buf, &cursor_pos);
        } else if (ch == KEY_DC) {
            buf[0] = '\0';
            cursor_pos = 0;
        } else if (ch >= 32 && ch != 127 && ch < KEY_MIN) {
            utf8_insert_byte_at(buf, sizeof(buf), &cursor_pos, (char)ch);
        }
    }
}

/**
 * @brief Visar och kör den interaktiva rullgardinsmenyn (drop-down) för bandval.
 *
 * Placeras direkt under bandfältet (rad band_row + 1). Användaren kan navigera
 * med piltangenter upp/ned, PgUp/PgDn, Home/End eller siffror. Enter eller Mellanslag
 * bekräftar valet. Esc avbryter utan att ändra aktuellt band eller rensa formuläret.
 * Tab / Shift-Tab bekräftar och indikerar fältflytt.
 *
 * @return 0 vid avbryt (Esc), 1 vid val (Enter/Mellanslag), 2 vid Tab, 3 vid Shift-Tab.
 */
int run_band_dropdown(StationSettings *st, QSO *qso,
                      FocusMode focus_mode, int active_field,
                      int field_cursor_pos, int *time_frozen, int editing_index,
                      const QSOList *log_list, int selected_log_pos, int *log_scroll_offset,
                      const char *search_filter, char *status_msg, size_t status_sz,
                      const char *qrb_str, int band_row, int colon_col) {
    int cur_idx = find_band_index(qso->band);
    if (cur_idx < 0) cur_idx = 0;
    int sel_idx = cur_idx;

    int list_w = 14;
    int pop_x = colon_col;
    if (pop_x + list_w >= COLS) {
        pop_x = COLS - list_w - 1;
    }
    if (pop_x < 1) pop_x = 1;

    int pop_y = band_row + 1;
    int avail_h = (LINES - 1) - pop_y;
    int visible_items = BAND_OPTION_COUNT;
    if (visible_items + 2 > avail_h) {
        visible_items = avail_h - 2;
        if (visible_items < 4) visible_items = 4;
    }
    int list_h = visible_items + 2;

    int scroll_offset = 0;

    curs_set(0);

    while (1) {
        if (sel_idx < scroll_offset) {
            scroll_offset = sel_idx;
        } else if (sel_idx >= scroll_offset + visible_items) {
            scroll_offset = sel_idx - visible_items + 1;
        }
        if (scroll_offset < 0) scroll_offset = 0;
        if (scroll_offset + visible_items > BAND_OPTION_COUNT) {
            scroll_offset = BAND_OPTION_COUNT - visible_items;
            if (scroll_offset < 0) scroll_offset = 0;
        }

        int cur_y = 0, cur_x = 0;
        draw_main_screen(st, qso, focus_mode, active_field, 1,
                         field_cursor_pos, *time_frozen, editing_index,
                         log_list, selected_log_pos, log_scroll_offset,
                         search_filter, status_msg, qrb_str,
                         &cur_y, &cur_x);

        /* Rita popup-ram */
        draw_box_with_title(pop_y, pop_x, list_h, list_w, "Band", 1);

        /* Rita rullningsindikatorer i ramen om listan är beskuren */
        if (scroll_offset > 0) {
            mvprintw(pop_y, pop_x + list_w - 2, "^");
        }
        if (scroll_offset + visible_items < BAND_OPTION_COUNT) {
            mvprintw(pop_y + list_h - 1, pop_x + list_w - 2, "v");
        }

        /* Rita alternativen */
        for (int i = 0; i < visible_items; ++i) {
            int item_idx = scroll_offset + i;
            if (item_idx >= BAND_OPTION_COUNT) break;
            int r = pop_y + 1 + i;
            int is_sel = (item_idx == sel_idx);

            if (is_sel) {
                attron(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
                mvprintw(r, pop_x + 1, " > %-6s < ", BAND_OPTIONS[item_idx]);
                attroff(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
            } else {
                attron(COLOR_PAIR(CP_FIELD_INACTIVE));
                mvprintw(r, pop_x + 1, "   %-6s   ", BAND_OPTIONS[item_idx]);
                attroff(COLOR_PAIR(CP_FIELD_INACTIVE));
            }
        }

        refresh();

        int ch = getch();
        if (ch == ERR) continue;

        if (ch == 27) {
            /* Avbryt bandval utan att ändra något */
            copy_str(status_msg, status_sz, "Bandval avbrutet.");
            curs_set(1);
            return 0;
        } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' ') {
            /* Bekräfta val */
            if (!*time_frozen) {
                get_utc_strings(qso->date, sizeof(qso->date),
                                qso->time_utc, sizeof(qso->time_utc), NULL, 0);
                *time_frozen = 1;
            }
            set_band_choice(qso, st, sel_idx);
            snprintf(status_msg, status_sz, "Band valt: %s (%s MHz)", qso->band, qso->freq);
            curs_set(1);
            return 1;
        } else if (ch == '\t') {
            /* Bekräfta och gå framåt */
            if (!*time_frozen) {
                get_utc_strings(qso->date, sizeof(qso->date),
                                qso->time_utc, sizeof(qso->time_utc), NULL, 0);
                *time_frozen = 1;
            }
            set_band_choice(qso, st, sel_idx);
            snprintf(status_msg, status_sz, "Band valt: %s (%s MHz)", qso->band, qso->freq);
            curs_set(1);
            return 2;
        } else if (ch == KEY_BTAB) {
            /* Bekräfta och gå bakåt */
            if (!*time_frozen) {
                get_utc_strings(qso->date, sizeof(qso->date),
                                qso->time_utc, sizeof(qso->time_utc), NULL, 0);
                *time_frozen = 1;
            }
            set_band_choice(qso, st, sel_idx);
            snprintf(status_msg, status_sz, "Band valt: %s (%s MHz)", qso->band, qso->freq);
            curs_set(1);
            return 3;
        } else if (ch == KEY_UP || ch == 'k') {
            sel_idx = (sel_idx - 1 + BAND_OPTION_COUNT) % BAND_OPTION_COUNT;
        } else if (ch == KEY_DOWN || ch == 'j') {
            sel_idx = (sel_idx + 1) % BAND_OPTION_COUNT;
        } else if (ch == KEY_PPAGE) {
            sel_idx -= visible_items;
            if (sel_idx < 0) sel_idx = 0;
        } else if (ch == KEY_NPAGE) {
            sel_idx += visible_items;
            if (sel_idx >= BAND_OPTION_COUNT) sel_idx = BAND_OPTION_COUNT - 1;
        } else if (ch == KEY_HOME) {
            sel_idx = 0;
        } else if (ch == KEY_END) {
            sel_idx = BAND_OPTION_COUNT - 1;
        } else if (ch >= '0' && ch <= '9') {
            for (int b = 0; b < BAND_OPTION_COUNT; ++b) {
                if (BAND_OPTIONS[b][0] == (char)ch) {
                    sel_idx = b;
                    break;
                }
            }
        }
    }
}

/**
 * @brief Returnerar pekare till motsvarande strängfält i QSO-strukturen för ett fältindex.
 */
char *get_qso_field_ptr(QSO *qso, int idx, size_t *max_len) {
    if (qso == NULL) return NULL;
    switch (idx) {
        case FLD_CALL: if (max_len) *max_len = sizeof(qso->call);       return qso->call;
        case FLD_NAME: if (max_len) *max_len = sizeof(qso->name);       return qso->name;
        case FLD_RSTS: if (max_len) *max_len = sizeof(qso->rst_sent);   return qso->rst_sent;
        case FLD_RSTR: if (max_len) *max_len = sizeof(qso->rst_rcvd);   return qso->rst_rcvd;
        case FLD_QTH:  if (max_len) *max_len = sizeof(qso->qth);        return qso->qth;
        case FLD_GRID: if (max_len) *max_len = sizeof(qso->gridsquare); return qso->gridsquare;
        case FLD_DXCC: if (max_len) *max_len = sizeof(qso->dxcc);       return qso->dxcc;
        case FLD_DATE: if (max_len) *max_len = sizeof(qso->date);       return qso->date;
        case FLD_TIME: if (max_len) *max_len = sizeof(qso->time_utc);   return qso->time_utc;
        case FLD_FREQ: if (max_len) *max_len = sizeof(qso->freq);       return qso->freq;
        case FLD_BAND: if (max_len) *max_len = sizeof(qso->band);       return qso->band;
        case FLD_MODE: if (max_len) *max_len = sizeof(qso->mode);       return qso->mode;
        case FLD_COMM: if (max_len) *max_len = sizeof(qso->comment);    return qso->comment;
        default:       if (max_len) *max_len = 0; return NULL;
    }
}

/**
 * @brief Flyttar markören till slutet av ett textfält.
 */
void sync_cursor_to_field_end(QSO *qso, int active_field, int *cursor_pos) {
    if (cursor_pos == NULL) return;
    char *p = get_qso_field_ptr(qso, active_field, NULL);
    *cursor_pos = (p != NULL) ? utf8_display_width(p) : 0;
}

/**
 * @brief Returnerar standardkolumn (0..3) för vertikal piltangentsnavigering.
 */
int field_default_col(int fld) {
    switch (fld) {
        case FLD_CALL: case FLD_QTH:  case FLD_FREQ: case FLD_DATE: return 0;
        case FLD_RSTS: case FLD_GRID: case FLD_BAND: case FLD_TIME: return 1;
        case FLD_RSTR:                case FLD_MODE:                return 2;
        case FLD_NAME: case FLD_DXCC:                               return 3;
        case FLD_COMM:                                              return 2;
        default: return 0;
    }
}

/**
 * @brief Returnerar radindex (0..3) i inmatningsformuläret för ett givet fält.
 */
int field_row_index(int fld) {
    switch (fld) {
        case FLD_CALL: case FLD_RSTS: case FLD_RSTR: case FLD_NAME: return 0;
        case FLD_QTH:  case FLD_GRID: case FLD_DXCC:                return 1;
        case FLD_FREQ: case FLD_BAND: case FLD_MODE:                return 2;
        case FLD_DATE: case FLD_TIME: case FLD_COMM:                return 3;
        default: return 0;
    }
}

/**
 * @brief Stegar aktivt fält uppåt (-1) eller nedåt (+1) baserat på kolumnprioritet.
 */
int step_field_vertical(int current_field, int dir, int pref_col) {
    static const int grid[4][4] = {
        { FLD_CALL, FLD_RSTS, FLD_RSTR, FLD_NAME },
        { FLD_QTH,  FLD_GRID, FLD_DXCC, FLD_DXCC },
        { FLD_FREQ, FLD_BAND, FLD_MODE, FLD_MODE },
        { FLD_DATE, FLD_TIME, FLD_COMM, FLD_COMM }
    };
    int r = field_row_index(current_field);
    int nr = (r + dir + 4) % 4;
    int c = pref_col;
    if (c < 0) c = 0;
    if (c > 3) c = 3;
    return grid[nr][c];
}

/**
 * @brief Återställer formuläret inför inmatning av ett nytt QSO.
 * Fyller i senast använda frekvens, band, mode och sätter klockan i live-läge.
 */
void reset_qso_form(QSO *qso, const StationSettings *st, int *time_frozen, int *active_field,
                    int *field_cursor_pos, int *editing_index, char *last_smcb_call,
                    char *current_qrb) {
    memset(qso, 0, sizeof(*qso));
    if (st != NULL) {
        copy_str(qso->band, sizeof(qso->band), st->last_band);
        copy_str(qso->freq, sizeof(qso->freq), st->last_freq);
        copy_str(qso->mode, sizeof(qso->mode), st->last_mode);
        if (st->auto_rst_59) {
            const char *def_rst = (strcmp(qso->mode, "CW") == 0) ? "599" : "59";
            copy_str(qso->rst_sent, sizeof(qso->rst_sent), def_rst);
            copy_str(qso->rst_rcvd, sizeof(qso->rst_rcvd), def_rst);
        }
    }
    *time_frozen = 0;
    *active_field = FLD_CALL;
    *field_cursor_pos = 0;
    *editing_index = -1;
    if (last_smcb_call) last_smcb_call[0] = '\0';
    if (current_qrb) copy_str(current_qrb, 32, "-");

    get_utc_strings(qso->date, sizeof(qso->date),
                    qso->time_utc, sizeof(qso->time_utc),
                    NULL, 0);
}
