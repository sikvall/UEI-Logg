#include "tui.h"
#include <dirent.h>
#include <sys/stat.h>

/* Färgscheman inspirerade av Borland Turbo Pascal och Midnight Commander (mc) */
const char * const THEME_NAMES[] = {
    "Turbo Pascal (MC Default)",
    "MC Darkfar (Svart/Blå)",
    "MC Gotar (Svart/Gul)",
    "MC Sand (Ljusgrå/Blå)",
    "MC Matrix (Fosforgrön)",
    "Ljust (Grå/Orange/Blå)"
};

/* Initiera TUI, terminalinställningar och tangentbordssekvenser */
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
}

/* Återställ terminalen vid avslut */
void tui_cleanup(void) {
    endwin();
}

/* Aktivera valt färgschema (0..THEME_COUNT-1) */
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
            init_pair(CP_TITLE,          COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_LABEL,          COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_FIELD_INACTIVE, COLOR_CYAN,   COLOR_BLUE);
            init_pair(CP_FIELD_ACTIVE,   COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_FROZEN_BADGE,   COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_LIVE_BADGE,     COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_EDIT_BADGE,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_TABLE_HEADER,   COLOR_CYAN,   COLOR_BLUE);
            init_pair(CP_LOG_ROW,        COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_LOG_SELECTED,   COLOR_BLACK,  COLOR_CYAN);
            init_pair(CP_LOG_MARKED,     COLOR_YELLOW, COLOR_MAGENTA);
            init_pair(CP_FKEY_NUM,       COLOR_RED,    COLOR_WHITE);
            init_pair(CP_FKEY_LABEL,     COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_STATUS_OK,      COLOR_GREEN,  COLOR_BLUE);
            init_pair(CP_STATUS_WARN,    COLOR_WHITE,  COLOR_RED);
            init_pair(CP_FLAG_NEW,         COLOR_RED,    COLOR_BLUE);
            init_pair(CP_FLAG_HANDLED,     COLOR_GREEN,  COLOR_BLUE);
            init_pair(CP_FLAG_NEW_SEL,     COLOR_RED,    COLOR_CYAN);
            init_pair(CP_FLAG_HANDLED_SEL, COLOR_GREEN,  COLOR_CYAN);
            init_pair(CP_FLAG_NEW_MRK,     COLOR_RED,    COLOR_MAGENTA);
            init_pair(CP_FLAG_HANDLED_MRK, COLOR_GREEN,  COLOR_MAGENTA);
            break;

        case 1: /* 2. MC Darkfar (Svart bakgrund, blå/cyan lister, gul/vit text) */
            init_pair(CP_MAIN_BG,        COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_TOPBAR,         COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_TOPBAR_ACCENT,  COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_BORDER,         COLOR_BLUE,   COLOR_BLACK);
            init_pair(CP_BORDER_ACTIVE,  COLOR_CYAN,   COLOR_BLACK);
            init_pair(CP_TITLE,          COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_LABEL,          COLOR_CYAN,   COLOR_BLACK);
            init_pair(CP_FIELD_INACTIVE, COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_FIELD_ACTIVE,   COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_FROZEN_BADGE,   COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_LIVE_BADGE,     COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_EDIT_BADGE,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_TABLE_HEADER,   COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_LOG_ROW,        COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_LOG_SELECTED,   COLOR_BLACK,  COLOR_CYAN);
            init_pair(CP_LOG_MARKED,     COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_FKEY_NUM,       COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_FKEY_LABEL,     COLOR_BLACK,  COLOR_CYAN);
            init_pair(CP_STATUS_OK,      COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_STATUS_WARN,    COLOR_WHITE,  COLOR_RED);
            init_pair(CP_FLAG_NEW,         COLOR_RED,    COLOR_BLACK);
            init_pair(CP_FLAG_HANDLED,     COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_FLAG_NEW_SEL,     COLOR_RED,    COLOR_CYAN);
            init_pair(CP_FLAG_HANDLED_SEL, COLOR_GREEN,  COLOR_CYAN);
            init_pair(CP_FLAG_NEW_MRK,     COLOR_RED,    COLOR_BLUE);
            init_pair(CP_FLAG_HANDLED_MRK, COLOR_GREEN,  COLOR_BLUE);
            break;

        case 2: /* 3. MC Gotar (Svart bakgrund, bärnstensgul/grön stil) */
            init_pair(CP_MAIN_BG,        COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_TOPBAR,         COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_TOPBAR_ACCENT,  COLOR_RED,    COLOR_YELLOW);
            init_pair(CP_BORDER,         COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_BORDER_ACTIVE,  COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_TITLE,          COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_LABEL,          COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_FIELD_INACTIVE, COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_FIELD_ACTIVE,   COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_FROZEN_BADGE,   COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_LIVE_BADGE,     COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_EDIT_BADGE,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_TABLE_HEADER,   COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_LOG_ROW,        COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_LOG_SELECTED,   COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_LOG_MARKED,     COLOR_WHITE,  COLOR_GREEN);
            init_pair(CP_FKEY_NUM,       COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_FKEY_LABEL,     COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_STATUS_OK,      COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_STATUS_WARN,    COLOR_WHITE,  COLOR_RED);
            init_pair(CP_FLAG_NEW,         COLOR_RED,    COLOR_BLACK);
            init_pair(CP_FLAG_HANDLED,     COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_FLAG_NEW_SEL,     COLOR_RED,    COLOR_GREEN);
            init_pair(CP_FLAG_HANDLED_SEL, COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_FLAG_NEW_MRK,     COLOR_RED,    COLOR_GREEN);
            init_pair(CP_FLAG_HANDLED_MRK, COLOR_BLACK,  COLOR_GREEN);
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
            break;
        }
    }

    bkgd(COLOR_PAIR(CP_MAIN_BG));
}

/* Rita en ram (box) med titel och markering om sektionen har fokus */
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
        attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvprintw(y, x + 2, " %s ", title);
        attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
    }
}

/* Formatera både UTC och lokal tid för statusraden längst upp */
static void format_top_bar_clocks(const StationSettings *st, char *out, size_t out_sz) {
    time_t now = time(NULL);
    struct tm utc_tm;
    struct tm loc_tm;

    struct tm *u = gmtime(&now);
    if (u == NULL) {
        if (out_sz > 0) out[0] = '\0';
        return;
    }
    utc_tm = *u;

    long offset_sec = 0;
    if (st != NULL && parse_tz_offset_seconds(st->timezone, &offset_sec)) {
        time_t adj = now + offset_sec;
        struct tm *l = gmtime(&adj);
        if (l != NULL) {
            loc_tm = *l;
        } else {
            loc_tm = utc_tm;
        }
    } else {
        struct tm *l = localtime(&now);
        if (l != NULL) {
            loc_tm = *l;
        } else {
            loc_tm = utc_tm;
        }
    }

    char utc_part[32], loc_part[16];
    strftime(utc_part, sizeof(utc_part), "%Y-%m-%d %H:%M UTC", &utc_tm);
    strftime(loc_part, sizeof(loc_part), "%H:%M", &loc_tm);

    if (COLS >= 74) {
        snprintf(out, out_sz, "%s  |  Lokal: %s", utc_part, loc_part);
    } else {
        snprintf(out, out_sz, "%s (%s)", utc_part, loc_part);
    }
}

/* Rita den översta statusraden (inklusive UTC och lokal tid) */
void draw_top_bar(const StationSettings *st) {
    char clock_str[80] = "";
    format_top_bar_clocks(st, clock_str, sizeof(clock_str));

    const char *my_call = st->call[0] ? st->call : "(ange i F12)";
    const char *my_loc  = st->gridsquare[0] ? st->gridsquare : "----";

    attron(COLOR_PAIR(CP_TOPBAR));
    mvhline(0, 0, ' ', COLS);
    mvprintw(0, 1, "Station: ");
    attroff(COLOR_PAIR(CP_TOPBAR));

    attron(COLOR_PAIR(CP_TOPBAR_ACCENT) | A_BOLD);
    printw("%s", my_call);
    attroff(COLOR_PAIR(CP_TOPBAR_ACCENT) | A_BOLD);

    attron(COLOR_PAIR(CP_TOPBAR));
    if (COLS >= 88 && st->qth[0] != '\0') {
        printw(" (%s)", st->qth);
    }
    printw("  |  Lokator: ");
    attroff(COLOR_PAIR(CP_TOPBAR));

    attron(COLOR_PAIR(CP_TOPBAR_ACCENT) | A_BOLD);
    printw("%s", my_loc);
    attroff(COLOR_PAIR(CP_TOPBAR_ACCENT) | A_BOLD);

    int clock_len = (int)strlen(clock_str);
    int clock_col = COLS - clock_len - 1;
    if (clock_col > 1) {
        attron(COLOR_PAIR(CP_TOPBAR));
        mvprintw(0, clock_col, "%s", clock_str);
        attroff(COLOR_PAIR(CP_TOPBAR));
    }
}

/* Rita ett inmatningsfält där etikettens ':' hamnar exakt på kolumn colon_col */
void draw_field_aligned(int row, int colon_col, const char *label, const char *val,
                        int width, int is_active, int cursor_pos,
                        int *cur_y, int *cur_x) {
    if (width < 2) width = 2;

    int label_w = utf8_display_width(label);
    int label_col = colon_col - label_w;
    if (label_col < 1) label_col = 1;

    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(row, label_col, "%s:", label);
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

    int fcol = colon_col + 2;

    if (is_active) {
        attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        for (int i = 0; i < width; ++i) {
            mvaddch(row, fcol + i, ' ');
        }
    } else {
        attron(COLOR_PAIR(CP_FIELD_INACTIVE));
        for (int i = 0; i < width; ++i) {
            mvaddch(row, fcol + i, ' ');
        }
    }

    int total_chars = utf8_display_width(val);
    int start_char = 0;
    int cpos = is_active ? cursor_pos : 0;
    if (cpos < 0) cpos = 0;
    if (cpos > total_chars) cpos = total_chars;

    if (is_active && cpos >= width) {
        start_char = cpos - width + 1;
    }

    size_t start_byte = utf8_char_to_byte(val, start_char);
    size_t end_byte   = utf8_char_to_byte(val, start_char + width);
    int byte_len = (int)(end_byte - start_byte);
    mvprintw(row, fcol, "%.*s", byte_len, val + start_byte);

    if (is_active) {
        attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        int screen_offset = cpos - start_char;
        if (screen_offset < 0) screen_offset = 0;
        if (screen_offset >= width) screen_offset = width - 1;
        *cur_y = row;
        *cur_x = fcol + screen_offset;
    } else {
        attroff(COLOR_PAIR(CP_FIELD_INACTIVE));
    }
}

