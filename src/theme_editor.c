/**
 * @file theme_editor.c
 * @brief Interaktiv editor för användaranpassade färgprofiler med realtidsförhandsgranskning.
 */

#include "theme_editor.h"
#include "tui.h"
#include "util.h"
#include <ncurses.h>
#include <stdio.h>
#include <string.h>

typedef struct {
    const char *name; /**< Namn på färgen på svenska */
    short color_256;  /**< Färgkod i 256-färgers terminal */
    short color_8;    /**< Färgkod i 8-färgers terminal (fallback) */
} ColorOption;

static const ColorOption PALETTE[CUSTOM_PALETTE_SIZE] = {
    { "Svart",              COLOR_BLACK, COLOR_BLACK },
    { "Kritvit",            255,         COLOR_WHITE },
    { "Ljusgrå",            250,         COLOR_WHITE },
    { "Mörkgrå",            236,         COLOR_BLACK },
    { "Marinblå (Mörk)",    17,          COLOR_BLUE },
    { "Havsblå",            24,          COLOR_BLUE },
    { "Klarblå",            33,          COLOR_BLUE },
    { "Ljusblå (Frost)",    117,         COLOR_CYAN },
    { "Cyan (Turkos)",      37,          COLOR_CYAN },
    { "Elcyan (Neon)",      87,          COLOR_CYAN },
    { "Fosforgrön",         84,          COLOR_GREEN },
    { "Skogsgrön",          29,          COLOR_GREEN },
    { "Fältgrön (Oliv)",    107,         COLOR_GREEN },
    { "Bärnsten / Guld",    178,         COLOR_YELLOW },
    { "Solgul (Klar)",      221,         COLOR_YELLOW },
    { "Orange (Varm)",      208,         COLOR_YELLOW },
    { "Tegelröd",           167,         COLOR_RED },
    { "Klar röd",           196,         COLOR_RED },
    { "Korallrosa",         212,         COLOR_MAGENTA },
    { "Neonlila / Violett", 141,         COLOR_MAGENTA },
    { "Plommon / Mauve",    134,         COLOR_MAGENTA },
    { "Vintage Brun",       235,         COLOR_BLACK }
};

static const char * const SLOT_NAMES[14] = {
    "Huvudbakgrund",
    "Normal text",
    "Normala ramar",
    "Aktiv ram (Fokus)",
    "Titlar & rubriker",
    "Fältetiketter",
    "Aktivt fält (Bakgrund)",
    "Aktivt fält (Text)",
    "Vald loggrad (Bakgrund)",
    "Vald loggrad (Text)",
    "Titelrad (Bakgrund)",
    "Titelrad (Text)",
    "Status OK / bock",
    "Varning / Felmarkör"
};

static short get_palette_color(int idx) {
    if (idx < 0) idx = 0;
    if (idx >= CUSTOM_PALETTE_SIZE) idx = CUSTOM_PALETTE_SIZE - 1;
    return (COLORS >= 256) ? PALETTE[idx].color_256 : PALETTE[idx].color_8;
}

const char *custom_theme_get_color_name(int color_idx) {
    if (color_idx < 0 || color_idx >= CUSTOM_PALETTE_SIZE) return "Okänd";
    return PALETTE[color_idx].name;
}

static int get_slot_val(const CustomTheme *ct, int slot) {
    switch (slot) {
        case 0: return ct->bg_main;
        case 1: return ct->fg_text;
        case 2: return ct->border;
        case 3: return ct->border_active;
        case 4: return ct->title;
        case 5: return ct->label;
        case 6: return ct->field_act_bg;
        case 7: return ct->field_act_fg;
        case 8: return ct->sel_bg;
        case 9: return ct->sel_fg;
        case 10: return ct->topbar_bg;
        case 11: return ct->topbar_fg;
        case 12: return ct->status_ok;
        case 13: return ct->status_warn;
        default: return 0;
    }
}

static void set_slot_val(CustomTheme *ct, int slot, int val) {
    if (val < 0) val = 0;
    if (val >= CUSTOM_PALETTE_SIZE) val = CUSTOM_PALETTE_SIZE - 1;
    switch (slot) {
        case 0: ct->bg_main = val; break;
        case 1: ct->fg_text = val; break;
        case 2: ct->border = val; break;
        case 3: ct->border_active = val; break;
        case 4: ct->title = val; break;
        case 5: ct->label = val; break;
        case 6: ct->field_act_bg = val; break;
        case 7: ct->field_act_fg = val; break;
        case 8: ct->sel_bg = val; break;
        case 9: ct->sel_fg = val; break;
        case 10: ct->topbar_bg = val; break;
        case 11: ct->topbar_fg = val; break;
        case 12: ct->status_ok = val; break;
        case 13: ct->status_warn = val; break;
        default: break;
    }
}

