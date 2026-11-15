/**
 * @file tui.c
 * @brief Terminal User Interface (TUI) med ncurses, färgscheman (inkl. gråskala och ambra),
 * rymdvädersbox från hf.avitec.se, loggradshantering och hjälpskärmar.
 */

#include "tui.h"
#include "db.h"
#include "dxcc.h"
#include "logg.h"
#include "qrb.h"
#include "space_weather.h"
#include "util.h"
#include <ctype.h>
#include "theme_editor.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Färgscheman (UEI-tema som default, Borland Turbo Pascal, Midnight Commander, ren gråskala, ambra, arktisk natt, field day, mörkerläge, solarized dark, dracula, militärradio samt egen profil) */
const char * const THEME_NAMES[] = {
    "UEI-tema",
    "Turbo Pascal (MC Default)",
    "MC Darkfar (Svart/Blå)",
    "MC Gotar (Svart/Gul)",
    "MC Sand (Ljusgrå/Blå)",
    "MC Matrix (Fosforgrön)",
    "Ljust (Grå/Orange/Blå)",
    "Monokrom gråskala",
    "Ambra (Retro CRT)",
    "Arktisk Natt (Nord Frost)",
    "Field Day (Vintage Radio)",
    "Mörkerläge (Crimson Night)",
    "Solarized Dark (Djuphav)",
    "Dracula (Neon Skymning)",
    "Militärradio (Fältgrön)",
    "Egen profil (Anpassad)"
};

static CustomTheme g_custom_theme;
static int g_custom_theme_init = 0;

void set_custom_theme(const CustomTheme *ct) {
    if (ct != NULL) {
        g_custom_theme = *ct;
        g_custom_theme_init = 1;
    }
}

void get_custom_theme(CustomTheme *ct) {
    if (ct != NULL) {
        if (!g_custom_theme_init) {
            custom_theme_init_defaults(&g_custom_theme);
            g_custom_theme_init = 1;
        }
        *ct = g_custom_theme;
    }
}

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
    curs_set(1);
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
 * @brief Aktiverar och konfigurerar ett av de 14 tillgängliga färgschemana i ncurses.
 * Inkluderar modern gråskala, retro ambra, arktisk natt, field day, crimson mörkerläge,
 * solarized dark, dracula och militärradio.
 * @param theme_idx Index på schemat (0..THEME_COUNT-1).
 */