/* Rita Band-väljarfältet (< 20m >) linjerat med ':' på colon_col */
void draw_band_selector_aligned(int row, int colon_col, const char *val,
                                int is_active, int band_selecting,
                                int *cur_y, int *cur_x) {
    const char *label = "Band";
    int label_w = utf8_display_width(label);
    int label_col = colon_col - label_w;
    if (label_col < 1) label_col = 1;

    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(row, label_col, "%s:", label);
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

    int fcol = colon_col + 2;
    const char *shown = (val && val[0]) ? val : "20 m";

    if (is_active) {
        int attr = A_BOLD | (band_selecting ? A_REVERSE : 0);
        attron(COLOR_PAIR(CP_FIELD_ACTIVE) | attr);
        mvprintw(row, fcol, "<%-5.5s>", shown);
        attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | attr);
        *cur_y = row;
        *cur_x = fcol + 1;
    } else {
        attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
        mvprintw(row, fcol, "<%-5.5s>", shown);
        attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
    }
}

/* Rita raden för bandval när Band är aktivt */
void draw_band_popup_list(int row, const char *current_band, int band_selecting) {
    int x = 2;
    if (!band_selecting) {
        attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvprintw(row, x, " Band: Tryck Enter för att välja band ");
        attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
        return;
    }

    int cur_idx = find_band_index(current_band);
    attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
    if (COLS >= 95) {
        mvprintw(row, x, "Band (Pilar väljer, Enter=Klar): ");
        x += 33;
    } else {
        mvprintw(row, x, "Band: ");
        x += 6;
    }
    attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);

    static const char * const SHORT_NAMES[] = {
        "160", "80", "60", "40", "30", "20", "17", "15", "12", "10", "6", "2", "70cm", "23cm"
    };

    int start_i = 0;
    if (cur_idx >= 7 && COLS < 80) {
        start_i = cur_idx - 5;
        if (start_i + 8 > BAND_OPTION_COUNT) start_i = BAND_OPTION_COUNT - 8;
        if (start_i < 0) start_i = 0;
    }

    for (int i = start_i; i < BAND_OPTION_COUNT && x + 5 < COLS - 1; ++i) {
        if (i == cur_idx) {
            attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD | A_REVERSE);
            mvprintw(row, x, "[%s]", SHORT_NAMES[i]);
            attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD | A_REVERSE);
            x += (int)strlen(SHORT_NAMES[i]) + 2;
        } else {
            attron(COLOR_PAIR(CP_FIELD_INACTIVE));
            mvprintw(row, x, " %s ", SHORT_NAMES[i]);
            attroff(COLOR_PAIR(CP_FIELD_INACTIVE));
            x += (int)strlen(SHORT_NAMES[i]) + 2;
        }
    }
}

/* Rita funktionstangentmenyn allra längst ned */
void draw_bottom_fkey_bar(int mode_type, FocusMode focus_mode, const char *search_filter) {
    int row = LINES - 1;
    attron(COLOR_PAIR(CP_FKEY_LABEL));
    mvhline(row, 0, ' ', COLS);
    attroff(COLOR_PAIR(CP_FKEY_LABEL));

    struct {
        const char *key;
        const char *label;
    } btns_main[] = {
        {"F1/Esc", "Nytt"},
        {"Enter",  focus_mode == FOCUS_LOG ? "Redigera" : "Logga"},
        {"F3",     (search_filter && search_filter[0]) ? "Sök*" : "Sök"},
        {"F4",     focus_mode == FOCUS_LOG ? "Inmatn." : "Loggrad"},
        {"Ins",    focus_mode == FOCUS_LOG ? "Märk" : NULL},
        {"F5",     focus_mode == FOCUS_LOG ? "Hanterad" : NULL},
        {"F6",     focus_mode == FOCUS_LOG ? "Ny" : NULL},
        {"F8",     focus_mode == FOCUS_LOG ? "Radera" : NULL},
        {"F9",     "Hjälp"},
        {"F10",    "Avsluta"},
        {"F12",    "Inställn."}
    };

    struct {
        const char *key;
        const char *label;
    } btns_modal[] = {
        {"Enter/F2",  "Spara"},
        {"F5",        "Import"},
        {"F6",        "Export"},
        {"Tab/Pilar", "Byt fält"},
        {"Esc",       "Tillbaka"}
    };

    int col = 1;
    if (mode_type == 0) {
        int count = (int)(sizeof(btns_main) / sizeof(btns_main[0]));
        for (int i = 0; i < count; ++i) {
            if (btns_main[i].label == NULL) continue;
            int btn_len = (int)strlen(btns_main[i].key) + (int)strlen(btns_main[i].label) + 2;
            if (col + btn_len >= COLS) break;

            attron(COLOR_PAIR(CP_FKEY_NUM) | A_BOLD);
            mvprintw(row, col, "%s", btns_main[i].key);
            col += (int)strlen(btns_main[i].key);
            attroff(COLOR_PAIR(CP_FKEY_NUM) | A_BOLD);

            attron(COLOR_PAIR(CP_FKEY_LABEL));
            mvprintw(row, col, " %-s ", btns_main[i].label);
            col += (int)strlen(btns_main[i].label) + 2;
            attroff(COLOR_PAIR(CP_FKEY_LABEL));
        }
    } else {
        int count = (int)(sizeof(btns_modal) / sizeof(btns_modal[0]));
        for (int i = 0; i < count && col < COLS - 4; ++i) {
            attron(COLOR_PAIR(CP_FKEY_NUM) | A_BOLD);
            mvprintw(row, col, "%s", btns_modal[i].key);
            col += (int)strlen(btns_modal[i].key);
            attroff(COLOR_PAIR(CP_FKEY_NUM) | A_BOLD);

            attron(COLOR_PAIR(CP_FKEY_LABEL));
            mvprintw(row, col, " %-s  ", btns_modal[i].label);
            col += (int)strlen(btns_modal[i].label) + 3;
            attroff(COLOR_PAIR(CP_FKEY_LABEL));
        }
    }
}

/* Returnera antalet tillgängliga datarader i loggradsfönstret */
int get_avail_log_rows(void) {
    int log_top = 8;
    int log_height = (LINES - 1) - log_top;
    int first_data_row = log_top + 2;
    int avail = (log_top + log_height - 1) - first_data_row;
    return (avail > 0) ? avail : 1;
}

int build_filtered_indices(const QSOList *log_list, const char *search_filter,
                                  int *out_indices, int max_out) {
    int count = 0;
    for (int i = (int)log_list->count - 1; i >= 0 && count < max_out; --i) {
        const QSO *item = &log_list->items[i];
        if (search_filter != NULL && search_filter[0] != '\0') {
            int match = utf8_contains_ci(item->call, search_filter) ||
                        utf8_contains_ci(item->name, search_filter) ||
                        utf8_contains_ci(item->qth, search_filter) ||
                        utf8_contains_ci(item->gridsquare, search_filter);
            if (!match) continue;
        }
        out_indices[count++] = i;
    }
    return count;
}