void custom_theme_init_defaults(CustomTheme *ct) {
    if (ct == NULL) return;
    ct->bg_main       = 3;  /* Mörkgrå (236) */
    ct->fg_text       = 1;  /* Kritvit */
    ct->border        = 8;  /* Cyan */
    ct->border_active = 9;  /* Elcyan */
    ct->title         = 9;  /* Elcyan */
    ct->label         = 7;  /* Ljusblå */
    ct->field_act_bg  = 0;  /* Svart */
    ct->field_act_fg  = 9;  /* Elcyan */
    ct->sel_bg        = 5;  /* Havsblå (24) */
    ct->sel_fg        = 1;  /* Kritvit */
    ct->topbar_bg     = 8;  /* Cyan */
    ct->topbar_fg     = 0;  /* Svart */
    ct->status_ok     = 10; /* Fosforgrön */
    ct->status_warn   = 17; /* Klar röd */
}

void custom_theme_init_preset(CustomTheme *ct, int preset) {
    if (ct == NULL) return;
    switch (preset) {
        case 0: /* Standard Mörk / Cyan */
            custom_theme_init_defaults(ct);
            break;
        case 1: /* Klassisk Blå (Borland) */
            ct->bg_main = 4; ct->fg_text = 14; ct->border = 8; ct->border_active = 1;
            ct->title = 1; ct->label = 8; ct->field_act_bg = 8; ct->field_act_fg = 0;
            ct->sel_bg = 8; ct->sel_fg = 0; ct->topbar_bg = 2; ct->topbar_fg = 0;
            ct->status_ok = 10; ct->status_warn = 17;
            break;
        case 2: /* Marinblå (Solarized) */
            ct->bg_main = 4; ct->fg_text = 2; ct->border = 3; ct->border_active = 8;
            ct->title = 8; ct->label = 6; ct->field_act_bg = 8; ct->field_act_fg = 0;
            ct->sel_bg = 5; ct->sel_fg = 1; ct->topbar_bg = 8; ct->topbar_fg = 0;
            ct->status_ok = 10; ct->status_warn = 16;
            break;
        case 3: /* Neon Skymning (Dracula) */
            ct->bg_main = 3; ct->fg_text = 1; ct->border = 7; ct->border_active = 19;
            ct->title = 9; ct->label = 18; ct->field_act_bg = 0; ct->field_act_fg = 9;
            ct->sel_bg = 19; ct->sel_fg = 0; ct->topbar_bg = 19; ct->topbar_fg = 0;
            ct->status_ok = 10; ct->status_warn = 17;
            break;
        case 4: /* Fältgrön (Militär) */
            ct->bg_main = 0; ct->fg_text = 2; ct->border = 11; ct->border_active = 13;
            ct->title = 13; ct->label = 12; ct->field_act_bg = 13; ct->field_act_fg = 0;
            ct->sel_bg = 12; ct->sel_fg = 1; ct->topbar_bg = 13; ct->topbar_fg = 0;
            ct->status_ok = 12; ct->status_warn = 16;
            break;
        case 5: /* Fosforgrön (Matrix) */
            ct->bg_main = 0; ct->fg_text = 10; ct->border = 10; ct->border_active = 1;
            ct->title = 10; ct->label = 10; ct->field_act_bg = 10; ct->field_act_fg = 0;
            ct->sel_bg = 10; ct->sel_fg = 0; ct->topbar_bg = 10; ct->topbar_fg = 0;
            ct->status_ok = 10; ct->status_warn = 17;
            break;
        case 6: /* Bärnsten CRT (Ambra) */
            ct->bg_main = 0; ct->fg_text = 13; ct->border = 13; ct->border_active = 14;
            ct->title = 14; ct->label = 13; ct->field_act_bg = 13; ct->field_act_fg = 0;
            ct->sel_bg = 13; ct->sel_fg = 0; ct->topbar_bg = 13; ct->topbar_fg = 0;
            ct->status_ok = 13; ct->status_warn = 14;
            break;
        case 7: /* Mörkerläge (Crimson Night) */
            ct->bg_main = 0; ct->fg_text = 16; ct->border = 16; ct->border_active = 17;
            ct->title = 17; ct->label = 16; ct->field_act_bg = 17; ct->field_act_fg = 0;
            ct->sel_bg = 16; ct->sel_fg = 0; ct->topbar_bg = 16; ct->topbar_fg = 0;
            ct->status_ok = 16; ct->status_warn = 17;
            break;
        default:
            custom_theme_init_defaults(ct);
            break;
    }
}