void apply_color_theme(int theme_idx) {
    if (!has_colors()) return;
    start_color();

    if (theme_idx < 0 || theme_idx >= THEME_COUNT) {
        theme_idx = THEME_DEFAULT;
    }

    switch (theme_idx) {
        case 0: { /* 1. UEI-tema (Standard - Marinblå bas, solgula accenter, skogsgröna ramar och bärnstensfokus) */
            static const CustomTheme uei_theme = {
                4,  /* bg_main:       Marinblå (Mörk) */
                14, /* fg_text:       Solgul (Klar) */
                11, /* border:        Skogsgrön */
                13, /* border_active: Bärnsten / Guld */
                1,  /* title:         Kritvit */
                14, /* label:         Solgul (Klar) */
                3,  /* field_act_bg:  Mörkgrå */
                15, /* field_act_fg:  Orange (Varm) */
                5,  /* sel_bg:        Havsblå */
                15, /* sel_fg:        Orange (Varm) */
                2,  /* topbar_bg:     Ljusgrå */
                3,  /* topbar_fg:     Mörkgrå */
                11, /* status_ok:     Skogsgrön */
                17  /* status_warn:   Klar röd */
            };
            apply_custom_theme_pairs(&uei_theme);
            break;
        }

        case 1: /* 2. Turbo Pascal / MC Default (Klassisk blå bakgrund) */
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

        case 2: /* 3. MC Darkfar (Svart bakgrund med blå accenter och gråa ramar) */
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

        case 3: /* 4. MC Gotar (Svart bakgrund med gula detaljer och gråa ramar) */
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

        case 4: /* 5. MC Sand / Gray (Ljusgrå/vit bakgrund med blå och svarta accenter) */
            init_pair(CP_MAIN_BG,        COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_TOPBAR,         COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_TOPBAR_ACCENT,  COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_BORDER,         COLOR_BLUE,   COLOR_WHITE);
            init_pair(CP_BORDER_ACTIVE,  COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_TITLE,          COLOR_BLUE,   COLOR_WHITE);
            init_pair(CP_LABEL,          COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_FIELD_INACTIVE, COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_FIELD_ACTIVE,   COLOR_WHITE,  COLOR_BLUE);
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

        case 5: /* 6. MC Matrix (Klassisk fosforgrön terminalskärm på svart bakgrund) */
            init_pair(CP_MAIN_BG,        COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_TOPBAR,         COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_TOPBAR_ACCENT,  COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_BORDER,         COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_BORDER_ACTIVE,  COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_TITLE,          COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_LABEL,          COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_FIELD_INACTIVE, COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_FIELD_ACTIVE,   COLOR_BLACK,  COLOR_GREEN);
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

        case 6: { /* 7. Ljust färgschema (Ljusgrå bakgrund, svart text, orange ramar, blå highlights) */
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
            init_pair(CP_FIELD_ACTIVE,   COLOR_WHITE,  COLOR_BLUE);
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

        case 7: { /* 8. Monokrom gråskala (Allt i harmoniska nyanser av grått och vitt på svart) */
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

        case 8: { /* 9. Ambra / Retro CRT (Varm orange monokrom skärm på svart bakgrund) */
            short amb_bright = (COLORS >= 256) ? 208 : COLOR_YELLOW; /* Klar varm orange (#ff8700) */
            short amb_mid    = (COLORS >= 256) ? 166 : COLOR_YELLOW; /* Djup bärnstens-orange (#d75f00) */
            short amb_dark   = (COLORS >= 256) ? 130 : COLOR_RED;    /* Mörk bärnsten (#af5f00) */
            short amb_bg     = COLOR_BLACK;

            init_pair(CP_MAIN_BG,        amb_bright,   amb_bg);
            init_pair(CP_TOPBAR,         amb_bg,       amb_bright);
            init_pair(CP_TOPBAR_ACCENT,  amb_dark,     amb_bright);
            init_pair(CP_BORDER,         amb_mid,      amb_bg);
            init_pair(CP_BORDER_ACTIVE,  amb_bright,   amb_bg);
            init_pair(CP_TITLE,          amb_bright,   amb_bg);
            init_pair(CP_LABEL,          amb_bright,   amb_bg);
            init_pair(CP_FIELD_INACTIVE, amb_bright,   amb_bg);
            init_pair(CP_FIELD_ACTIVE,   amb_bg,       amb_bright);
            init_pair(CP_FROZEN_BADGE,   amb_bg,       amb_mid);
            init_pair(CP_LIVE_BADGE,     amb_bg,       amb_bright);
            init_pair(CP_EDIT_BADGE,     amb_bright,   amb_dark);
            init_pair(CP_TABLE_HEADER,   amb_bright,   amb_bg);
            init_pair(CP_LOG_ROW,        amb_bright,   amb_bg);
            init_pair(CP_LOG_SELECTED,   amb_bg,       amb_bright);
            init_pair(CP_LOG_MARKED,     amb_bg,       amb_mid);
            init_pair(CP_FKEY_NUM,       amb_bg,       amb_dark);
            init_pair(CP_FKEY_LABEL,     amb_bg,       amb_bright);
            init_pair(CP_STATUS_OK,      amb_bright,   amb_bg);
            init_pair(CP_STATUS_WARN,    amb_bg,       amb_bright);
            init_pair(CP_FLAG_NEW,         amb_bright,   amb_bg);
            init_pair(CP_FLAG_HANDLED,     amb_mid,      amb_bg);
            init_pair(CP_FLAG_NEW_SEL,     amb_bg,       amb_bright);
            init_pair(CP_FLAG_HANDLED_SEL, amb_mid,      amb_bright);
            init_pair(CP_FLAG_NEW_MRK,     amb_bright,   amb_mid);
            init_pair(CP_FLAG_HANDLED_MRK, amb_bg,       amb_mid);
            init_pair(CP_SW_NORMAL,        amb_bright,   amb_bg);
            init_pair(CP_SW_OK,            amb_bright,   amb_bg);
            init_pair(CP_SW_WARN,          amb_mid,      amb_bg);
            init_pair(CP_SW_BAD,           amb_bright,   amb_bg);
            init_pair(CP_SW_SEVERE,        amb_bg,       amb_bright);
            break;
        }

        case 9: { /* 10. Arktisk Natt (Nord Frost - Dämpad skifferblå polarnatt med isblå frost och norrsken) */
            short bg_nord       = (COLORS >= 256) ? 236 : COLOR_BLACK;
            short fg_snow       = (COLORS >= 256) ? 254 : COLOR_WHITE;
            short fg_frost      = (COLORS >= 256) ? 117 : COLOR_CYAN;
            short fg_ice        = (COLORS >= 256) ? 153 : COLOR_CYAN;
            short fg_border_dim = (COLORS >= 256) ? 67  : COLOR_CYAN;
            short fg_aurora_grn = (COLORS >= 256) ? 150 : COLOR_GREEN;
            short fg_aurora_yel = (COLORS >= 256) ? 222 : COLOR_YELLOW;
            short fg_aurora_red = (COLORS >= 256) ? 203 : COLOR_RED;
            short fg_aurora_pur = (COLORS >= 256) ? 139 : COLOR_MAGENTA;
            short sel_bg        = (COLORS >= 256) ? 111 : COLOR_CYAN;

            init_pair(CP_MAIN_BG,        fg_snow,       bg_nord);
            init_pair(CP_TOPBAR,         bg_nord,       fg_frost);
            init_pair(CP_TOPBAR_ACCENT,  fg_aurora_red, fg_frost);
            init_pair(CP_BORDER,         fg_border_dim, bg_nord);
            init_pair(CP_BORDER_ACTIVE,  fg_frost,      bg_nord);
            init_pair(CP_TITLE,          fg_frost,      bg_nord);
            init_pair(CP_LABEL,          fg_ice,        bg_nord);
            init_pair(CP_FIELD_INACTIVE, fg_snow,       bg_nord);
            init_pair(CP_FIELD_ACTIVE,   bg_nord,       fg_frost);
            init_pair(CP_FROZEN_BADGE,   bg_nord,       fg_aurora_yel);
            init_pair(CP_LIVE_BADGE,     bg_nord,       fg_aurora_grn);
            init_pair(CP_EDIT_BADGE,     fg_snow,       fg_aurora_pur);
            init_pair(CP_TABLE_HEADER,   fg_frost,      bg_nord);
            init_pair(CP_LOG_ROW,        fg_snow,       bg_nord);
            init_pair(CP_LOG_SELECTED,   bg_nord,       sel_bg);
            init_pair(CP_LOG_MARKED,     fg_snow,       fg_aurora_pur);
            init_pair(CP_FKEY_NUM,       bg_nord,       fg_frost);
            init_pair(CP_FKEY_LABEL,     fg_snow,       bg_nord);
            init_pair(CP_STATUS_OK,      fg_frost,      bg_nord);
            init_pair(CP_STATUS_WARN,    fg_snow,       fg_aurora_red);
            init_pair(CP_FLAG_NEW,         fg_aurora_red, bg_nord);
            init_pair(CP_FLAG_HANDLED,     fg_aurora_grn, bg_nord);
            init_pair(CP_FLAG_NEW_SEL,     fg_aurora_red, sel_bg);
            init_pair(CP_FLAG_HANDLED_SEL, bg_nord,       sel_bg);
            init_pair(CP_FLAG_NEW_MRK,     fg_aurora_red, fg_aurora_pur);
            init_pair(CP_FLAG_HANDLED_MRK, fg_aurora_grn, fg_aurora_pur);
            init_pair(CP_SW_NORMAL,        fg_ice,        bg_nord);
            init_pair(CP_SW_OK,            fg_aurora_grn, bg_nord);
            init_pair(CP_SW_WARN,          fg_aurora_yel, bg_nord);
            init_pair(CP_SW_BAD,           fg_aurora_red, bg_nord);
            init_pair(CP_SW_SEVERE,        fg_aurora_pur, bg_nord);
            break;
        }

        case 10: { /* 11. Field Day (Vintage Radio / Gruvbox - Varm mörkbrun bas, bärnsten och analog stationskänsla) */
            short bg_vintage   = (COLORS >= 256) ? 235 : COLOR_BLACK;
            short fg_cream     = (COLORS >= 256) ? 229 : COLOR_WHITE;
            short fg_amber     = (COLORS >= 256) ? 214 : COLOR_YELLOW;
            short fg_bronze    = (COLORS >= 256) ? 137 : COLOR_YELLOW;
            short fg_sage      = (COLORS >= 256) ? 107 : COLOR_GREEN;
            short fg_rust      = (COLORS >= 256) ? 166 : COLOR_YELLOW;
            short fg_brick_red = (COLORS >= 256) ? 167 : COLOR_RED;
            short fg_mauve     = (COLORS >= 256) ? 175 : COLOR_MAGENTA;
            short sel_bg       = (COLORS >= 256) ? 214 : COLOR_YELLOW;

            init_pair(CP_MAIN_BG,        fg_cream,     bg_vintage);
            init_pair(CP_TOPBAR,         bg_vintage,   fg_amber);
            init_pair(CP_TOPBAR_ACCENT,  fg_brick_red, fg_amber);
            init_pair(CP_BORDER,         fg_bronze,    bg_vintage);
            init_pair(CP_BORDER_ACTIVE,  fg_amber,     bg_vintage);
            init_pair(CP_TITLE,          fg_amber,     bg_vintage);
            init_pair(CP_LABEL,          fg_bronze,    bg_vintage);
            init_pair(CP_FIELD_INACTIVE, fg_cream,     bg_vintage);
            init_pair(CP_FIELD_ACTIVE,   bg_vintage,   fg_amber);
            init_pair(CP_FROZEN_BADGE,   bg_vintage,   fg_rust);
            init_pair(CP_LIVE_BADGE,     bg_vintage,   fg_sage);
            init_pair(CP_EDIT_BADGE,     fg_cream,     fg_mauve);
            init_pair(CP_TABLE_HEADER,   fg_amber,     bg_vintage);
            init_pair(CP_LOG_ROW,        fg_cream,     bg_vintage);
            init_pair(CP_LOG_SELECTED,   bg_vintage,   sel_bg);
            init_pair(CP_LOG_MARKED,     fg_cream,     fg_mauve);
            init_pair(CP_FKEY_NUM,       bg_vintage,   fg_bronze);
            init_pair(CP_FKEY_LABEL,     fg_cream,     bg_vintage);
            init_pair(CP_STATUS_OK,      fg_amber,     bg_vintage);
            init_pair(CP_STATUS_WARN,    fg_cream,     fg_brick_red);
            init_pair(CP_FLAG_NEW,         fg_brick_red, bg_vintage);
            init_pair(CP_FLAG_HANDLED,     fg_sage,      bg_vintage);
            init_pair(CP_FLAG_NEW_SEL,     fg_brick_red, sel_bg);
            init_pair(CP_FLAG_HANDLED_SEL, bg_vintage,   sel_bg);
            init_pair(CP_FLAG_NEW_MRK,     fg_brick_red, fg_mauve);
            init_pair(CP_FLAG_HANDLED_MRK, fg_sage,      fg_mauve);
            init_pair(CP_SW_NORMAL,        fg_cream,     bg_vintage);
            init_pair(CP_SW_OK,            fg_sage,      bg_vintage);
            init_pair(CP_SW_WARN,          fg_rust,      bg_vintage);
            init_pair(CP_SW_BAD,           fg_brick_red, bg_vintage);
            init_pair(CP_SW_SEVERE,        fg_mauve,     bg_vintage);
            break;
        }

        case 11: { /* 12. Mörkerläge (Crimson Night - Skonsamt röd-svart tema för nattpass utan att störa mörkerseendet) */
            short bg_night     = (COLORS >= 256) ? 232 : COLOR_BLACK;
            short fg_bright    = (COLORS >= 256) ? 196 : COLOR_RED;
            short fg_light_red = (COLORS >= 256) ? 203 : COLOR_RED;
            short fg_mid_red   = (COLORS >= 256) ? 160 : COLOR_RED;
            short fg_dim_red   = (COLORS >= 256) ? 88  : COLOR_RED;

            init_pair(CP_MAIN_BG,        fg_light_red, bg_night);
            init_pair(CP_TOPBAR,         bg_night,     fg_mid_red);
            init_pair(CP_TOPBAR_ACCENT,  fg_bright,    bg_night);
            init_pair(CP_BORDER,         fg_dim_red,   bg_night);
            init_pair(CP_BORDER_ACTIVE,  fg_bright,    bg_night);
            init_pair(CP_TITLE,          fg_bright,    bg_night);
            init_pair(CP_LABEL,          fg_mid_red,   bg_night);
            init_pair(CP_FIELD_INACTIVE, fg_light_red, bg_night);
            init_pair(CP_FIELD_ACTIVE,   bg_night,     fg_bright);
            init_pair(CP_FROZEN_BADGE,   bg_night,     fg_mid_red);
            init_pair(CP_LIVE_BADGE,     bg_night,     fg_bright);
            init_pair(CP_EDIT_BADGE,     fg_bright,    fg_dim_red);
            init_pair(CP_TABLE_HEADER,   fg_bright,    bg_night);
            init_pair(CP_LOG_ROW,        fg_light_red, bg_night);
            init_pair(CP_LOG_SELECTED,   bg_night,     fg_mid_red);
            init_pair(CP_LOG_MARKED,     fg_bright,    fg_dim_red);
            init_pair(CP_FKEY_NUM,       bg_night,     fg_dim_red);
            init_pair(CP_FKEY_LABEL,     fg_light_red, bg_night);
            init_pair(CP_STATUS_OK,      fg_bright,    bg_night);
            init_pair(CP_STATUS_WARN,    bg_night,     fg_bright);
            init_pair(CP_FLAG_NEW,         fg_bright,    bg_night);
            init_pair(CP_FLAG_HANDLED,     fg_dim_red,   bg_night);
            init_pair(CP_FLAG_NEW_SEL,     fg_bright,    fg_mid_red);
            init_pair(CP_FLAG_HANDLED_SEL, bg_night,     fg_mid_red);
            init_pair(CP_FLAG_NEW_MRK,     fg_bright,    fg_dim_red);
            init_pair(CP_FLAG_HANDLED_MRK, bg_night,     fg_dim_red);
            init_pair(CP_SW_NORMAL,        fg_mid_red,   bg_night);
            init_pair(CP_SW_OK,            fg_dim_red,   bg_night);
            init_pair(CP_SW_WARN,          fg_light_red, bg_night);
            init_pair(CP_SW_BAD,           fg_bright,    bg_night);
            init_pair(CP_SW_SEVERE,        bg_night,     fg_bright);
            break;
        }

        case 12: { /* 13. Solarized Dark (Djuphav - Ethan Schoonovers vetenskapligt balanserade mörkblå marinpalett) */
            short bg_sol     = (COLORS >= 256) ? 234 : COLOR_BLACK;
            short fg_base0   = (COLORS >= 256) ? 250 : COLOR_WHITE;
            short fg_base1   = (COLORS >= 256) ? 254 : COLOR_WHITE;
            short fg_base01  = (COLORS >= 256) ? 241 : COLOR_CYAN;
            short fg_cyan    = (COLORS >= 256) ? 37  : COLOR_CYAN;
            short fg_blue    = (COLORS >= 256) ? 33  : COLOR_BLUE;
            short fg_yellow  = (COLORS >= 256) ? 178 : COLOR_YELLOW;
            short fg_orange  = (COLORS >= 256) ? 166 : COLOR_YELLOW;
            short fg_red     = (COLORS >= 256) ? 160 : COLOR_RED;
            short fg_magenta = (COLORS >= 256) ? 134 : COLOR_MAGENTA;
            short fg_green   = (COLORS >= 256) ? 106 : COLOR_GREEN;
            short sel_bg     = (COLORS >= 256) ? 24  : COLOR_BLUE;

            init_pair(CP_MAIN_BG,        fg_base0,     bg_sol);
            init_pair(CP_TOPBAR,         bg_sol,       fg_cyan);
            init_pair(CP_TOPBAR_ACCENT,  fg_yellow,    fg_cyan);
            init_pair(CP_BORDER,         fg_base01,    bg_sol);
            init_pair(CP_BORDER_ACTIVE,  fg_cyan,      bg_sol);
            init_pair(CP_TITLE,          fg_cyan,      bg_sol);
            init_pair(CP_LABEL,          fg_blue,      bg_sol);
            init_pair(CP_FIELD_INACTIVE, fg_base0,     bg_sol);
            init_pair(CP_FIELD_ACTIVE,   bg_sol,       fg_cyan);
            init_pair(CP_FROZEN_BADGE,   bg_sol,       fg_yellow);
            init_pair(CP_LIVE_BADGE,     bg_sol,       fg_green);
            init_pair(CP_EDIT_BADGE,     fg_base1,     fg_magenta);
            init_pair(CP_TABLE_HEADER,   fg_cyan,      bg_sol);
            init_pair(CP_LOG_ROW,        fg_base0,     bg_sol);
            init_pair(CP_LOG_SELECTED,   fg_base1,     sel_bg);
            init_pair(CP_LOG_MARKED,     fg_base1,     fg_magenta);
            init_pair(CP_FKEY_NUM,       bg_sol,       fg_base01);
            init_pair(CP_FKEY_LABEL,     fg_base0,     bg_sol);
            init_pair(CP_STATUS_OK,      fg_cyan,      bg_sol);
            init_pair(CP_STATUS_WARN,    fg_base1,     fg_red);
            init_pair(CP_FLAG_NEW,         fg_red,       bg_sol);
            init_pair(CP_FLAG_HANDLED,     fg_green,     bg_sol);
            init_pair(CP_FLAG_NEW_SEL,     fg_red,       sel_bg);
            init_pair(CP_FLAG_HANDLED_SEL, fg_green,     sel_bg);
            init_pair(CP_FLAG_NEW_MRK,     fg_red,       fg_magenta);
            init_pair(CP_FLAG_HANDLED_MRK, fg_green,     fg_magenta);
            init_pair(CP_SW_NORMAL,        fg_base0,     bg_sol);
            init_pair(CP_SW_OK,            fg_green,     bg_sol);
            init_pair(CP_SW_WARN,          fg_yellow,    bg_sol);
            init_pair(CP_SW_BAD,           fg_orange,    bg_sol);
            init_pair(CP_SW_SEVERE,        fg_magenta,   bg_sol);
            break;
        }

        case 13: { /* 14. Dracula (Neon Skymning - Modern mörk violett palett med neonlila, cyan och rosa accenter) */
            short bg_drac        = (COLORS >= 256) ? 236 : COLOR_BLACK;
            short fg_drac_fg     = (COLORS >= 256) ? 255 : COLOR_WHITE;
            short fg_drac_purple = (COLORS >= 256) ? 141 : COLOR_MAGENTA;
            short fg_drac_cyan   = (COLORS >= 256) ? 117 : COLOR_CYAN;
            short fg_drac_green  = (COLORS >= 256) ? 84  : COLOR_GREEN;
            short fg_drac_yellow = (COLORS >= 256) ? 228 : COLOR_YELLOW;
            short fg_drac_orange = (COLORS >= 256) ? 215 : COLOR_YELLOW;
            short fg_drac_pink   = (COLORS >= 256) ? 212 : COLOR_MAGENTA;
            short fg_drac_red    = (COLORS >= 256) ? 203 : COLOR_RED;
            short fg_drac_slate  = (COLORS >= 256) ? 61  : COLOR_BLUE;
            short sel_drac_bg    = (COLORS >= 256) ? 60  : COLOR_MAGENTA;

            init_pair(CP_MAIN_BG,        fg_drac_fg,     bg_drac);
            init_pair(CP_TOPBAR,         bg_drac,        fg_drac_purple);
            init_pair(CP_TOPBAR_ACCENT,  fg_drac_pink,   fg_drac_purple);
            init_pair(CP_BORDER,         fg_drac_slate,  bg_drac);
            init_pair(CP_BORDER_ACTIVE,  fg_drac_purple, bg_drac);
            init_pair(CP_TITLE,          fg_drac_cyan,   bg_drac);
            init_pair(CP_LABEL,          fg_drac_pink,   bg_drac);
            init_pair(CP_FIELD_INACTIVE, fg_drac_fg,     bg_drac);
            init_pair(CP_FIELD_ACTIVE,   bg_drac,        fg_drac_cyan);
            init_pair(CP_FROZEN_BADGE,   bg_drac,        fg_drac_orange);
            init_pair(CP_LIVE_BADGE,     bg_drac,        fg_drac_green);
            init_pair(CP_EDIT_BADGE,     fg_drac_fg,     fg_drac_pink);
            init_pair(CP_TABLE_HEADER,   fg_drac_purple, bg_drac);
            init_pair(CP_LOG_ROW,        fg_drac_fg,     bg_drac);
            init_pair(CP_LOG_SELECTED,   fg_drac_fg,     sel_drac_bg);
            init_pair(CP_LOG_MARKED,     fg_drac_fg,     fg_drac_pink);
            init_pair(CP_FKEY_NUM,       bg_drac,        fg_drac_purple);
            init_pair(CP_FKEY_LABEL,     fg_drac_fg,     bg_drac);
            init_pair(CP_STATUS_OK,      fg_drac_cyan,   bg_drac);
            init_pair(CP_STATUS_WARN,    fg_drac_fg,     fg_drac_red);
            init_pair(CP_FLAG_NEW,         fg_drac_red,    bg_drac);
            init_pair(CP_FLAG_HANDLED,     fg_drac_green,  bg_drac);
            init_pair(CP_FLAG_NEW_SEL,     fg_drac_red,    sel_drac_bg);
            init_pair(CP_FLAG_HANDLED_SEL, fg_drac_green,  sel_drac_bg);
            init_pair(CP_FLAG_NEW_MRK,     fg_drac_red,    fg_drac_pink);
            init_pair(CP_FLAG_HANDLED_MRK, fg_drac_green,  fg_drac_pink);
            init_pair(CP_SW_NORMAL,        fg_drac_cyan,   bg_drac);
            init_pair(CP_SW_OK,            fg_drac_green,  bg_drac);
            init_pair(CP_SW_WARN,          fg_drac_yellow, bg_drac);
            init_pair(CP_SW_BAD,           fg_drac_red,    bg_drac);
            init_pair(CP_SW_SEVERE,        fg_drac_pink,   bg_drac);
            break;
        }

        case 14: { /* 15. Militärradio (Fältgrön - Taktisk olivdrab bas, khaki duktext och mässingsdetaljer) */
            short bg_tac     = (COLORS >= 256) ? 234 : COLOR_BLACK;
            short fg_khaki   = (COLORS >= 256) ? 187 : COLOR_WHITE;
            short fg_olive   = (COLORS >= 256) ? 107 : COLOR_GREEN;
            short fg_camo    = (COLORS >= 256) ? 65  : COLOR_GREEN;
            short fg_brass   = (COLORS >= 256) ? 178 : COLOR_YELLOW;
            short fg_sig_yel = (COLORS >= 256) ? 221 : COLOR_YELLOW;
            short fg_haz_red = (COLORS >= 256) ? 167 : COLOR_RED;
            short fg_sig_orn = (COLORS >= 256) ? 172 : COLOR_YELLOW;
            short sel_tac_bg = (COLORS >= 256) ? 58  : COLOR_GREEN;

            init_pair(CP_MAIN_BG,        fg_khaki,     bg_tac);
            init_pair(CP_TOPBAR,         bg_tac,       fg_brass);
            init_pair(CP_TOPBAR_ACCENT,  fg_haz_red,   fg_brass);
            init_pair(CP_BORDER,         fg_camo,      bg_tac);
            init_pair(CP_BORDER_ACTIVE,  fg_brass,     bg_tac);
            init_pair(CP_TITLE,          fg_brass,     bg_tac);
            init_pair(CP_LABEL,          fg_olive,     bg_tac);
            init_pair(CP_FIELD_INACTIVE, fg_khaki,     bg_tac);
            init_pair(CP_FIELD_ACTIVE,   bg_tac,       fg_brass);
            init_pair(CP_FROZEN_BADGE,   bg_tac,       fg_sig_orn);
            init_pair(CP_LIVE_BADGE,     bg_tac,       fg_olive);
            init_pair(CP_EDIT_BADGE,     fg_khaki,     fg_sig_orn);
            init_pair(CP_TABLE_HEADER,   fg_brass,     bg_tac);
            init_pair(CP_LOG_ROW,        fg_khaki,     bg_tac);
            init_pair(CP_LOG_SELECTED,   fg_brass,     sel_tac_bg);
            init_pair(CP_LOG_MARKED,     fg_khaki,     fg_sig_orn);
            init_pair(CP_FKEY_NUM,       bg_tac,       fg_olive);
            init_pair(CP_FKEY_LABEL,     fg_khaki,     bg_tac);
            init_pair(CP_STATUS_OK,      fg_brass,     bg_tac);
            init_pair(CP_STATUS_WARN,    fg_khaki,     fg_haz_red);
            init_pair(CP_FLAG_NEW,         fg_haz_red,   bg_tac);
            init_pair(CP_FLAG_HANDLED,     fg_olive,     bg_tac);
            init_pair(CP_FLAG_NEW_SEL,     fg_haz_red,   sel_tac_bg);
            init_pair(CP_FLAG_HANDLED_SEL, fg_khaki,     sel_tac_bg);
            init_pair(CP_FLAG_NEW_MRK,     fg_haz_red,   fg_sig_orn);
            init_pair(CP_FLAG_HANDLED_MRK, fg_olive,     fg_sig_orn);
            init_pair(CP_SW_NORMAL,        fg_khaki,     bg_tac);
            init_pair(CP_SW_OK,            fg_olive,     bg_tac);
            init_pair(CP_SW_WARN,          fg_sig_yel,   bg_tac);
            init_pair(CP_SW_BAD,           fg_haz_red,   bg_tac);
            init_pair(CP_SW_SEVERE,        fg_sig_orn,   bg_tac);
            break;
        }

        case 15: { /* 16. Egen profil (Användaranpassat färgschema från editorn) */
            if (!g_custom_theme_init) {
                custom_theme_init_defaults(&g_custom_theme);
                g_custom_theme_init = 1;
            }
            apply_custom_theme_pairs(&g_custom_theme);
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
    const char *my_call = clean_call[0] ? clean_call : "SM5UEI";
    const char *my_qth  = (st != NULL && st->qth[0] != '\0') ? st->qth : "";
    const char *my_grid = (st != NULL && st->gridsquare[0] != '\0') ? st->gridsquare : "";

    if (my_qth[0] != '\0' && my_grid[0] != '\0') {
        snprintf(station_info, sizeof(station_info), "%s %s %s", my_call, my_qth, my_grid);
    } else if (my_qth[0] != '\0') {
        snprintf(station_info, sizeof(station_info), "%s %s", my_call, my_qth);
    } else if (my_grid[0] != '\0') {
        snprintf(station_info, sizeof(station_info), "%s %s", my_call, my_grid);
    } else {
        snprintf(station_info, sizeof(station_info), "%s", my_call);
    }

    int info_w = utf8_display_width(station_info);
    int info_col = (COLS - info_w) / 2;
    if (info_col < 14) info_col = 14;

    char time_str[64];
    get_utc_strings(NULL, 0, NULL, 0, time_str, sizeof(time_str));
    int time_col = COLS - utf8_display_width(time_str) - 2;
    if (time_col > info_col + info_w) {
        attron(COLOR_PAIR(CP_TOPBAR) | A_BOLD);
        mvprintw(0, info_col, "%s", station_info);
        attroff(COLOR_PAIR(CP_TOPBAR) | A_BOLD);
        mvprintw(0, time_col, "%s", time_str);
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
 * Om fönstret har minst 30 rader och statistikrutan är aktiverad visas statistikrutan (4 rader).
 * Om fönstret har minst 25 rader visas rymdvädersrutan (4 rader).
 * Vid färre än 25 rader döljs båda rutorna så att loggraderna och inmatningen får full höjd.
 * @param st Stationsinställningar (eller NULL för standard).
 * @return Antal synliga rader i loggen.
 */
int get_avail_log_rows(const StationSettings *st) {
    int show_stats = (st == NULL || st->show_stats);
    int stats_reserved = (show_stats && LINES >= 30) ? 4 : 0;
    int top_reserved   = 1 + stats_reserved + 6; /* Topbar (1) + Stats (0 el. 4) + Input box (6) */
    int bot_reserved   = 1;                      /* Endast F-tangentrad (1) */
    int show_sw        = (st == NULL || st->show_space_weather);
    int sw_reserved    = (show_sw && LINES >= 25) ? 4 : 0;  /* Rymdvädersbox (0 el. 4) */
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
    int lbl_attr = is_active ? (COLOR_PAIR(CP_BORDER_ACTIVE) | A_BOLD) : COLOR_PAIR(CP_LABEL);
    attron(lbl_attr);
    int llen = utf8_display_width(label);
    mvprintw(row, colon_col - llen, "%s:", label);
    attroff(lbl_attr);

    int val_col = colon_col + 2;
    int color = is_active ? CP_FIELD_ACTIVE : CP_FIELD_INACTIVE;
    int attr  = is_active ? A_BOLD : 0;

    attron(COLOR_PAIR(color) | attr);
    for (int i = 0; i < width; ++i) {
        mvaddch(row, val_col + i, ' ');
    }

    int val_w = utf8_display_width(val);
    if (cursor_pos < 0) cursor_pos = 0;
    if (cursor_pos > val_w) cursor_pos = val_w;

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
        int c_offset = cursor_pos - scroll_chars;
        if (c_offset < 0) c_offset = 0;
        if (c_offset >= width) c_offset = width - 1;

        attron(COLOR_PAIR(color) | A_BOLD | A_UNDERLINE);
        if (cursor_pos < val_w) {
            size_t cb_start = utf8_char_to_byte(val, cursor_pos);
            size_t cb_end   = utf8_char_to_byte(val, cursor_pos + 1);
            mvprintw(row, val_col + c_offset, "%.*s", (int)(cb_end - cb_start), val + cb_start);
        } else {
            mvaddch(row, val_col + c_offset, ' ');
        }
        attroff(COLOR_PAIR(color) | A_BOLD | A_UNDERLINE);

        if (cur_y) *cur_y = row;
        if (cur_x) *cur_x = val_col + c_offset;
    }
}

/**
 * @brief Ritar det anpassade bandvalsfältet.
 */
void draw_band_selector_aligned(int row, int colon_col, const char *val,
                                int is_active, int band_selecting,
                                int *cur_y, int *cur_x) {
    int lbl_attr = is_active ? (COLOR_PAIR(CP_BORDER_ACTIVE) | A_BOLD) : COLOR_PAIR(CP_LABEL);
    attron(lbl_attr);
    int llen = utf8_display_width("Band");
    mvprintw(row, colon_col - llen, "Band:");
    attroff(lbl_attr);

    int val_col = colon_col + 2;
    int width = 8;

    char formatted[16];
    format_band_display(val, formatted, sizeof(formatted));

    int color = is_active ? CP_FIELD_ACTIVE : CP_FIELD_INACTIVE;
    int attr  = is_active ? A_BOLD : 0;
    if (band_selecting) {
        attr |= A_REVERSE;
    }

    attron(COLOR_PAIR(color) | attr);
    for (int i = 0; i < width; ++i) {
        mvaddch(row, val_col + i, ' ');
    }
    mvprintw(row, val_col, "%-8s", formatted);
    attroff(COLOR_PAIR(color) | attr);

    if (is_active) {
        *cur_y = row;
        *cur_x = val_col;
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
            items[count++] = (FKeyItem){"^L", "QRZ"};
            items[count++] = (FKeyItem){"F4", "Fokus"};
            items[count++] = (FKeyItem){"F5", "Hant/Ny"};
            items[count++] = (FKeyItem){"F6", "Noter"};
            items[count++] = (FKeyItem){"F7", "Alla hant."};
            items[count++] = (FKeyItem){"F8", "Radera"};
            items[count++] = (FKeyItem){"F12", "Inställn."};
        } else {
            items[count++] = (FKeyItem){"F1", "Rensa"};
            if (search_filter && search_filter[0]) {
                items[count++] = (FKeyItem){"Esc", "Återställ"};
            }
            items[count++] = (FKeyItem){"F2", (COLS >= 92) ? "Klocka" : "UTC"};
            items[count++] = (FKeyItem){"^L", "QRZ"};
            items[count++] = (FKeyItem){"F3", (search_filter && search_filter[0]) ? "Sök*" : "Sök"};
            items[count++] = (FKeyItem){"F4", "Logg"};
            items[count++] = (FKeyItem){"F5", (COLS >= 92) ? "Uppslag" : "Uppsl"};
            items[count++] = (FKeyItem){"F6", "Noter"};
            items[count++] = (FKeyItem){"F7", "QRB"};
            items[count++] = (FKeyItem){"F9", "Hjälp"};
            items[count++] = (FKeyItem){"F10", (COLS >= 92) ? "Avsluta" : "Avsl"};
            items[count++] = (FKeyItem){"F12", (COLS >= 92) ? "Inställn." : "Inst"};
        }
    }

    int col = 0;
    for (int i = 0; i < count; ++i) {
        int klen = (int)strlen(items[i].fkey);
        int llen = utf8_display_width(items[i].label);
        if (col + klen + llen > COLS) break;

        attron(COLOR_PAIR(CP_FKEY_NUM) | A_BOLD);
        mvprintw(y, col, "%s", items[i].fkey);
        attroff(COLOR_PAIR(CP_FKEY_NUM) | A_BOLD);
        col += klen;

        attron(COLOR_PAIR(CP_FKEY_LABEL));
        mvprintw(y, col, "%s", items[i].label);
        attroff(COLOR_PAIR(CP_FKEY_LABEL));
        col += llen;

        if (col < COLS) {
            mvaddch(y, col++, ' ');
        }
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
 * Visar K-index (K/Kp), MUF, LUF, CME på rad 1 samt Solfackla, Solvind, Norrsken på rad 2.
 * Tiden då rymdvädret senast hämtades visas högerställt i rutans titelrad.
 * @param top_y Översta raden för rutan.
 * @param width Rutans bredd (COLS).
 */
void draw_space_weather_box(int top_y, int width) {
    draw_box_with_title(top_y, 0, 4, width, "Rymdväder (hf.avitec.se)", 0);

    SpaceWeatherData sw;
    space_weather_get_data(&sw);

    /* Tidsstämpel för senaste uppdatering högerställt i rutans titelrad */
    if (sw.updated_time[0] != '\0') {
        char time_str[64];
        snprintf(time_str, sizeof(time_str), " (kl %s) ", sw.updated_time);
        int tlen = utf8_display_width(time_str);
        int tx = (width - 1) - tlen;
        if (tx > 30) {
            attron(COLOR_PAIR(CP_TITLE));
            mvprintw(top_y, tx, "%s", time_str);
            attroff(COLOR_PAIR(CP_TITLE));
        }
    }

    /* Beräkna kolumnpositioner för kolon */
    int colon0 = 12;
    int colon1 = (width >= 90) ? (width * 31 / 100) : 32;
    int colon2 = (width >= 90) ? (width * 54 / 100) : 52;
    int colon3 = (width >= 90) ? (width * 77 / 100) : 68;

    int r1 = top_y + 1;
    int r2 = top_y + 2;

    /* RAD 1: K-index ("K/Kp"), MUF, LUF, CME */
    print_sw_field(r1, colon0, "K/Kp", sw.k_kiruna, sw_tone_to_color_pair(sw.k_tone));
    print_sw_field(r1, colon1, "MUF", sw.muf, sw_tone_to_color_pair(sw.muf_tone));
    print_sw_field(r1, colon2, "LUF", sw.luf, sw_tone_to_color_pair(sw.luf_tone));
    print_sw_field(r1, colon3, "CME", sw.cme, sw_tone_to_color_pair(sw.cme_tone));

    /* RAD 2: Solfackla, Solvind, Norrsken, DX/NVIS */
    print_sw_field(r2, colon0, "Solfackla", sw.solfackla, sw_tone_to_color_pair(sw.solfackla_tone));
    print_sw_field(r2, colon1, "Solvind", sw.solvind, sw_tone_to_color_pair(sw.solvind_tone));
    print_sw_field(r2, colon2, "Norrsken", sw.norrsken, sw_tone_to_color_pair(sw.norrsken_tone));
    print_sw_field(r2, colon3, "DX/NVIS", sw.dx_nvis, sw_tone_to_color_pair(sw.dx_nvis_tone));
}

/**
 * @brief Beräknar kolumnplaceringar och bredder för loggtabellen baserat på terminalens bredd.
 * QRB döljs före QTH vid minskande bredd, QTH kvarstår ned till 80 kolumner,
 * och QSO-nummerkolumnen tas bort vid < 90 kolumner.
 * @param total_cols Fönsterbredd (COLS).
 * @param layout Utdatapekare till LogColumnLayout-struktur.
 */
void calculate_log_column_layout(int total_cols, LogColumnLayout *layout) {
    if (!layout) return;
    memset(layout, 0, sizeof(*layout));

    layout->show_qso_num = (total_cols >= 90) ? 1 : 0;

    int max_col = total_cols - 5;

    const int base_name = 14;
    const int base_qth  = 13;
    const int min_qth   = 10;
    const int max_qth   = 30;
    const int base_qrb  = 6;  /* Siffra utan 'km' */
    const int base_quj  = 5;  /* Bäring i grader (t.ex. ' 145°') */
    const int base_loc  = 8;
    const int base_dxcc = 16;

    if (layout->show_qso_num) {
        layout->col_date = 7;
        layout->col_time = 14;
        layout->col_call = 19;
        layout->col_freq = 30;
        layout->col_band = 39;
        layout->col_mode = 46;
        layout->col_rst  = 51;
        layout->col_name = 62;
    } else {
        layout->col_date = 2;
        layout->col_time = 9;
        layout->col_call = 14;
        layout->col_freq = 25;
        layout->col_band = 34;
        layout->col_mode = 41;
        layout->col_rst  = 46;
        layout->col_name = 57;
    }

    int col_name = layout->col_name;
    int avail_total = max_col - col_name + 1;

    if (max_col >= col_name + 66) {
        /* Full bredd: Alla kolumner visas (Namn, QTH, QRB, QUJ, Lokator, DXCC) */
        layout->w_dxcc = base_dxcc;
        layout->w_loc = base_loc;
        layout->w_quj = base_quj;
        layout->w_qrb = base_qrb;
        layout->w_name = base_name;
        int extra = max_col - (col_name + 66);
        int qth_extra = (extra > (max_qth - base_qth)) ? (max_qth - base_qth) : extra;
        layout->w_qth = base_qth + qth_extra;
        int rem = extra - qth_extra;
        if (rem > 0) {
            int name_extra = (rem > 6) ? 6 : rem;
            layout->w_name += name_extra;
        }
        layout->col_qth  = col_name + layout->w_name + 1;
        layout->col_qrb  = layout->col_qth + layout->w_qth + 1;
        layout->col_quj  = layout->col_qrb + layout->w_qrb + 1;
        layout->col_loc  = layout->col_quj + layout->w_quj + 1;
        layout->col_dxcc = layout->col_loc + layout->w_loc + 1;
    } else if (max_col >= col_name + 49) {
        /* DXCC döljs först: Namn, QTH, QRB, QUJ, Lokator visas */
        layout->w_dxcc = 0;
        layout->w_loc = base_loc;
        layout->w_quj = base_quj;
        layout->w_qrb = base_qrb;
        layout->w_name = base_name;
        int extra = max_col - (col_name + 49);
        layout->w_qth = base_qth + extra;
        if (layout->w_qth > max_qth) layout->w_qth = max_qth;
        layout->col_qth = col_name + layout->w_name + 1;
        layout->col_qrb = layout->col_qth + layout->w_qth + 1;
        layout->col_quj = layout->col_qrb + layout->w_qrb + 1;
        layout->col_loc = layout->col_quj + layout->w_quj + 1;
    } else if (max_col >= col_name + 41) {
        /* Lokator döljs därefter: Namn, QTH (>= 13), QRB, QUJ visas */
        layout->w_dxcc = 0;
        layout->w_loc = 0;
        layout->w_quj = base_quj;
        layout->w_qrb = base_qrb;
        layout->w_name = base_name;
        int extra = max_col - (col_name + 41);
        layout->w_qth = base_qth + extra;
        if (layout->w_qth > max_qth) layout->w_qth = max_qth;
        layout->col_qth = col_name + layout->w_name + 1;
        layout->col_qrb = layout->col_qth + layout->w_qth + 1;
        layout->col_quj = layout->col_qrb + layout->w_qrb + 1;
    } else if (max_col >= col_name + 38) {
        /* Lokator döljs, QTH minskas mot 10 tecken, QRB och QUJ visas fortfarande */
        layout->w_dxcc = 0;
        layout->w_loc = 0;
        layout->w_quj = base_quj;
        layout->w_qrb = base_qrb;
        layout->w_name = base_name;
        layout->w_qth = max_col - (col_name + base_name + 1 + base_qrb + 1 + base_quj + 1);
        if (layout->w_qth < min_qth) layout->w_qth = min_qth;
        if (layout->w_qth > max_qth) layout->w_qth = max_qth;
        layout->col_qth = col_name + layout->w_name + 1;
        layout->col_qrb = layout->col_qth + layout->w_qth + 1;
        layout->col_quj = layout->col_qrb + layout->w_qrb + 1;
    } else if (total_cols >= 80) {
        /* QRB och QUJ döljs samtidigt FÖRE QTH. QTH förblir synlig så länge total_cols >= 80 */
        layout->w_dxcc = 0;
        layout->w_loc = 0;
        layout->w_quj = 0;
        layout->w_qrb = 0;
        if (avail_total >= base_name + 1 + min_qth) {
            layout->w_name = base_name;
            int qth_space = avail_total - (base_name + 1);
            if (qth_space > max_qth) qth_space = max_qth;
            if (qth_space < min_qth) qth_space = min_qth;
            layout->w_qth = qth_space;
            layout->col_qth = col_name + layout->w_name + 1;
        } else {
            layout->w_qth = min_qth;
            layout->w_name = avail_total - (min_qth + 1);
            if (layout->w_name < 3) layout->w_name = 3;
            layout->col_qth = col_name + layout->w_name + 1;
        }
    } else if (max_col >= col_name + 3) {
        /* Vid < 80 kolumner döljs QTH helt; endast Namn visas */
        layout->w_dxcc = 0;
        layout->w_loc = 0;
        layout->w_quj = 0;
        layout->w_qrb = 0;
        layout->w_qth = 0;
        layout->w_name = max_col - col_name + 1;
        if (layout->w_name > base_name) layout->w_name = base_name;
    } else {
        /* Mycket smalt fönster */
        layout->w_dxcc = 0;
        layout->w_loc = 0;
        layout->w_quj = 0;
        layout->w_qrb = 0;
        layout->w_qth = 0;
        layout->w_name = 0;
    }
}

/**
 * @brief Ritar hela programmets huvudfönster inklusive inmatningssektion, loggtabell,
 * rymdvädersruta (vid LINES >= 25), statusrad och F-tangentrad.
 */
void draw_main_screen(const StationSettings *st, const QSO *qso,
                      FocusMode focus_mode, int active_field, int band_selecting,
                      int field_cursor_pos, int time_frozen, int editing_index,
                      QSOList *log_list, int selected_log_pos, int *log_scroll_offset,
                      const char *search_filter, const char *status_msg,
                      const char *qrb_str, const char *quj_str,
                      int *cur_y, int *cur_x) {
    erase();

    if (COLS < MIN_COLS || LINES < MIN_LINES) {
        mvprintw(LINES / 2, 2, "Fönstret är för litet (%dx%d). Minsta storlek är %dx%d.",
                 COLS, LINES, MIN_COLS, MIN_LINES);
        refresh();
        return;
    }

    draw_top_bar(st);

    /* RITA STATISTIKRUTAN OVANFÖR INMATNINGSRUTAN (endast om LINES >= 30 och aktiverad i inställningar) */
    int stat_h = (st != NULL && st->show_stats && LINES >= 30) ? 4 : 0;
    if (stat_h > 0) {
        draw_stats_box(1, COLS, log_list);
    }

    /* RITA INMATNINGSSEKTIONEN */
    int input_top = 1 + stat_h;
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
    } else if (time_frozen > 0) {
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
            int is_err = (strstr(status_msg, "Fel:") != NULL ||
                          strstr(status_msg, "VARNING") != NULL ||
                          strstr(status_msg, "Blockerat") != NULL ||
                          strstr(status_msg, "blockerat") != NULL);
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

    int form_w_qth = (COLS >= 88) ? 14 : (COLS - (c4 + 2) - 1);
    if (form_w_qth < 6) form_w_qth = 6;
    if (form_w_qth > 16) form_w_qth = 16;

    int form_w_dxcc = (COLS >= 95) ? 14 : (COLS >= 85 ? 12 : 10);

    /* Rad 1 (y = 6): Signal, RST T, RST R, Namn */
    draw_field_aligned(input_top + 1, c1, "Signal", qso->call,     10,          (focus_mode == FOCUS_INPUT && active_field == FLD_CALL), field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(input_top + 1, c2, "RST T",  qso->rst_sent,  6,          (focus_mode == FOCUS_INPUT && active_field == FLD_RSTS), field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(input_top + 1, c3, "RST R",  qso->rst_rcvd,  6,          (focus_mode == FOCUS_INPUT && active_field == FLD_RSTR), field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(input_top + 1, c4, "Namn",   qso->name,     form_w_name, (focus_mode == FOCUS_INPUT && active_field == FLD_NAME), field_cursor_pos, cur_y, cur_x);

    /* Rad 2 (y = 7): DXCC, Lokator, QRB, QTH */
    draw_field_aligned(input_top + 2, c1, "DXCC",    qso->dxcc,       form_w_dxcc, (focus_mode == FOCUS_INPUT && active_field == FLD_DXCC), field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(input_top + 2, c2, "Lokator", qso->gridsquare, 6,           (focus_mode == FOCUS_INPUT && active_field == FLD_GRID), field_cursor_pos, cur_y, cur_x);

    /* QRB mellan Lokator och QTH på rad 2 */
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

    draw_field_aligned(input_top + 2, c4, "QTH",     qso->qth,        form_w_qth,  (focus_mode == FOCUS_INPUT && active_field == FLD_QTH),  field_cursor_pos, cur_y, cur_x);

    /* Rad 3 (y = 8): Frekvens, Band, Mode, QUJ */
    draw_field_aligned(input_top + 3, c1, "Frekvens", qso->freq, 10, (focus_mode == FOCUS_INPUT && active_field == FLD_FREQ), field_cursor_pos, cur_y, cur_x);
    draw_band_selector_aligned(input_top + 3, c2, qso->band, (focus_mode == FOCUS_INPUT && active_field == FLD_BAND), band_selecting, cur_y, cur_x);
    draw_field_aligned(input_top + 3, c3, "Mode",     qso->mode,  6, (focus_mode == FOCUS_INPUT && active_field == FLD_MODE), field_cursor_pos, cur_y, cur_x);

    /* QUJ till höger om Mode på rad 3 */
    int has_quj = (quj_str != NULL && quj_str[0] != '\0' && strcmp(quj_str, "-") != 0);
    const char *quj_val = has_quj ? quj_str : "-";
    int lbl_quj_x = c4 - 3; /* "QUJ" är 3 tecken */
    attron(COLOR_PAIR(CP_LABEL));
    mvprintw(input_top + 3, lbl_quj_x, "QUJ:");
    attroff(COLOR_PAIR(CP_LABEL));
    int quj_color = has_quj ? CP_STATUS_OK : CP_FIELD_INACTIVE;
    attron(COLOR_PAIR(quj_color) | (has_quj ? A_BOLD : 0));
    mvprintw(input_top + 3, c4 + 2, "%-8.8s", quj_val);
    attroff(COLOR_PAIR(quj_color) | (has_quj ? A_BOLD : 0));

    /* Rad 4 (y = 9): Datum, UTC, Notering */
    int comm_w = COLS - (c3 + 2) - 1;
    if (comm_w < 5) comm_w = 5;
    const char *lbl_not = (COLS >= 88) ? "Notering" : "Not";
    draw_field_aligned(input_top + 4, c1, "Datum",    qso->date,     10,     (focus_mode == FOCUS_INPUT && active_field == FLD_DATE), field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(input_top + 4, c2, "UTC",      qso->time_utc,  6,     (focus_mode == FOCUS_INPUT && active_field == FLD_TIME), field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(input_top + 4, c3, lbl_not,    qso->comment,  comm_w, (focus_mode == FOCUS_INPUT && active_field == FLD_COMM), field_cursor_pos, cur_y, cur_x);

    /* RITA LOGGRADSKOMPONENTEN */
    int show_notes = (st == NULL || st->show_notes);
    int show_sw = (st == NULL || st->show_space_weather);
    int sw_reserved = (show_sw && LINES >= 25) ? 4 : 0;
    int log_top = input_top + input_h;
    int log_h = LINES - log_top - 1 - sw_reserved;
    if (log_h < 3) log_h = 3;

    char log_title[64];
    if (search_filter != NULL && search_filter[0] != '\0') {
        snprintf(log_title, sizeof(log_title), "Loggrader [Filter: %s]", search_filter);
    } else {
        snprintf(log_title, sizeof(log_title), "Tidigare QSO (Loggrader)");
    }
    draw_box_with_title(log_top, 0, log_h, COLS, log_title, (focus_mode == FOCUS_LOG));

    /* Kolumnrubriker och dynamisk layout */
    int col_ny = COLS - 4;

    LogColumnLayout layout;
    calculate_log_column_layout(COLS, &layout);

    attron(COLOR_PAIR(CP_TABLE_HEADER) | A_BOLD);
    mvhline(log_top + 1, 1, ' ', COLS - 2);
    if (layout.show_qso_num) {
        mvprintw(log_top + 1, 2, "   #");
    }
    mvprintw(log_top + 1, layout.col_date, "YYMMDD");
    mvprintw(log_top + 1, layout.col_time, "HHMM");
    mvprintw(log_top + 1, layout.col_call, "Signal    ");
    mvprintw(log_top + 1, layout.col_freq, "%-8s", "Frekv");
    mvprintw(log_top + 1, layout.col_band, "%6s", "Band");
    mvprintw(log_top + 1, layout.col_mode, "Mode");
    mvprintw(log_top + 1, layout.col_rst,  "%-10s", "RST");

    char h_buf[64];
    if (layout.w_name > 0) {
        utf8_pad_or_clip(h_buf, sizeof(h_buf), "Namn", layout.w_name, 0);
        mvprintw(log_top + 1, layout.col_name, "%s", h_buf);
    }
    if (layout.w_qth > 0) {
        utf8_pad_or_clip(h_buf, sizeof(h_buf), "QTH", layout.w_qth, 0);
        mvprintw(log_top + 1, layout.col_qth, "%s", h_buf);
    }
    if (layout.w_qrb > 0) {
        utf8_pad_or_clip(h_buf, sizeof(h_buf), "QRB", layout.w_qrb, 1);
        mvprintw(log_top + 1, layout.col_qrb, "%s", h_buf);
    }
    if (layout.w_quj > 0) {
        utf8_pad_or_clip(h_buf, sizeof(h_buf), "QUJ", layout.w_quj, 1);
        mvprintw(log_top + 1, layout.col_quj, "%s", h_buf);
    }
    if (layout.w_loc > 0) {
        utf8_pad_or_clip(h_buf, sizeof(h_buf), "Lokator", layout.w_loc, 0);
        mvprintw(log_top + 1, layout.col_loc, "%s", h_buf);
    }
    if (layout.w_dxcc > 0) {
        utf8_pad_or_clip(h_buf, sizeof(h_buf), "DXCC", layout.w_dxcc, 0);
        mvprintw(log_top + 1, layout.col_dxcc, "%s", h_buf);
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
                if (show_notes && log_list->items[orig].comment[0] != '\0') {
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
        if (layout.show_qso_num) {
            mvprintw(row_y, 2, "%c%3d", mark_char, actual_idx + 1);
        } else {
            mvprintw(row_y, 1, "%c", mark_char);
        }

        char d_str[16];
        format_date_yymmdd(item->date, d_str, sizeof(d_str));
        mvprintw(row_y, layout.col_date, "%-6s", d_str);

        char t_str[16];
        format_time_hhmm(item->time_utc, t_str, sizeof(t_str));
        mvprintw(row_y, layout.col_time, "%-4s", t_str);

        mvprintw(row_y, layout.col_call, "%-10s", item->call);
        mvprintw(row_y, layout.col_freq, "%-8s", item->freq);

        char band_dsp[16];
        format_band_display(item->band, band_dsp, sizeof(band_dsp));
        mvprintw(row_y, layout.col_band, "%6s", band_dsp);

        mvprintw(row_y, layout.col_mode, "%-4s", item->mode);

        char rst_str[32];
        format_rst_combined(item->rst_sent, item->rst_rcvd, rst_str, sizeof(rst_str));
        mvprintw(row_y, layout.col_rst, "%-10.10s", rst_str);

        char cell_buf[128];
        if (layout.w_name > 0) {
            utf8_pad_or_clip(cell_buf, sizeof(cell_buf), item->name, layout.w_name, 0);
            mvprintw(row_y, layout.col_name, "%s", cell_buf);
        }
        if (layout.w_qth > 0) {
            utf8_pad_or_clip(cell_buf, sizeof(cell_buf), item->qth, layout.w_qth, 0);
            mvprintw(row_y, layout.col_qth, "%s", cell_buf);
        }
        if (layout.w_qrb > 0) {
            if (item->distance[0] == '\0' || strcmp(item->distance, "-") == 0) {
                update_qso_qrb(st, (QSO *)item);
            }
            char qrb_dsp[16];
            format_qrb_display(item, qrb_dsp, sizeof(qrb_dsp));
            utf8_pad_or_clip(cell_buf, sizeof(cell_buf), qrb_dsp, layout.w_qrb, 1);
            mvprintw(row_y, layout.col_qrb, "%s", cell_buf);
        }
        if (layout.w_quj > 0) {
            char quj_dsp[16];
            format_quj_display(st, item, quj_dsp, sizeof(quj_dsp));
            utf8_pad_or_clip(cell_buf, sizeof(cell_buf), quj_dsp, layout.w_quj, 1);
            mvprintw(row_y, layout.col_quj, "%s", cell_buf);
        }
        if (layout.w_loc > 0) {
            utf8_pad_or_clip(cell_buf, sizeof(cell_buf), item->gridsquare, layout.w_loc, 0);
            mvprintw(row_y, layout.col_loc, "%s", cell_buf);
        }
        if (layout.w_dxcc > 0) {
            utf8_pad_or_clip(cell_buf, sizeof(cell_buf), item->dxcc, layout.w_dxcc, 0);
            mvprintw(row_y, layout.col_dxcc, "%s", cell_buf);
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
            *cur_x = layout.show_qso_num ? 2 : 1;
        }

        cur_screen_row++;

        /* Om det finns en Notering och visning av noteringar är på och det finns plats kvar: visa den på raden under */
        if (show_notes && item->comment[0] != '\0' && cur_screen_row < log_avail_rows) {
            int note_y = log_top + 2 + cur_screen_row;
            attron(COLOR_PAIR(row_color) | row_attr);
            mvhline(note_y, 1, ' ', COLS - 2);

            int note_col = layout.show_qso_num ? 7 : layout.col_date;
            int max_note_w = COLS - (note_col + 12);
            if (max_note_w < 5) max_note_w = 5;

            char note_buf[512];
            utf8_pad_or_clip(note_buf, sizeof(note_buf), item->comment, max_note_w, 0);
            size_t nlen = strlen(note_buf);
            while (nlen > 0 && note_buf[nlen - 1] == ' ') {
                note_buf[--nlen] = '\0';
            }

            if (is_selected || is_marked) {
                mvprintw(note_y, note_col, "Notering: %s", note_buf);
                attroff(COLOR_PAIR(row_color) | row_attr);
            } else {
                attroff(COLOR_PAIR(row_color) | row_attr);
                attron(COLOR_PAIR(CP_LABEL));
                mvprintw(note_y, note_col, "Notering: ");
                attroff(COLOR_PAIR(CP_LABEL));
                attron(COLOR_PAIR(CP_LOG_ROW));
                printw("%s", note_buf);
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

    /* RITA RYMDVÄDERSKOMPONENTEN (endast om show_sw och LINES >= 25) */
    if (show_sw && LINES >= 25) {
        int sw_top = log_top + log_h;
        draw_space_weather_box(sw_top, COLS);
    }

    /* RITA F-TANGENTRAD ALLRA LÄNGST NED */
    draw_bottom_fkey_bar(0, focus_mode, search_filter);

    curs_set(band_selecting ? 0 : 1);
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
    curs_set(1);

    while (1) {
        int y = LINES - 2;
        attron(COLOR_PAIR(CP_TOPBAR) | A_BOLD);
        mvhline(y, 0, ' ', COLS);
        mvprintw(y, 2, "Sök i loggen (Esc/F3 avbryter, Enter bekräftar): ");
        int prompt_len = 50;

        attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        int field_w = COLS - prompt_len - 4;
        if (field_w > 30) field_w = 30;
        if (field_w < 5) field_w = 5;
        for (int i = 0; i < field_w; ++i) mvaddch(y, prompt_len + i, ' ');
        int sc = 0;
        if (cursor_pos >= field_w) sc = cursor_pos - field_w + 1;
        size_t sb = utf8_char_to_byte(buf, sc);
        size_t eb = utf8_char_to_byte(buf, sc + field_w);
        mvprintw(y, prompt_len, "%.*s", (int)(eb - sb), buf + sb);
        attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        attroff(COLOR_PAIR(CP_TOPBAR) | A_BOLD);

        move(y, prompt_len + (cursor_pos - sc));
        refresh();

        wint_t wch = 0;
        int input_res = get_wch(&wch);
        int ch = (input_res == ERR) ? ERR : (int)wch;
        if (ch == ERR) continue;

        if (ch == 27 || ch == KEY_F(3)) {
            return 0;
        } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            trim_str(buf);
            copy_str(search_filter, filter_sz, buf);
            return 1;
        } else if (ch == KEY_LEFT) {
            if (cursor_pos > 0) cursor_pos--;
        } else if (ch == KEY_RIGHT) {
            int total_c = utf8_display_width(buf);
            if (cursor_pos < total_c) cursor_pos++;
        } else if (ch == KEY_HOME) {
            cursor_pos = 0;
        } else if (ch == KEY_END) {
            cursor_pos = utf8_display_width(buf);
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            utf8_backspace_at(buf, &cursor_pos);
        } else if (ch == KEY_DC) {
            buf[0] = '\0';
            cursor_pos = 0;
        } else if (input_res == OK && wch >= 32 && wch != 127) {
            char u8[8] = {0};
            mbstate_t ps;
            memset(&ps, 0, sizeof(ps));
            size_t u8_len = wcrtomb(u8, (wchar_t)wch, &ps);
            if (u8_len == (size_t)-1 || u8_len == 0) {
                u8[0] = (char)wch;
                u8[1] = '\0';
                u8_len = 1;
            } else {
                u8[u8_len] = '\0';
            }
            utf8_insert_char_at(buf, sizeof(buf), &cursor_pos, u8, u8_len);
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
                      QSOList *log_list, int selected_log_pos, int *log_scroll_offset,
                      const char *search_filter, char *status_msg, size_t status_sz,
                      const char *qrb_str, const char *quj_str, int band_row, int colon_col) {
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
                         search_filter, status_msg, qrb_str, quj_str,
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
            if (*time_frozen == 0) {
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
            if (*time_frozen == 0) {
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
            if (*time_frozen == 0) {
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
        case FLD_CALL: case FLD_DXCC: case FLD_FREQ: case FLD_DATE: return 0;
        case FLD_RSTS: case FLD_GRID: case FLD_BAND: case FLD_TIME: return 1;
        case FLD_RSTR:                case FLD_MODE:                return 2;
        case FLD_NAME: case FLD_QTH:                                return 3;
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
        case FLD_DXCC: case FLD_GRID: case FLD_QTH:                 return 1;
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
        { FLD_DXCC, FLD_GRID, FLD_QTH,  FLD_QTH  },
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
                    char *current_qrb, char *current_quj) {
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
    if (current_quj) copy_str(current_quj, 32, "-");

    get_utc_strings(qso->date, sizeof(qso->date),
                    qso->time_utc, sizeof(qso->time_utc),
                    NULL, 0);
}