/* Rita hela huvudskärmen med fälten logiskt linjerade på ':' */
void draw_main_screen(const StationSettings *st, const QSO *qso,
                      FocusMode focus_mode, int active_field, int band_selecting,
                      int field_cursor_pos, int time_frozen, int editing_index,
                      const QSOList *log_list, int selected_log_pos, int *log_scroll_offset,
                      const char *search_filter, const char *status_msg,
                      const char *qrb_str,
                      int *cur_y, int *cur_x) {
    erase();

    if (COLS < MIN_COLS || LINES < MIN_LINES) {
        attron(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);
        mvprintw(LINES / 2 - 1, 2, "Fönstret är för litet (%dx%d).", COLS, LINES);
        mvprintw(LINES / 2,     2, "Minsta storlek är %dx%d. Förstora terminalen.", MIN_COLS, MIN_LINES);
        attroff(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);
        refresh();
        return;
    }

    int in_input = (focus_mode == FOCUS_INPUT);

    /* 1. Längst upp: Statusrad */
    draw_top_bar(st);

    /* 2. Under detta: Inmatningsfälten (Rader 1..7) */
    draw_box_with_title(1, 0, 7, COLS,
                        editing_index >= 0 ? "Redigera QSO" : "Ny förbindelse",
                        in_input);

    if (editing_index >= 0) {
        attron(COLOR_PAIR(CP_EDIT_BADGE) | A_BOLD);
        mvprintw(1, 18, " REDIGERAR #%d ", editing_index + 1);
        attroff(COLOR_PAIR(CP_EDIT_BADGE) | A_BOLD);
    } else if (time_frozen) {
        attron(COLOR_PAIR(CP_FROZEN_BADGE) | A_BOLD);
        mvprintw(1, 20, " TID FRYST ");
        attroff(COLOR_PAIR(CP_FROZEN_BADGE) | A_BOLD);
    } else {
        attron(COLOR_PAIR(CP_LIVE_BADGE) | A_BOLD);
        mvprintw(1, 20, " UTC LIVE ");
        attroff(COLOR_PAIR(CP_LIVE_BADGE) | A_BOLD);
    }

    if (status_msg != NULL && status_msg[0] != '\0' && COLS >= 60) {
        int max_msg = COLS - 36;
        if (max_msg > 0) {
            attron(COLOR_PAIR(CP_STATUS_OK) | A_BOLD);
            mvprintw(1, 34, "%.*s", max_msg, status_msg);
            attroff(COLOR_PAIR(CP_STATUS_OK) | A_BOLD);
        }
    }

    /* Kolumnlinjering på ':' i inmatningsrutan:
     * - Kolumn 1 (colon1): Signal:, QTH:, Datum:, Frekv.:, Not:
     * - Kolumn 2 (colon2): Namn:, Lokator:, UTC:, Band:
     * - Kolumn 3 (colon3): RST T:, DXCC:, Mode:
     * - Kolumn 4 (colon4 på rad 2): RST R: */
    int colon1 = 11;
    int colon2 = (COLS >= 76) ? 36 : 34;
    int colon3 = (COLS >= 76) ? 57 : 50;
    int colon4 = (COLS >= 76) ? 73 : (COLS - 6);

    const char *lbl_mode = "Mode";

    int w_call = (colon2 - 10) - (colon1 + 2);
    if (w_call > 12) w_call = 12;
    if (w_call < 7)  w_call = 7;

    int w_name = (colon3 - 9) - (colon2 + 2);
    if (w_name < 6) w_name = 6;

    int w_col3_end = (COLS - 2) - (colon3 + 2);
    if (w_col3_end < 6) w_col3_end = 6;

    /* Rad 2: Signal: [...] | Namn: [...] | RST T: [...] | RST R: [...] */
    draw_field_aligned(2, colon1, "Signal",   qso->call, w_call,
                       in_input && active_field == FLD_CALL, field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(2, colon2, "Namn",     qso->name, w_name,
                       in_input && active_field == FLD_NAME, field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(2, colon3, "RST T",    qso->rst_sent, 4,
                       in_input && active_field == FLD_RSTS, field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(2, colon4, "RST R",    qso->rst_rcvd, 4,
                       in_input && active_field == FLD_RSTR, field_cursor_pos, cur_y, cur_x);

    /* Rad 3: QTH: [...] | Lokator: [...] | DXCC: [...] | QRB: [...] */
    int w_qth = (colon2 - 10) - (colon1 + 2);
    if (w_qth < 6) w_qth = 6;
    int w_grid = (colon3 - 9) - (colon2 + 2);
    if (w_grid < 6) w_grid = 6;
    int w_dxcc = (colon4 - 6) - (colon3 + 2);
    if (w_dxcc < 6) w_dxcc = 6;

    draw_field_aligned(3, colon1, "QTH",      qso->qth,        w_qth,
                       in_input && active_field == FLD_QTH, field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(3, colon2, "Lokator",  qso->gridsquare, w_grid,
                       in_input && active_field == FLD_GRID, field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(3, colon3, "DXCC",     qso->dxcc,       w_dxcc,
                       in_input && active_field == FLD_DXCC, field_cursor_pos, cur_y, cur_x);

    /* QRB under RST R på rad 3 */
    int dummy_y, dummy_x;
    draw_field_aligned(3, colon4, "QRB",      (qrb_str && qrb_str[0]) ? qrb_str : "-", 8,
                       0, 0, &dummy_y, &dummy_x);

    /* Rad 4: Datum: [...] | UTC: [...] */
    draw_field_aligned(4, colon1, "Datum",     qso->date,       11,
                       in_input && active_field == FLD_DATE, field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(4, colon2, "UTC",       qso->time_utc,   6,
                       in_input && active_field == FLD_TIME, field_cursor_pos, cur_y, cur_x);

    /* Rad 5: Frekv.: [...] | Band: <20m  > | Mode: [...] */
    draw_field_aligned(5,         colon1, "Frekv.",   qso->freq, 9,
                       in_input && active_field == FLD_FREQ, field_cursor_pos, cur_y, cur_x);
    draw_band_selector_aligned(5, colon2,             qso->band,
                       in_input && active_field == FLD_BAND, band_selecting, cur_y, cur_x);
    draw_field_aligned(5,         colon3, lbl_mode,   qso->mode, w_col3_end > 8 ? 8 : w_col3_end,
                       in_input && active_field == FLD_MODE, field_cursor_pos, cur_y, cur_x);

    /* Rad 6: Not: [...] */
    int w_comment = (COLS - 2) - (colon1 + 2);
    draw_field_aligned(6, colon1, "Not",      qso->comment, w_comment,
                       in_input && active_field == FLD_COMM, field_cursor_pos, cur_y, cur_x);

    if (in_input && active_field == FLD_BAND) {
        draw_band_popup_list(7, qso->band, band_selecting);
    }

    /* 3. Under inmatningsrutan: Redan loggade QSO ("Loggrader") */
    int log_top = 8;
    int log_height = (LINES - 1) - log_top;
    int marked_count = 0;
    for (size_t i = 0; i < log_list->count; ++i) {
        if (log_list->items[i].marked) marked_count++;
    }

    char log_title[112];
    if (focus_mode == FOCUS_LOG) {
        if (marked_count > 0) {
            snprintf(log_title, sizeof(log_title),
                     "Loggrader – Ins=Märk, F8=Radera (%d markerade, %zu QSO)",
                     marked_count, log_list->count);
        } else {
            snprintf(log_title, sizeof(log_title),
                     "Loggrader – Välj med Pilar, Ins=Märk, F8=Radera, Enter=Redigera (%zu QSO)",
                     log_list->count);
        }
    } else if (search_filter != NULL && search_filter[0] != '\0') {
        if (marked_count > 0) {
            snprintf(log_title, sizeof(log_title), "Loggrader (Filter: %s | %d markerade | %zu QSO)",
                     search_filter, marked_count, log_list->count);
        } else {
            snprintf(log_title, sizeof(log_title), "Loggrader (Filter: %s | %zu QSO)",
                     search_filter, log_list->count);
        }
    } else {
        if (marked_count > 0) {
            snprintf(log_title, sizeof(log_title), "Loggrader (%d markerade, %zu QSO)",
                     marked_count, log_list->count);
        } else {
            snprintf(log_title, sizeof(log_title), "Loggrader (%zu QSO)", log_list->count);
        }
    }
    draw_box_with_title(log_top, 0, log_height, COLS, log_title, focus_mode == FOCUS_LOG);

    int hdr_row = log_top + 1;
    int first_data_row = log_top + 2;
    int avail_log_rows = get_avail_log_rows();

    /* Band och Frekv i loggraderna: Band är högerställt (%5s) och Frekv (%8s) med 3 decimaler */
    attron(COLOR_PAIR(CP_TABLE_HEADER) | A_BOLD);
    if (COLS >= 92) {
        int rem_hdr = (COLS - 5) - 82;
        if (rem_hdr < 3) rem_hdr = 3;
        mvprintw(hdr_row, 2, "%-10s %-5s %-11s %5s %8s %-5s %-7s %-8s %-12s %-.*s",
                 "Datum", "UTC", "Signal", "Band", "Frekv", "Mode", "RST S/M", "Lokator", "Namn", rem_hdr, "QTH");
    } else if (COLS >= 74) {
        int rem_hdr = (COLS - 5) - 66;
        if (rem_hdr < 4) rem_hdr = 4;
        mvprintw(hdr_row, 2, "%-10s %-5s %-10s %5s %8s %-5s %-7s %-6s %-.*s",
                 "Datum", "UTC", "Signal", "Band", "Frekv", "Mode", "RST", "Lok.", rem_hdr, "Namn/QTH");
    } else {
        int rem_hdr = (COLS - 5) - 56;
        if (rem_hdr < 3) rem_hdr = 3;
        mvprintw(hdr_row, 2, "%-10s %-5s %-9s %5s %8s %-4s %-7s %-.*s",
                 "Datum", "UTC", "Signal", "Band", "Frekv", "Mode", "RST", rem_hdr, "Namn");
    }
    mvprintw(hdr_row, COLS - 4, "St");
    attroff(COLOR_PAIR(CP_TABLE_HEADER) | A_BOLD);

    int max_filtered = (int)log_list->count > 0 ? (int)log_list->count : 1;
    int *filtered_idx = malloc((size_t)max_filtered * sizeof(int));
    int filtered_count = 0;
    if (filtered_idx != NULL) {
        filtered_count = build_filtered_indices(log_list, search_filter, filtered_idx, max_filtered);
    }

    if (avail_log_rows > 0 && filtered_count > 0) {
        if (selected_log_pos < *log_scroll_offset) {
            *log_scroll_offset = selected_log_pos;
        } else if (selected_log_pos >= *log_scroll_offset + avail_log_rows) {
            *log_scroll_offset = selected_log_pos - avail_log_rows + 1;
        }
        if (*log_scroll_offset < 0) *log_scroll_offset = 0;
    } else {
        *log_scroll_offset = 0;
    }

    int shown = 0;
    for (int pos = *log_scroll_offset; pos < filtered_count && shown < avail_log_rows; ++pos) {
        int qso_idx = filtered_idx[pos];
        const QSO *item = &log_list->items[qso_idx];
        int r = first_data_row + shown;
        int is_selected_row = (focus_mode == FOCUS_LOG && pos == selected_log_pos);

        char d_disp[16], t_disp[16], f_disp[16], b_disp[16], rst_disp[36];
        format_date_yyyy_mm_dd(item->date, d_disp, sizeof(d_disp));
        format_time_hh_mm(item->time_utc, t_disp, sizeof(t_disp));
        format_band_display(item->band, b_disp, sizeof(b_disp));
        parse_and_normalize_freq(item->freq, f_disp, sizeof(f_disp));
        snprintf(rst_disp, sizeof(rst_disp), "%s/%s",
                 item->rst_sent[0] ? item->rst_sent : "-",
                 item->rst_rcvd[0] ? item->rst_rcvd : "-");

        if (is_selected_row) {
            attron(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
            mvhline(r, 1, ' ', COLS - 2);
            if (item->marked) {
                mvaddch(r, 1, '*');
            } else {
                mvaddch(r, 1, '>');
            }
            *cur_y = r;
            *cur_x = 2;
        } else if (item->marked) {
            attron(COLOR_PAIR(CP_LOG_MARKED) | A_BOLD);
            mvhline(r, 1, ' ', COLS - 2);
            mvaddch(r, 1, '*');
        } else {
            attron(COLOR_PAIR(CP_LOG_ROW) | A_BOLD);
            mvaddch(r, 1, ' ');
        }

        if (COLS >= 92) {
            int rem_qth = (COLS - 5) - 82;
            if (rem_qth < 2) rem_qth = 2;
            mvprintw(r, 2, "%-10.10s %-5.5s %-11.11s %5.5s %8.8s %-5.5s %-7.7s %-8.8s %-12.12s %-.*s",
                     d_disp[0] ? d_disp : "-",
                     t_disp[0] ? t_disp : "-",
                     item->call[0] ? item->call : "-",
                     b_disp[0] ? b_disp : "-",
                     f_disp[0] ? f_disp : "-",
                     item->mode[0] ? item->mode : "-",
                     rst_disp,
                     item->gridsquare[0] ? item->gridsquare : "-",
                     item->name[0] ? item->name : "-",
                     rem_qth,
                     item->qth[0] ? item->qth : "-");
        } else if (COLS >= 74) {
            int rem = (COLS - 5) - 66;
            if (rem < 2) rem = 2;
            const char *nq = item->name[0] ? item->name : (item->qth[0] ? item->qth : "-");
            mvprintw(r, 2, "%-10.10s %-5.5s %-10.10s %5.5s %8.8s %-5.5s %-7.7s %-6.6s %-.*s",
                     d_disp[0] ? d_disp : "-",
                     t_disp[0] ? t_disp : "-",
                     item->call[0] ? item->call : "-",
                     b_disp[0] ? b_disp : "-",
                     f_disp[0] ? f_disp : "-",
                     item->mode[0] ? item->mode : "-",
                     rst_disp,
                     item->gridsquare[0] ? item->gridsquare : "-",
                     rem, nq);
        } else {
            int rem = (COLS - 5) - 56;
            if (rem < 2) rem = 2;
            mvprintw(r, 2, "%-10.10s %-5.5s %-9.9s %5.5s %8.8s %-4.4s %-7.7s %-.*s",
                     d_disp[0] ? d_disp : "-",
                     t_disp[0] ? t_disp : "-",
                     item->call[0] ? item->call : "-",
                     b_disp[0] ? b_disp : "-",
                     f_disp[0] ? f_disp : "-",
                     item->mode[0] ? item->mode : "-",
                     rst_disp,
                     rem,
                     item->name[0] ? item->name : "-");
        }

        int flag_cp = is_selected_row ? (item->handled ? CP_FLAG_HANDLED_SEL : CP_FLAG_NEW_SEL)
                    : item->marked    ? (item->handled ? CP_FLAG_HANDLED_MRK : CP_FLAG_NEW_MRK)
                                      : (item->handled ? CP_FLAG_HANDLED : CP_FLAG_NEW);
        attron(COLOR_PAIR(flag_cp) | A_BOLD);
        mvprintw(r, COLS - 3, "%s", item->handled ? "✓" : "✗");
        attroff(COLOR_PAIR(flag_cp) | A_BOLD);

        if (is_selected_row) {
            attroff(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
        } else {
            attroff(COLOR_PAIR(CP_LOG_ROW) | A_BOLD);
        }
        shown++;
    }

    free(filtered_idx);

    if (shown == 0 && avail_log_rows > 0) {
        mvprintw(first_data_row, 2, "Inga förbindelser att visa ännu. Skriv in anropssignal ovan och tryck Enter.");
    }

    /* 4. Allra längst ned: Funktionstangentrad */
    draw_bottom_fkey_bar(0, focus_mode, search_filter);

    move(*cur_y, *cur_x);
    refresh();
}

/* F9: Hjälpskärm */
void run_help_modal(const StationSettings *st) {
    while (1) {
        erase();
        draw_top_bar(st);
        draw_box_with_title(1, 0, LINES - 2, COLS, "Hjälp – QSO-logg (F9)", 1);

        attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvprintw(3, 3, "Tangentbordskommandon:");
        attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);

        attron(COLOR_PAIR(CP_LABEL));
        mvprintw(4,  4, "Tab / Shift-Tab   : Hoppa till nästa / föregående inmatningsfält");
        mvprintw(5,  4, "Vänster / Höger   : Hoppa till föregående / nästa fält");
        mvprintw(6,  4, "Pil upp / Pil ned : Gå till närmaste fält över eller under aktivt fält");
        mvprintw(7,  4, "Enter på Band     : Aktivera bandval (byt med pilar, Enter bekräftar)");
        mvprintw(8,  4, "F1 / Esc          : Rensa inmatningsrutan (UTC-klockan tickar igen)");
        mvprintw(9,  4, "F2 / Enter        : Logga QSO direkt med ifyllda uppgifter");
        mvprintw(10, 4, "F3 / Ctrl-F       : Sök/filtrera (Signal, Namn, QTH, Lokator)");
        mvprintw(11, 4, "F4 / Ctrl-O       : Växla mellan Inmatningsrutan och Loggraderna");
        mvprintw(12, 4, "Loggrader         : Välj med Pilar, Enter = Redigera");
        mvprintw(13, 4, "                    Ins = Markera rad (och gå ned), F8 = Radera markerade/aktuell");
        mvprintw(14, 4, "                    F5 = Markera som hanterad (✓), F6 = Markera som ny (✗)");
        mvprintw(15, 4, "                    Home/End = Första/sista rad på sidan, PgUp/PgDn = Bläddra sida");
        mvprintw(16, 4, "                    Ctrl-Home / Ctrl-End = Översta / sista raden i hela loggen");
        mvprintw(17, 4, "Inmatningsfält    : Home = Anropssignal, End = Not (Kommentarsfältet)");
        mvprintw(18, 4, "F9                : Visa denna hjälpsida");
        mvprintw(19, 4, "F10 / Ctrl-Q      : Avsluta programmet");
        mvprintw(20, 4, "F12               : Inställningar (Station, Färg, F5 Import, F6 Export)");

        if (LINES >= 26) {
            attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
            mvprintw(21, 3, "Smarta funktioner:");
            attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
            attron(COLOR_PAIR(CP_LABEL));
            mvprintw(22, 4, "- Frekvens utanför amatörband ger beräknat metervärde (300/f avrundat).");
            mvprintw(23, 4, "- DXCC identifieras automatiskt från dxcc.txt när du skriver anropssignal.");
            mvprintw(24, 4, "- Svenska signaler (SA-SM, 7S, 8S) kan slås upp i SM Callbook (slås på i F12).");
            mvprintw(25, 4, "- Fältet 'Not' lagras som standard ADIF-kommentar (COMMENT).");
        }
        attroff(COLOR_PAIR(CP_LABEL));

        int row = LINES - 1;
        attron(COLOR_PAIR(CP_FKEY_LABEL));
        mvhline(row, 0, ' ', COLS);
        mvprintw(row, 2, "Tryck Esc, Enter eller F9 för att stänga hjälpen.");
        attroff(COLOR_PAIR(CP_FKEY_LABEL));

        refresh();

        int ch = getch();
        if (ch == 27 || ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == KEY_F(9)) {
            break;
        }
    }
}

/* Struktur och hjälpfunktioner för filväljardialogen */
typedef struct {
    char name[256];
    int is_dir;
    long size_bytes;
} FileDialogItem;

static int cmp_file_dialog_item(const void *a_ptr, const void *b_ptr) {
    const FileDialogItem *a = (const FileDialogItem *)a_ptr;
    const FileDialogItem *b = (const FileDialogItem *)b_ptr;
    if (strcmp(a->name, "..") == 0) return -1;
    if (strcmp(b->name, "..") == 0) return 1;
    if (a->is_dir != b->is_dir) {
        return a->is_dir ? -1 : 1;
    }
    return strcasecmp(a->name, b->name);
}

static int matches_extension(const char *filename, const char *ext_list) {
    if (ext_list == NULL || ext_list[0] == '\0') return 1;
    const char *dot = strrchr(filename, '.');
    if (dot == NULL) return 0;

    char buf[128];
    copy_str(buf, sizeof(buf), ext_list);
    char *token = strtok(buf, ";, ");
    while (token != NULL) {
        while (*token == '*' || *token == ' ') token++;
        if (*token == '.') {
            if (strcasecmp(dot, token) == 0) return 1;
        } else {
            if (strcasecmp(dot + 1, token) == 0) return 1;
        }
        token = strtok(NULL, ";, ");
    }
    return 0;
}

static void format_file_size(long bytes, char *out, size_t out_sz) {
    if (bytes < 0) {
        copy_str(out, out_sz, "");
    } else if (bytes < 1024) {
        snprintf(out, out_sz, "%ld B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(out, out_sz, "%.1f KB", (double)bytes / 1024.0);
    } else if (bytes < 1024L * 1024 * 1024) {
        snprintf(out, out_sz, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
    } else {
        snprintf(out, out_sz, "%.1f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    }
}

static int load_dir_items(const char *dir_path, int filter_active, const char *ext_filter,
                          FileDialogItem **items_out, size_t *count_out) {
    *items_out = NULL;
    *count_out = 0;

    DIR *d = opendir(dir_path);
    if (d == NULL) return -1;

    size_t cap = 32;
    FileDialogItem *items = malloc(cap * sizeof(FileDialogItem));
    if (items == NULL) {
        closedir(d);
        return -1;
    }
    size_t count = 0;

    if (strcmp(dir_path, "/") != 0) {
        copy_str(items[count].name, sizeof(items[count].name), "..");
        items[count].is_dir = 1;
        items[count].size_bytes = -1;
        count++;
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        if (ent->d_name[0] == '.') {
            continue;
        }

        char full_path[PATH_MAX + 512];
        if (strcmp(dir_path, "/") == 0) {
            snprintf(full_path, sizeof(full_path), "/%s", ent->d_name);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, ent->d_name);
        }

        struct stat st;
        if (stat(full_path, &st) != 0) {
            continue;
        }

        int is_dir = S_ISDIR(st.st_mode);
        int is_reg = S_ISREG(st.st_mode);

        if (!is_dir && !is_reg) {
            continue;
        }

        if (is_reg && filter_active && !matches_extension(ent->d_name, ext_filter)) {
            continue;
        }

        if (count >= cap) {
            size_t new_cap = cap * 2;
            FileDialogItem *new_items = realloc(items, new_cap * sizeof(FileDialogItem));
            if (new_items == NULL) break;
            items = new_items;
            cap = new_cap;
        }

        copy_str(items[count].name, sizeof(items[count].name), ent->d_name);
        items[count].is_dir = is_dir;
        items[count].size_bytes = is_dir ? -1 : (long)st.st_size;
        count++;
    }

    closedir(d);

    if (count > 1) {
        qsort(items, count, sizeof(FileDialogItem), cmp_file_dialog_item);
    }

    *items_out = items;
    *count_out = count;
    return 0;
}

static void ensure_adif_extension(char *filename, size_t max_len) {
    if (filename == NULL || filename[0] == '\0') return;
    const char *base = get_path_basename(filename);
    const char *dot = strrchr(base, '.');
    if (dot == NULL || dot[1] == '\0') {
        if (dot != NULL && dot[1] == '\0') {
            strncat(filename, "adi", max_len - strlen(filename) - 1);
        } else {
            strncat(filename, ".adi", max_len - strlen(filename) - 1);
        }
    }
}

/* Generisk filväljardialog med ncurses (stöder både öppna/importera och spara/exportera) */
int run_file_dialog(const char *title, const char *start_dir, const char *ext_filter,
                    char *selected_path, size_t max_path_len,
                    int is_save_mode, const char *default_filename) {
    char current_dir[PATH_MAX];
    if (start_dir != NULL && start_dir[0] != '\0') {
        if (realpath(start_dir, current_dir) == NULL) {
            if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
                copy_str(current_dir, sizeof(current_dir), ".");
            }
        }
    } else {
        if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
            copy_str(current_dir, sizeof(current_dir), ".");
        }
    }

    int filter_active = (ext_filter != NULL && ext_filter[0] != '\0') ? 1 : 0;
    FileDialogItem *items = NULL;
    size_t count = 0;
    load_dir_items(current_dir, filter_active, ext_filter, &items, &count);

    int selected_pos = 0;
    int scroll_offset = 0;
    int focus_area = is_save_mode ? 0 : 1; /* 0 = filnamn, 1 = fillista */

    char fname_buf[256];
    if (default_filename != NULL && default_filename[0] != '\0') {
        copy_str(fname_buf, sizeof(fname_buf), default_filename);
    } else {
        copy_str(fname_buf, sizeof(fname_buf), "");
    }
    int fname_cursor_pos = utf8_display_width(fname_buf);

    while (1) {
        erase();
        int dialog_w = COLS - 6;
        if (dialog_w > 78) dialog_w = 78;
        if (dialog_w < 50) dialog_w = 50;
        int dialog_h = LINES - 4;
        if (dialog_h > 24) dialog_h = 24;
        if (dialog_h < 14) dialog_h = 14;

        int top_y = (LINES - dialog_h) / 2;
        int left_x = (COLS - dialog_w) / 2;

        /* Bakgrundsfyllning för dialogrutan */
        attron(COLOR_PAIR(CP_MAIN_BG));
        for (int r = 0; r < dialog_h; ++r) {
            mvhline(top_y + r, left_x, ' ', dialog_w);
        }
        attroff(COLOR_PAIR(CP_MAIN_BG));

        draw_box_with_title(top_y, left_x, dialog_h, dialog_w, title, 1);

        /* Katalograd */
        attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
        mvprintw(top_y + 1, left_x + 2, "Katalog: ");
        attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

        int max_dir_disp = dialog_w - 14;
        int dir_len = (int)strlen(current_dir);
        if (dir_len > max_dir_disp && max_dir_disp > 3) {
            attron(COLOR_PAIR(CP_FIELD_INACTIVE));
            mvprintw(top_y + 1, left_x + 11, "...%s", current_dir + (dir_len - max_dir_disp + 3));
            attroff(COLOR_PAIR(CP_FIELD_INACTIVE));
        } else {
            attron(COLOR_PAIR(CP_FIELD_INACTIVE));
            mvprintw(top_y + 1, left_x + 11, "%-.*s", max_dir_disp, current_dir);
            attroff(COLOR_PAIR(CP_FIELD_INACTIVE));
        }

        /* Om spara-läge: Filnamnsinmatningsfält */
        int list_top_y = top_y + 2;
        int list_height = dialog_h - 4;
        int cur_input_y = -1, cur_input_x = -1;

        if (is_save_mode) {
            int fn_y = top_y + 2;
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(fn_y, left_x + 2, "Filnamn: ");
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            int fn_w = dialog_w - 14;
            int fn_col = left_x + 11;

            if (focus_area == 0) {
                attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                for (int i = 0; i < fn_w; ++i) mvaddch(fn_y, fn_col + i, ' ');
                int sc = 0;
                if (fname_cursor_pos >= fn_w) sc = fname_cursor_pos - fn_w + 1;
                size_t sb = utf8_char_to_byte(fname_buf, sc);
                size_t eb = utf8_char_to_byte(fname_buf, sc + fn_w);
                mvprintw(fn_y, fn_col, "%.*s", (int)(eb - sb), fname_buf + sb);
                attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                cur_input_y = fn_y;
                cur_input_x = fn_col + (fname_cursor_pos - sc);
            } else {
                attron(COLOR_PAIR(CP_FIELD_INACTIVE));
                for (int i = 0; i < fn_w; ++i) mvaddch(fn_y, fn_col + i, ' ');
                mvprintw(fn_y, fn_col, "%-.*s", fn_w, fname_buf);
                attroff(COLOR_PAIR(CP_FIELD_INACTIVE));
            }

            list_top_y = top_y + 4;
            list_height = dialog_h - 6;
            attron(COLOR_PAIR(CP_BORDER));
            mvhline(top_y + 3, left_x + 1, ACS_HLINE, dialog_w - 2);
            attroff(COLOR_PAIR(CP_BORDER));
        }

        /* Lista över filer och mappar */
        int list_avail_rows = list_height;
        if (list_avail_rows < 1) list_avail_rows = 1;

        if (count > 0) {
            if (selected_pos < scroll_offset) {
                scroll_offset = selected_pos;
            } else if (selected_pos >= scroll_offset + list_avail_rows) {
                scroll_offset = selected_pos - list_avail_rows + 1;
            }
            if (scroll_offset < 0) scroll_offset = 0;
        } else {
            scroll_offset = 0;
        }

        for (int i = 0; i < list_avail_rows; ++i) {
            int idx = scroll_offset + i;
            int r = list_top_y + i;
            if (idx < (int)count) {
                const FileDialogItem *item = &items[idx];
                int is_sel = (focus_area == 1 && idx == selected_pos);

                if (is_sel) {
                    attron(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
                    mvhline(r, left_x + 1, ' ', dialog_w - 2);
                    mvaddch(r, left_x + 1, '>');
                } else {
                    attron(COLOR_PAIR(CP_LOG_ROW));
                    mvhline(r, left_x + 1, ' ', dialog_w - 2);
                }

                char sz_str[32] = "";
                if (!item->is_dir) {
                    format_file_size(item->size_bytes, sz_str, sizeof(sz_str));
                }

                int name_max = dialog_w - 18;
                if (item->is_dir) {
                    mvprintw(r, left_x + 3, "[%-.*s]", name_max, item->name);
                    mvprintw(r, left_x + dialog_w - 10, "<Mapp>");
                } else {
                    mvprintw(r, left_x + 3, "%-.*s", name_max, item->name);
                    mvprintw(r, left_x + dialog_w - 12, "%10s", sz_str);
                }

                if (is_sel) {
                    attroff(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
                    if (cur_input_y < 0) {
                        cur_input_y = r;
                        cur_input_x = left_x + 2;
                    }
                } else {
                    attroff(COLOR_PAIR(CP_LOG_ROW));
                }
            } else {
                if (count == 0 && i == 0) {
                    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
                    mvprintw(r, left_x + 3, "(Katalogen är tom eller matchar inte filtret)");
                    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);
                }
            }
        }

        /* Nedre linje och status/hjälp */
        int bot_y = top_y + dialog_h - 2;
        attron(COLOR_PAIR(CP_BORDER));
        mvhline(bot_y, left_x + 1, ACS_HLINE, dialog_w - 2);
        attroff(COLOR_PAIR(CP_BORDER));

        attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
        if (is_save_mode) {
            mvprintw(bot_y + 1, left_x + 2, "Tab/Pilar=Växla fält  Enter=Spara  Esc=Avbryt");
        } else {
            mvprintw(bot_y + 1, left_x + 2, "Pilar=Bläddra  Enter=Välj  Backspace=Uppåt  Esc=Avbryt");
        }
        if (ext_filter != NULL && ext_filter[0] != '\0') {
            int fw_col = left_x + dialog_w - (int)strlen(ext_filter) - 11;
            if (fw_col > left_x + 40) {
                attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                mvprintw(bot_y + 1, fw_col, "[Filter: %s]", ext_filter);
                attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
            }
        }
        attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

        if (cur_input_y >= 0 && cur_input_x >= 0) {
            move(cur_input_y, cur_input_x);
        }
        refresh();

        int ch = getch();
        if (ch == ERR) continue;

        if (ch == 27) {
            if (items) free(items);
            return 0;
        }

        if (is_save_mode && (ch == '\t' || (focus_area == 0 && (ch == KEY_DOWN)))) {
            focus_area = 1;
            continue;
        } else if (is_save_mode && (ch == KEY_BTAB || (focus_area == 1 && (ch == KEY_UP && selected_pos == 0)))) {
            focus_area = 0;
            continue;
        }

        if (focus_area == 0 && is_save_mode) {
            if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
                trim_str(fname_buf);
                if (fname_buf[0] != '\0') {
                    ensure_adif_extension(fname_buf, sizeof(fname_buf));
                    if (fname_buf[0] == '/') {
                        copy_str(selected_path, max_path_len, fname_buf);
                    } else if (strcmp(current_dir, "/") == 0) {
                        snprintf(selected_path, max_path_len, "/%s", fname_buf);
                    } else {
                        snprintf(selected_path, max_path_len, "%s/%s", current_dir, fname_buf);
                    }
                    if (items) free(items);
                    return 1;
                }
            } else if (ch == KEY_LEFT) {
                if (fname_cursor_pos > 0) fname_cursor_pos--;
            } else if (ch == KEY_RIGHT) {
                int total_c = utf8_display_width(fname_buf);
                if (fname_cursor_pos < total_c) fname_cursor_pos++;
            } else if (ch == KEY_HOME) {
                fname_cursor_pos = 0;
            } else if (ch == KEY_END) {
                fname_cursor_pos = utf8_display_width(fname_buf);
            } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
                utf8_backspace_at(fname_buf, &fname_cursor_pos);
            } else if (ch == KEY_DC) {
                utf8_delete_at(fname_buf, fname_cursor_pos);
            } else if (ch >= 32 && ch != 127 && ch < KEY_MIN) {
                utf8_insert_byte_at(fname_buf, sizeof(fname_buf), &fname_cursor_pos, (char)ch);
            }
        } else {
            /* Fillistans navigering */
            if (ch == KEY_UP) {
                if (selected_pos > 0) {
                    selected_pos--;
                } else if (is_save_mode) {
                    focus_area = 0;
                }
            } else if (ch == KEY_DOWN) {
                if (selected_pos + 1 < (int)count) {
                    selected_pos++;
                }
            } else if (ch == KEY_PPAGE) {
                selected_pos -= list_avail_rows;
                if (selected_pos < 0) selected_pos = 0;
            } else if (ch == KEY_NPAGE) {
                selected_pos += list_avail_rows;
                if (selected_pos >= (int)count) selected_pos = count > 0 ? (int)count - 1 : 0;
            } else if (ch == KEY_HOME) {
                selected_pos = 0;
            } else if (ch == KEY_END) {
                selected_pos = count > 0 ? (int)count - 1 : 0;
            } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
                /* Gå upp en katalog */
                char parent_dir[PATH_MAX + 16];
                if (strcmp(current_dir, "/") != 0) {
                    snprintf(parent_dir, sizeof(parent_dir), "%s/..", current_dir);
                    char resolved[PATH_MAX];
                    if (realpath(parent_dir, resolved) != NULL) {
                        copy_str(current_dir, sizeof(current_dir), resolved);
                        if (items) free(items);
                        items = NULL;
                        count = 0;
                        load_dir_items(current_dir, filter_active, ext_filter, &items, &count);
                        selected_pos = 0;
                        scroll_offset = 0;
                    }
                }
            } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
                if (count > 0 && selected_pos >= 0 && selected_pos < (int)count) {
                    const FileDialogItem *sel = &items[selected_pos];
                    if (sel->is_dir) {
                        char next_dir[PATH_MAX + 512];
                        if (strcmp(sel->name, "..") == 0) {
                            snprintf(next_dir, sizeof(next_dir), "%s/..", current_dir);
                        } else if (strcmp(current_dir, "/") == 0) {
                            snprintf(next_dir, sizeof(next_dir), "/%s", sel->name);
                        } else {
                            snprintf(next_dir, sizeof(next_dir), "%s/%s", current_dir, sel->name);
                        }
                        char resolved[PATH_MAX];
                        if (realpath(next_dir, resolved) != NULL) {
                            copy_str(current_dir, sizeof(current_dir), resolved);
                            if (items) free(items);
                            items = NULL;
                            count = 0;
                            load_dir_items(current_dir, filter_active, ext_filter, &items, &count);
                            selected_pos = 0;
                            scroll_offset = 0;
                        }
                    } else if (is_save_mode) {
                        copy_str(fname_buf, sizeof(fname_buf), sel->name);
                        fname_cursor_pos = utf8_display_width(fname_buf);
                        focus_area = 0;
                    } else {
                        if (strcmp(current_dir, "/") == 0) {
                            snprintf(selected_path, max_path_len, "/%s", sel->name);
                        } else {
                            snprintf(selected_path, max_path_len, "%s/%s", current_dir, sel->name);
                        }
                        if (items) free(items);
                        return 1;
                    }
                }
            }
        }
    }
}

/* F12: Inställningssida */
void run_settings_modal(StationSettings *st, QSO *qso, int editing_index,
                        QSOList *log_list, int *selected_log_pos, int *log_scroll_offset,
                        char *status_msg, size_t status_sz) {
    StationSettings edit = *st;
    int active = 0;
    int cursor_pos = utf8_display_width(edit.call);

    while (1) {
        int cur_y = 3, cur_x = 24;
        erase();

        if (COLS < MIN_COLS || LINES < MIN_LINES) {
            mvprintw(LINES / 2, 2, "Minsta fönsterstorlek är %dx%d.", MIN_COLS, MIN_LINES);
            refresh();
        } else {
            draw_top_bar(&edit);
            draw_box_with_title(1, 0, LINES - 2, COLS, "Inställningar – Min Station (F12)", 1);

            int colon_col = 22;
            int fw = COLS - colon_col - 5;
            if (fw > 32) fw = 32;
            if (fw < 12) fw = 12;

            draw_field_aligned(3,  colon_col, "Signal",        edit.call,       fw, active == 0, cursor_pos, &cur_y, &cur_x);
            draw_field_aligned(4,  colon_col, "Namn",          edit.name,       fw, active == 1, cursor_pos, &cur_y, &cur_x);
            draw_field_aligned(5,  colon_col, "QTH",           edit.qth,        fw, active == 2, cursor_pos, &cur_y, &cur_x);
            draw_field_aligned(6,  colon_col, "Lokator",       edit.gridsquare, fw, active == 3, cursor_pos, &cur_y, &cur_x);
            draw_field_aligned(7,  colon_col, "Lokal tidszon", edit.timezone,   fw, active == 4, cursor_pos, &cur_y, &cur_x);

            int r_theme  = 8;
            int r_smcb   = 9;
            int r_rst    = 10;
            int r_flt    = 11;
            int r_import = 12;
            int r_export = 13;
            int r_clear  = 14;

            /* Fält 5: Färgschema */
            const char *lbl_theme = "Färgschema";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_theme, colon_col - utf8_display_width(lbl_theme), "%s:", lbl_theme);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            int fcol = colon_col + 2;
            if (active == 5) {
                attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                mvprintw(r_theme, fcol, "< %-28s >", THEME_NAMES[edit.theme]);
                attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                cur_y = r_theme;
                cur_x = fcol + 2;
            } else {
                attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
                mvprintw(r_theme, fcol, "< %-28s >", THEME_NAMES[edit.theme]);
                attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
            }

            /* Fält 6: SM Callbook toggle */
            const char *lbl_smcb = "SM Callbook (ssa.se)";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_smcb, colon_col - utf8_display_width(lbl_smcb), "%s:", lbl_smcb);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            const char *smcb_txt = edit.smcb_enabled ? "På (Slå upp SA-SM, 7S, 8S)" : "Av";
            if (active == 6) {
                attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                mvprintw(r_smcb, fcol, "< %-28s >", smcb_txt);
                attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                cur_y = r_smcb;
                cur_x = fcol + 2;
            } else {
                attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
                mvprintw(r_smcb, fcol, "< %-28s >", smcb_txt);
                attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
            }

            /* Fält 7: Automatisk RST 59/59 vid nytt QSO */
            const char *lbl_rst = "Auto RST 59/59";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_rst, colon_col - utf8_display_width(lbl_rst), "%s:", lbl_rst);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            const char *rst_txt = edit.auto_rst_59 ? "På (Fyll i 59 / 59 vid nytt QSO)" : "Av";
            if (active == 7) {
                attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                mvprintw(r_rst, fcol, "< %-28s >", rst_txt);
                attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                cur_y = r_rst;
                cur_x = fcol + 2;
            } else {
                attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
                mvprintw(r_rst, fcol, "< %-28s >", rst_txt);
                attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
            }

            /* Fält 8: Filtrera loggrader efter anropssignal */
            const char *lbl_flt = "Filtrera loggrader";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_flt, colon_col - utf8_display_width(lbl_flt), "%s:", lbl_flt);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            const char *flt_txt = edit.filter_log ? "På" : "Av";
            if (active == 8) {
                attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                mvprintw(r_flt, fcol, "< %-28s >", flt_txt);
                attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                cur_y = r_flt;
                cur_x = fcol + 2;
            } else {
                attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
                mvprintw(r_flt, fcol, "< %-28s >", flt_txt);
                attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
            }

            /* Fält 9: Importera ADIF från fil (F5) */
            const char *lbl_import = "Importera ADIF (F5)";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_import, colon_col - utf8_display_width(lbl_import), "%s:", lbl_import);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            if (active == 9) {
                attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                mvprintw(r_import, fcol, "[ Tryck F5 eller Enter för att bläddra... ]");
                attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                cur_y = r_import;
                cur_x = fcol + 2;
            } else {
                attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
                mvprintw(r_import, fcol, "[ Tryck F5 eller Enter för att bläddra... ]");
                attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
            }

            /* Fält 10: Exportera ADIF till fil (F6) */
            int marked_count = 0;
            if (log_list != NULL) {
                for (size_t i = 0; i < log_list->count; ++i) {
                    if (log_list->items[i].marked) marked_count++;
                }
            }

            const char *lbl_export = "Exportera ADIF (F6)";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_export, colon_col - utf8_display_width(lbl_export), "%s:", lbl_export);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            char export_hint[128];
            if (marked_count > 0) {
                snprintf(export_hint, sizeof(export_hint), "[ F6 / Enter: Exportera %d markerade QSO ]", marked_count);
            } else if (log_list != NULL && log_list->count > 0) {
                int new_count = 0;
                for (size_t i = 0; i < log_list->count; ++i) {
                    if (log_list->items[i].handled == 0) new_count++;
                }
                if (new_count > 0 && new_count < (int)log_list->count) {
                    snprintf(export_hint, sizeof(export_hint), "[ F6 / Enter: Exportera (%d nya / %zu totalt) ]", new_count, log_list->count);
                } else if (new_count > 0) {
                    snprintf(export_hint, sizeof(export_hint), "[ F6 / Enter: Exportera alla (%zu nya QSO) ]", log_list->count);
                } else {
                    snprintf(export_hint, sizeof(export_hint), "[ F6 / Enter: Exportera alla (%zu QSO) ]", log_list->count);
                }
            } else {
                snprintf(export_hint, sizeof(export_hint), "[ F6 / Enter: Exportera alla (0 QSO) ]");
            }

            if (active == 10) {
                attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                mvprintw(r_export, fcol, "%-s", export_hint);
                attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                cur_y = r_export;
                cur_x = fcol + 2;
            } else {
                attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
                mvprintw(r_export, fcol, "%-s", export_hint);
                attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
            }

            /* Fält 11: Töm loggbok och börja på nytt */
            const char *lbl_clear = "Töm loggbok";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_clear, colon_col - utf8_display_width(lbl_clear), "%s:", lbl_clear);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            if (active == 11) {
                attron(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);
                mvprintw(r_clear, fcol, "[ Tryck Enter för att tömma ]");
                attroff(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);
                cur_y = r_clear;
                cur_x = fcol + 2;
            } else {
                attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
                mvprintw(r_clear, fcol, "[ Tryck Enter för att tömma ]");
                attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
            }

            if (status_msg != NULL && status_msg[0] != '\0' && LINES >= 19) {
                attron(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);
                mvprintw(LINES - 3, 3, "Status: %-s", status_msg);
                attroff(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);
            }

            if (LINES >= 17) {
                mvprintw(LINES - 2, 3, "Tab/Pilar byter fält. F5=Importera, F6=Exportera. Enter=Spara.");
            }

            draw_bottom_fkey_bar(1, FOCUS_INPUT, NULL);
            move(cur_y, cur_x);
            refresh();
        }

        int ch = getch();
        if (ch == ERR) {
            continue;
        } else if (ch == 27 || ch == KEY_F(12)) {
            apply_color_theme(st->theme);
            copy_str(status_msg, status_sz, "Inställningar stängda.");
            return;
        } else if (ch == KEY_F(5) || (active == 9 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' '))) {
            char chosen_path[PATH_MAX] = {0};
            int chosen = run_file_dialog("Välj ADIF-fil att importera", ".", ".adi;.adif", chosen_path, sizeof(chosen_path), 0, NULL);
            if (chosen && chosen_path[0] != '\0') {
                int added = import_qsos_from_adif(chosen_path, log_list, &edit);
                if (added < 0) {
                    snprintf(status_msg, status_sz, "Kunde inte läsa filen: %s", get_path_basename(chosen_path));
                } else if (added == 0) {
                    snprintf(status_msg, status_sz, "Import klar: Inga nya QSO hittades i %s.", get_path_basename(chosen_path));
                } else {
                    snprintf(status_msg, status_sz, "Importerade %d QSO från %s!", added, get_path_basename(chosen_path));
                    if (selected_log_pos && log_list && log_list->count > 0) {
                        *selected_log_pos = (int)log_list->count - 1;
                    }
                }
            } else {
                copy_str(status_msg, status_sz, "ADIF-import avbröts.");
            }
            apply_color_theme(edit.theme);
            continue;
        } else if (ch == KEY_F(6) || (active == 10 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' '))) {
            if (log_list == NULL || log_list->count == 0) {
                copy_str(status_msg, status_sz, "Loggen är tom – inget att exportera.");
                apply_color_theme(edit.theme);
                continue;
            }

            int m_cnt = 0;
            int m_new_cnt = 0;
            int new_cnt = 0;
            int handled_cnt = 0;
            for (size_t i = 0; i < log_list->count; ++i) {
                if (log_list->items[i].marked) {
                    m_cnt++;
                    if (log_list->items[i].handled == 0) m_new_cnt++;
                }
                if (log_list->items[i].handled == 0) new_cnt++;
                else handled_cnt++;
            }

            int exp_mode = EXPORT_MODE_ALL;
            int only_new = 0;
            char diag_title[128];

            if (m_cnt > 0) {
                exp_mode = EXPORT_MODE_MARKED;
                only_new = (m_new_cnt == m_cnt);
                snprintf(diag_title, sizeof(diag_title), "Exportera %d markerade QSO till ADIF", m_cnt);
            } else if (new_cnt > 0 && handled_cnt > 0) {
                int choice = 0;
                while (1) {
                    int box_h = 8, box_w = 48;
                    int by = (LINES - box_h) / 2;
                    int bx = (COLS - box_w) / 2;
                    attron(COLOR_PAIR(CP_MAIN_BG) | A_BOLD);
                    for (int r = 0; r < box_h; ++r) {
                        mvhline(by + r, bx, ' ', box_w);
                    }
                    draw_box_with_title(by, bx, box_h, box_w, "Exportera till ADIF", 1);
                    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
                    mvprintw(by + 2, bx + 4, "[N] Exportera endast nya  (%d QSO)", new_cnt);
                    mvprintw(by + 3, bx + 4, "[A] Exportera hela loggen (%zu QSO)", log_list->count);
                    mvprintw(by + 5, bx + 4, "[Esc] Avbryt");
                    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);
                    refresh();

                    int k = getch();
                    if (k == 'n' || k == 'N' || k == '\n' || k == '\r' || k == KEY_ENTER) {
                        choice = 1;
                        break;
                    } else if (k == 'a' || k == 'A' || k == 'h' || k == 'H') {
                        choice = 2;
                        break;
                    } else if (k == 27 || k == KEY_F(10) || k == KEY_F(12)) {
                        choice = 0;
                        break;
                    }
                }
                if (choice == 0) {
                    copy_str(status_msg, status_sz, "ADIF-export avbröts.");
                    apply_color_theme(edit.theme);
                    continue;
                }
                if (choice == 1) {
                    exp_mode = EXPORT_MODE_ONLY_NEW;
                    only_new = 1;
                    snprintf(diag_title, sizeof(diag_title), "Exportera %d nya QSO till ADIF", new_cnt);
                } else {
                    exp_mode = EXPORT_MODE_ALL;
                    only_new = 0;
                    snprintf(diag_title, sizeof(diag_title), "Exportera hela loggen (%zu QSO) till ADIF", log_list->count);
                }
            } else if (new_cnt > 0) {
                exp_mode = EXPORT_MODE_ALL;
                only_new = 1;
                snprintf(diag_title, sizeof(diag_title), "Exportera alla (%zu nya QSO) till ADIF", log_list->count);
            } else {
                exp_mode = EXPORT_MODE_ALL;
                only_new = 0;
                snprintf(diag_title, sizeof(diag_title), "Exportera hela loggen (%zu QSO) till ADIF", log_list->count);
            }

            char def_name[PATH_MAX];
            build_default_export_filename(def_name, sizeof(def_name), &edit, only_new);

            char chosen_path[PATH_MAX] = {0};
            int chosen = run_file_dialog(diag_title, ".", ".adi;.adif", chosen_path, sizeof(chosen_path), 1, def_name);
            if (chosen && chosen_path[0] != '\0') {
                int exported = export_qsos_adif(chosen_path, log_list, &edit, exp_mode);
                if (exported < 0) {
                    snprintf(status_msg, status_sz, "Kunde inte spara till filen: %s", get_path_basename(chosen_path));
                } else {
                    if (editing_index >= 0 && (size_t)editing_index < log_list->count) {
                        qso->handled = log_list->items[editing_index].handled;
                    }
                    if (exp_mode == EXPORT_MODE_ONLY_NEW) {
                        snprintf(status_msg, status_sz, "Exporterade %d nya QSO till %s!", exported, get_path_basename(chosen_path));
                    } else if (exp_mode == EXPORT_MODE_MARKED) {
                        snprintf(status_msg, status_sz, "Exporterade %d markerade QSO till %s!", exported, get_path_basename(chosen_path));
                    } else {
                        snprintf(status_msg, status_sz, "Exporterade %d QSO till %s!", exported, get_path_basename(chosen_path));
                    }
                }
            } else {
                copy_str(status_msg, status_sz, "ADIF-export avbröts.");
            }
            apply_color_theme(edit.theme);
            continue;
        } else if (active == 11 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' ')) {
            int confirm = 0;
            while (1) {
                int box_h = 7, box_w = 46;
                int by = (LINES - box_h) / 2;
                int bx = (COLS - box_w) / 2;
                attron(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);
                for (int r = 0; r < box_h; ++r) {
                    mvhline(by + r, bx, ' ', box_w);
                }
                mvprintw(by + 1, bx + 2, "VARNING: Tömma hela loggboken?");
                mvprintw(by + 2, bx + 2, "Alla QSO kommer att raderas permanent!");
                attroff(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);

                attron(COLOR_PAIR(CP_TOPBAR) | A_BOLD);
                mvprintw(by + 4, bx + 4, "[J] Ja, radera allt    [N/Esc] Avbryt");
                attroff(COLOR_PAIR(CP_TOPBAR) | A_BOLD);
                refresh();

                int cch = getch();
                if (cch == 'j' || cch == 'J') {
                    confirm = 1;
                    break;
                } else if (cch == 'n' || cch == 'N' || cch == 27 || cch == KEY_F(12)) {
                    confirm = 0;
                    break;
                }
            }
            if (confirm) {
                clear_log_book(log_list, st);
                if (selected_log_pos) *selected_log_pos = 0;
                if (log_scroll_offset) *log_scroll_offset = 0;
                apply_color_theme(st->theme);
                copy_str(status_msg, status_sz, "Loggboken har tömts!");
                return;
            } else {
                apply_color_theme(st->theme);
            }
        } else if (ch == KEY_F(2) || ch == 19 || (active < 9 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER))) {
            to_upper_str(edit.call);
            to_upper_str(edit.gridsquare);
            *st = edit;
            apply_color_theme(st->theme);
            save_station_settings(CONFIG_FILE, st);
            if (qso != NULL && editing_index < 0 && st->auto_rst_59) {
                if (qso->rst_sent[0] == '\0') copy_str(qso->rst_sent, sizeof(qso->rst_sent), "59");
                if (qso->rst_rcvd[0] == '\0') copy_str(qso->rst_rcvd, sizeof(qso->rst_rcvd), "59");
            }
            copy_str(status_msg, status_sz, "Stationsinställningar sparade!");
            return;
        } else if (ch == '\t' || ch == KEY_DOWN) {
            active = (active + 1) % STATION_FIELD_COUNT;
            char *p = (active == 0) ? edit.call : (active == 1) ? edit.name :
                      (active == 2) ? edit.qth  : (active == 3) ? edit.gridsquare :
                      (active == 4) ? edit.timezone : NULL;
            cursor_pos = p ? utf8_display_width(p) : 0;
        } else if (ch == KEY_BTAB || ch == KEY_UP) {
            active = (active + STATION_FIELD_COUNT - 1) % STATION_FIELD_COUNT;
            char *p = (active == 0) ? edit.call : (active == 1) ? edit.name :
                      (active == 2) ? edit.qth  : (active == 3) ? edit.gridsquare :
                      (active == 4) ? edit.timezone : NULL;
            cursor_pos = p ? utf8_display_width(p) : 0;
        } else if (active == 5 && ch == KEY_LEFT) {
            edit.theme = (edit.theme + THEME_COUNT - 1) % THEME_COUNT;
            apply_color_theme(edit.theme);
        } else if (active == 5 && (ch == KEY_RIGHT || ch == ' ')) {
            edit.theme = (edit.theme + 1) % THEME_COUNT;
            apply_color_theme(edit.theme);
        } else if (active == 6 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')) {
            edit.smcb_enabled = !edit.smcb_enabled;
        } else if (active == 7 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')) {
            edit.auto_rst_59 = !edit.auto_rst_59;
        } else if (active == 8 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')) {
            edit.filter_log = !edit.filter_log;
        } else if (active < 5 && ch == KEY_LEFT) {
            if (cursor_pos > 0) cursor_pos--;
        } else if (active < 5 && ch == KEY_RIGHT) {
            char *p = (active == 0) ? edit.call : (active == 1) ? edit.name :
                      (active == 2) ? edit.qth  : (active == 3) ? edit.gridsquare : edit.timezone;
            int max_c = utf8_display_width(p);
            if (cursor_pos < max_c) cursor_pos++;
        } else if (active < 5 && (ch == KEY_BACKSPACE || ch == 127 || ch == '\b')) {
            char *p = (active == 0) ? edit.call : (active == 1) ? edit.name :
                      (active == 2) ? edit.qth  : (active == 3) ? edit.gridsquare : edit.timezone;
            utf8_backspace_at(p, &cursor_pos);
        } else if (active < 5 && ch == KEY_DC) {
            char *p = (active == 0) ? edit.call : (active == 1) ? edit.name :
                      (active == 2) ? edit.qth  : (active == 3) ? edit.gridsquare : edit.timezone;
            utf8_delete_at(p, cursor_pos);
        } else if (active < 5 && ch >= 32 && ch != 127 && ch < KEY_MIN) {
            char *ptr = NULL;
            size_t cap = 0;
            switch (active) {
                case 0: ptr = edit.call;       cap = sizeof(edit.call); break;
                case 1: ptr = edit.name;       cap = sizeof(edit.name); break;
                case 2: ptr = edit.qth;        cap = sizeof(edit.qth); break;
                case 3: ptr = edit.gridsquare; cap = sizeof(edit.gridsquare); break;
                case 4: ptr = edit.timezone;   cap = sizeof(edit.timezone); break;
            }
            if (ptr) {
                char c = (char)ch;
                if ((active == 0 || active == 3) && (unsigned char)c < 128) {
                    c = (char)toupper((unsigned char)c);
                }
                utf8_insert_byte_at(ptr, cap, &cursor_pos, c);
            }
        }
    }
}