void apply_custom_theme_pairs(const CustomTheme *ct) {
    if (!has_colors() || ct == NULL) return;
    start_color();

    short c_bg_main       = get_palette_color(ct->bg_main);
    short c_fg_text       = get_palette_color(ct->fg_text);
    short c_border        = get_palette_color(ct->border);
    short c_border_active = get_palette_color(ct->border_active);
    short c_title         = get_palette_color(ct->title);
    short c_label         = get_palette_color(ct->label);
    short c_field_act_bg  = get_palette_color(ct->field_act_bg);
    short c_field_act_fg  = get_palette_color(ct->field_act_fg);
    short c_sel_bg        = get_palette_color(ct->sel_bg);
    short c_sel_fg        = get_palette_color(ct->sel_fg);
    short c_topbar_bg     = get_palette_color(ct->topbar_bg);
    short c_topbar_fg     = get_palette_color(ct->topbar_fg);
    short c_status_ok     = get_palette_color(ct->status_ok);
    short c_status_warn   = get_palette_color(ct->status_warn);

    short accent_yel = get_palette_color(14);
    short accent_mag = get_palette_color(19);

    init_pair(CP_MAIN_BG,          c_fg_text,       c_bg_main);
    init_pair(CP_TOPBAR,           c_topbar_fg,     c_topbar_bg);
    init_pair(CP_TOPBAR_ACCENT,    c_status_warn,   c_topbar_bg);
    init_pair(CP_BORDER,           c_border,        c_bg_main);
    init_pair(CP_BORDER_ACTIVE,    c_border_active, c_bg_main);
    init_pair(CP_TITLE,            c_title,         c_bg_main);
    init_pair(CP_LABEL,            c_label,         c_bg_main);
    init_pair(CP_FIELD_INACTIVE,   c_fg_text,       c_bg_main);
    init_pair(CP_FIELD_ACTIVE,     c_field_act_fg,  c_field_act_bg);
    init_pair(CP_FROZEN_BADGE,     c_bg_main,       accent_yel);
    init_pair(CP_LIVE_BADGE,       c_bg_main,       c_status_ok);
    init_pair(CP_EDIT_BADGE,       c_fg_text,       accent_mag);
    init_pair(CP_TABLE_HEADER,     c_title,         c_bg_main);
    init_pair(CP_LOG_ROW,          c_fg_text,       c_bg_main);
    init_pair(CP_LOG_SELECTED,     c_sel_fg,        c_sel_bg);
    init_pair(CP_LOG_MARKED,       c_fg_text,       accent_mag);
    init_pair(CP_FKEY_NUM,         c_topbar_bg,     c_border);
    init_pair(CP_FKEY_LABEL,       c_fg_text,       c_bg_main);
    init_pair(CP_STATUS_OK,        c_status_ok,     c_bg_main);
    init_pair(CP_STATUS_WARN,      c_fg_text,       c_status_warn);
    init_pair(CP_FLAG_NEW,         c_status_warn,   c_bg_main);
    init_pair(CP_FLAG_HANDLED,     c_status_ok,     c_bg_main);
    init_pair(CP_FLAG_NEW_SEL,     c_status_warn,   c_sel_bg);
    init_pair(CP_FLAG_HANDLED_SEL, c_status_ok,     c_sel_bg);
    init_pair(CP_FLAG_NEW_MRK,     c_status_warn,   accent_mag);
    init_pair(CP_FLAG_HANDLED_MRK, c_status_ok,     accent_mag);
    init_pair(CP_SW_NORMAL,        c_fg_text,       c_bg_main);
    init_pair(CP_SW_OK,            c_status_ok,     c_bg_main);
    init_pair(CP_SW_WARN,          accent_yel,      c_bg_main);
    init_pair(CP_SW_BAD,           c_status_warn,   c_bg_main);
    init_pair(CP_SW_SEVERE,        accent_mag,      c_bg_main);

    bkgd(COLOR_PAIR(CP_MAIN_BG));
}