/* F3: Sök/filtrera i Loggrader (Signal, Namn, QTH, Lokator) */
void run_search_bar(const StationSettings *st, char *search_filter, size_t filter_sz) {
    int cpos = utf8_display_width(search_filter);
    while (1) {
        int cur_y = LINES - 1, cur_x = 35;
        draw_top_bar(st);
        mvhline(LINES - 1, 0, ' ', COLS);
        const char *prompt_lbl = (COLS >= 80) ? "Sök (Signal/Namn/QTH/Lokator)" : "Sök";
        int colon_x = (COLS >= 80) ? 33 : 6;
        draw_field_aligned(LINES - 1, colon_x, prompt_lbl,
                           search_filter, 18, 1, cpos, &cur_y, &cur_x);
        if (COLS >= 75) {
            attron(COLOR_PAIR(CP_FKEY_LABEL));
            mvprintw(LINES - 1, colon_x + 22, "[Enter=Klar, Tom=Alla]");
            attroff(COLOR_PAIR(CP_FKEY_LABEL));
        }
        move(cur_y, cur_x);
        refresh();

        int ch = getch();
        if (ch == ERR) continue;
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == 27 || ch == KEY_F(3)) {
            break;
        } else if (ch == KEY_LEFT) {
            if (cpos > 0) cpos--;
        } else if (ch == KEY_RIGHT) {
            int max_c = utf8_display_width(search_filter);
            if (cpos < max_c) cpos++;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            utf8_backspace_at(search_filter, &cpos);
        } else if (ch == KEY_DC) {
            utf8_delete_at(search_filter, cpos);
        } else if (ch >= 32 && ch != 127 && ch < KEY_MIN) {
            utf8_insert_byte_at(search_filter, filter_sz, &cpos, (char)ch);
        }
    }
}

/* Fälthantering och navigering */
char *get_qso_field_ptr(QSO *qso, int idx, size_t *max_len) {
    switch (idx) {
        case FLD_CALL: *max_len = sizeof(qso->call);       return qso->call;
        case FLD_NAME: *max_len = sizeof(qso->name);       return qso->name;
        case FLD_RSTS: *max_len = sizeof(qso->rst_sent);   return qso->rst_sent;
        case FLD_RSTR: *max_len = sizeof(qso->rst_rcvd);   return qso->rst_rcvd;
        case FLD_QTH:  *max_len = sizeof(qso->qth);        return qso->qth;
        case FLD_GRID: *max_len = sizeof(qso->gridsquare); return qso->gridsquare;
        case FLD_DXCC: *max_len = sizeof(qso->dxcc);       return qso->dxcc;
        case FLD_DATE: *max_len = sizeof(qso->date);       return qso->date;
        case FLD_TIME: *max_len = sizeof(qso->time_utc);   return qso->time_utc;
        case FLD_FREQ: *max_len = sizeof(qso->freq);       return qso->freq;
        case FLD_BAND: *max_len = sizeof(qso->band);       return qso->band;
        case FLD_MODE: *max_len = sizeof(qso->mode);       return qso->mode;
        case FLD_COMM: *max_len = sizeof(qso->comment);    return qso->comment;
        default:       *max_len = 0;                       return NULL;
    }
}