static void draw_editor_preview(int py, int px, int pw) {
    if (pw < 32) return;

    /* Rita förhandsgranskningens titelrad */
    attron(COLOR_PAIR(CP_TOPBAR) | A_BOLD);
    mvhline(py, px, ' ', pw);
    mvprintw(py, px + 1, " SM5UEI  Stockholm  JO89VJ  12:34 UTC");
    attroff(COLOR_PAIR(CP_TOPBAR) | A_BOLD);

    /* Inmatningsbox (Aktiv) */
    draw_box_with_title(py + 1, px, 6, pw, "QSO-inmatning (Aktiv)", 1);

    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(py + 2, px + 2, "Signal:");
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);
    attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
    mvprintw(py + 2, px + 10, " SM7ABC ");
    attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);

    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(py + 2, px + 20, "RST:");
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);
    attron(COLOR_PAIR(CP_FIELD_INACTIVE));
    mvprintw(py + 2, px + 25, "59");
    attroff(COLOR_PAIR(CP_FIELD_INACTIVE));

    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(py + 4, px + 2, "Frekv:");
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);
    attron(COLOR_PAIR(CP_FIELD_INACTIVE));
    mvprintw(py + 4, px + 10, "14.225");
    attroff(COLOR_PAIR(CP_FIELD_INACTIVE));

    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(py + 4, px + 20, "Band:");
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);
    attron(COLOR_PAIR(CP_FIELD_INACTIVE));
    mvprintw(py + 4, px + 25, "20 m");
    attroff(COLOR_PAIR(CP_FIELD_INACTIVE));

    /* Loggbox */
    draw_box_with_title(py + 7, px, 6, pw, "Loggtabell", 0);

    /* Header */
    attron(COLOR_PAIR(CP_TABLE_HEADER) | A_BOLD);
    mvprintw(py + 8, px + 2, "#   Datum  Signal   Band  RST  Ny");
    attroff(COLOR_PAIR(CP_TABLE_HEADER) | A_BOLD);

    /* Rad 1 (vanlig) */
    attron(COLOR_PAIR(CP_LOG_ROW));
    mvprintw(py + 9, px + 2, "1  261114  SM5UEI   20m   59");
    attroff(COLOR_PAIR(CP_LOG_ROW));
    attron(COLOR_PAIR(CP_FLAG_HANDLED) | A_BOLD);
    mvprintw(py + 9, px + 28, "✓");
    attroff(COLOR_PAIR(CP_FLAG_HANDLED) | A_BOLD);

    /* Rad 2 (vald) */
    attron(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
    mvprintw(py + 10, px + 1, " 2  261114  DL1ABC   40m   599 ");
    attroff(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
    attron(COLOR_PAIR(CP_FLAG_NEW_SEL) | A_BOLD);
    mvprintw(py + 10, px + 28, "✗");
    attroff(COLOR_PAIR(CP_FLAG_NEW_SEL) | A_BOLD);

    /* Statusrad */
    attron(COLOR_PAIR(CP_STATUS_OK) | A_BOLD);
    mvprintw(py + 13, px + 1, "Status: Signal uppslagen [OK]");
    attroff(COLOR_PAIR(CP_STATUS_OK) | A_BOLD);
}

int run_color_theme_editor(StationSettings *edit) {
    if (edit == NULL) return 0;

    CustomTheme temp = edit->custom_theme;
    int orig_theme = edit->theme;
    int sel_slot = 0;

    apply_custom_theme_pairs(&temp);
    curs_set(0);

    while (1) {
        erase();

        draw_top_bar(edit);
        draw_box_with_title(1, 0, LINES - 2, COLS, "Egen Färgprofilseditor", 1);

        int start_y = (LINES >= 24) ? 3 : 2;
        attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvprintw(start_y, 2, "Anpassa profilfärger med pilar. Ändringar visas live i realtid!");
        attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);

        /* Vänster panel: Lista över 14 färginställningar */
        int list_y = start_y + 2;
        int list_w = 40;
        if (COLS < 80) list_w = COLS / 2;

        for (int i = 0; i < 14; ++i) {
            int r = list_y + i;
            if (r >= LINES - 3) break;
            int is_sel = (i == sel_slot);
            int color_idx = get_slot_val(&temp, i);
            const char *c_name = PALETTE[color_idx].name;

            if (is_sel) {
                attron(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
                mvprintw(r, 2, " > %-22s [ < %-13s > ] < ", SLOT_NAMES[i], c_name);
                attroff(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
            } else {
                attron(COLOR_PAIR(CP_LABEL));
                mvprintw(r, 2, "   %-22s", SLOT_NAMES[i]);
                attroff(COLOR_PAIR(CP_LABEL));
                attron(COLOR_PAIR(CP_FIELD_INACTIVE));
                mvprintw(r, 27, "[ %-17s ]", c_name);
                attroff(COLOR_PAIR(CP_FIELD_INACTIVE));
            }
        }

        /* Höger panel: Live-förhandsgranskning */
        int prev_x = list_w + 3;
        int prev_w = COLS - prev_x - 2;
        if (prev_w > 48) prev_w = 48;
        if (prev_w >= 32) {
            draw_editor_preview(list_y, prev_x, prev_w);
        }

        /* Instruktioner i nederkant */
        if (LINES >= 20) {
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(LINES - 3, 2, "Pilar Upp/Ned: Välj element.  Pilar Vänster/Höger: Byt färg direkt.");
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);
            attron(COLOR_PAIR(CP_TOPBAR) | A_BOLD);
            mvprintw(LINES - 2, 2, "[Enter/F2]: Spara & aktivera  [P]: Ladda förinställd mall  [R]: Återställ  [Esc]: Avbryt");
            attroff(COLOR_PAIR(CP_TOPBAR) | A_BOLD);
        }

        refresh();

        wint_t wch = 0;
        int input_res = get_wch(&wch);
        int ch = (input_res == ERR) ? ERR : (int)wch;
        if (ch == ERR) continue;

        if (ch == 27) { /* Esc - avbryt */
            apply_color_theme(orig_theme);
            return 0;
        } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == KEY_F(2) || ch == KEY_F(12)) {
            /* Spara profil och aktivera den */
            edit->custom_theme = temp;
            edit->theme = THEME_CUSTOM;
            set_custom_theme(&edit->custom_theme);
            apply_color_theme(THEME_CUSTOM);
            return 1;
        } else if (ch == KEY_UP || ch == 'k' || ch == 'K') {
            sel_slot = (sel_slot + 13) % 14;
        } else if (ch == KEY_DOWN || ch == 'j' || ch == 'J') {
            sel_slot = (sel_slot + 1) % 14;
        } else if (ch == KEY_LEFT || ch == 'h' || ch == 'H') {
            int cur_val = get_slot_val(&temp, sel_slot);
            int new_val = (cur_val + CUSTOM_PALETTE_SIZE - 1) % CUSTOM_PALETTE_SIZE;
            set_slot_val(&temp, sel_slot, new_val);
            apply_custom_theme_pairs(&temp);
        } else if (ch == KEY_RIGHT || ch == 'l' || ch == 'L') {
            int cur_val = get_slot_val(&temp, sel_slot);
            int new_val = (cur_val + 1) % CUSTOM_PALETTE_SIZE;
            set_slot_val(&temp, sel_slot, new_val);
            apply_custom_theme_pairs(&temp);
        } else if (ch == 'r' || ch == 'R') {
            custom_theme_init_defaults(&temp);
            apply_custom_theme_pairs(&temp);
        } else if (ch == 'p' || ch == 'P') {
            /* Popup för att ladda förinställd mall */
            const char * const PRESETS[] = {
                "Standard Mörk / Cyan",
                "Klassisk Blå (Borland)",
                "Marinblå (Solarized)",
                "Neon Skymning (Dracula)",
                "Fältgrön (Militär)",
                "Fosforgrön (Matrix)",
                "Bärnsten CRT (Ambra)",
                "Mörkerläge (Crimson Night)"
            };
            int p_sel = 0;
            int box_h = 12, box_w = 40;
            int by = (LINES - box_h) / 2;
            int bx = (COLS - box_w) / 2;

            while (1) {
                draw_box_with_title(by, bx, box_h, box_w, "Ladda förinställd mall", 1);
                for (int i = 0; i < 8; ++i) {
                    if (i == p_sel) {
                        attron(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
                        mvprintw(by + 2 + i, bx + 2, " > %-32s < ", PRESETS[i]);
                        attroff(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
                    } else {
                        attron(COLOR_PAIR(CP_FIELD_INACTIVE));
                        mvprintw(by + 2 + i, bx + 2, "   %-32s   ", PRESETS[i]);
                        attroff(COLOR_PAIR(CP_FIELD_INACTIVE));
                    }
                }
                refresh();

                int pch = getch();
                if (pch == 27) {
                    break;
                } else if (pch == '\n' || pch == '\r' || pch == KEY_ENTER || pch == ' ') {
                    custom_theme_init_preset(&temp, p_sel);
                    apply_custom_theme_pairs(&temp);
                    break;
                } else if (pch == KEY_UP || pch == 'k' || pch == 'K') {
                    p_sel = (p_sel + 7) % 8;
                } else if (pch == KEY_DOWN || pch == 'j' || pch == 'J') {
                    p_sel = (p_sel + 1) % 8;
                } else if (pch >= '1' && pch <= '8') {
                    p_sel = pch - '1';
                    custom_theme_init_preset(&temp, p_sel);
                    apply_custom_theme_pairs(&temp);
                    break;
                }
            }
        }
    }
}