void sync_cursor_to_field_end(QSO *qso, int active_field, int *cursor_pos) {
    size_t max_len = 0;
    char *ptr = get_qso_field_ptr(qso, active_field, &max_len);
    *cursor_pos = ptr ? utf8_display_width(ptr) : 0;
}

int field_default_col(int fld) {
    switch (fld) {
        case FLD_CALL:
        case FLD_QTH:
        case FLD_DATE:
        case FLD_FREQ:
        case FLD_COMM:
            return 0;
        case FLD_NAME:
        case FLD_GRID:
        case FLD_TIME:
        case FLD_BAND:
            return 1;
        case FLD_RSTS:
        case FLD_DXCC:
        case FLD_MODE:
            return 2;
        case FLD_RSTR:
            return 3;
        default:
            return 0;
    }
}

int field_row_index(int fld) {
    switch (fld) {
        case FLD_CALL:
        case FLD_NAME:
        case FLD_RSTS:
        case FLD_RSTR:
            return 0;
        case FLD_QTH:
        case FLD_GRID:
        case FLD_DXCC:
            return 1;
        case FLD_DATE:
        case FLD_TIME:
            return 2;
        case FLD_FREQ:
        case FLD_BAND:
        case FLD_MODE:
            return 3;
        case FLD_COMM:
        default:
            return 4;
    }
}

int step_field_vertical(int current_field, int dir, int pref_col) {
    int r = field_row_index(current_field);
    int target_row = (r + dir + 5) % 5;

    switch (target_row) {
        case 0:
            if (pref_col == 0) return FLD_CALL;
            if (pref_col == 1) return FLD_NAME;
            if (pref_col == 2) return FLD_RSTS;
            return FLD_RSTR;
        case 1:
            if (pref_col == 0) return FLD_QTH;
            if (pref_col == 1) return FLD_GRID;
            return FLD_DXCC;
        case 2:
            if (pref_col == 0) return FLD_DATE;
            return FLD_TIME;
        case 3:
            if (pref_col == 0) return FLD_FREQ;
            if (pref_col == 1) return FLD_BAND;
            return FLD_MODE;
        case 4:
        default:
            return FLD_COMM;
    }
}

void reset_qso_form(QSO *qso, const StationSettings *st, int *time_frozen, int *active_field,
                    int *field_cursor_pos, int *editing_index, char *last_smcb_call,
                    char *current_qrb) {
    char saved_freq[32], saved_band[16], saved_mode[16];
    copy_str(saved_freq, sizeof(saved_freq), qso->freq);
    copy_str(saved_band, sizeof(saved_band), qso->band);
    copy_str(saved_mode, sizeof(saved_mode), qso->mode);

    memset(qso, 0, sizeof(*qso));
    *time_frozen = 0;
    *active_field = FLD_CALL;
    *field_cursor_pos = 0;
    *editing_index = -1;
    if (last_smcb_call) last_smcb_call[0] = '\0';
    if (current_qrb) copy_str(current_qrb, 32, "-");

    copy_str(qso->band, sizeof(qso->band), saved_band[0] ? saved_band : "20 m");
    copy_str(qso->freq, sizeof(qso->freq), saved_freq[0] ? saved_freq : "14.200");
    copy_str(qso->mode, sizeof(qso->mode), saved_mode);

    if (st != NULL && st->auto_rst_59) {
        copy_str(qso->rst_sent, sizeof(qso->rst_sent), "59");
        copy_str(qso->rst_rcvd, sizeof(qso->rst_rcvd), "59");
    }

    get_utc_strings(qso->date, sizeof(qso->date),
                    qso->time_utc, sizeof(qso->time_utc),
                    NULL, 0);
}
