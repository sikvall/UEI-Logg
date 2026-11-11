#ifndef TUI_H
#define TUI_H

#include "qsolog.h"
#include <ncurses.h>

/**
 * @file tui.h
 * @brief Terminal User Interface (TUI) med ncurses, färgscheman, komponenter och hjälpskärmar.
 */

/* Färgpar för ncurses TUI */
enum {
    CP_MAIN_BG = 1,
    CP_TOPBAR,
    CP_TOPBAR_ACCENT,
    CP_BORDER,
    CP_BORDER_ACTIVE,
    CP_TITLE,
    CP_LABEL,
    CP_FIELD_INACTIVE,
    CP_FIELD_ACTIVE,
    CP_FROZEN_BADGE,
    CP_LIVE_BADGE,
    CP_EDIT_BADGE,
    CP_TABLE_HEADER,
    CP_LOG_ROW,
    CP_LOG_SELECTED,
    CP_LOG_MARKED,
    CP_FKEY_NUM,
    CP_FKEY_LABEL,
    CP_STATUS_OK,
    CP_STATUS_WARN,
    CP_FLAG_NEW,
    CP_FLAG_HANDLED,
    CP_FLAG_NEW_SEL,
    CP_FLAG_HANDLED_SEL,
    CP_FLAG_NEW_MRK,
    CP_FLAG_HANDLED_MRK,
    CP_SW_NORMAL,
    CP_SW_OK,      /* Grön (optimalt / lugnt) */
    CP_SW_WARN,    /* Gul (förhöjt / måttligt) */
    CP_SW_BAD,     /* Röd (stört / hög dämpning) */
    CP_SW_SEVERE   /* Magenta / Stark storm */
};

/* TUI-livscykel */
void tui_init(void);
void tui_cleanup(void);

/* Färgscheman */
void apply_color_theme(int theme_idx);

/* Ritytor och komponenter */
int get_avail_log_rows(void);
int build_filtered_indices(const QSOList *log_list, const char *search_filter,
                           int *indices_out, int max_out);
void draw_box_with_title(int y, int x, int h, int w, const char *title, int has_focus);
void draw_top_bar(const StationSettings *st);
void draw_field_aligned(int row, int colon_col, const char *label, const char *val,
                        int width, int is_active, int cursor_pos,
                        int *cur_y, int *cur_x);
void draw_band_selector_aligned(int row, int colon_col, const char *val,
                                int is_active, int band_selecting,
                                int *cur_y, int *cur_x);
void draw_band_popup_list(int row, const char *current_band, int band_selecting);
void draw_bottom_fkey_bar(int mode_type, FocusMode focus_mode, const char *search_filter);
void draw_space_weather_box(int top_y, int width);
void draw_main_screen(const StationSettings *st, const QSO *qso,
                      FocusMode focus_mode, int active_field, int band_selecting,
                      int field_cursor_pos, int time_frozen, int editing_index,
                      const QSOList *log_list, int selected_log_pos, int *log_scroll_offset,
                      const char *search_filter, const char *status_msg,
                      const char *qrb_str,
                      int *cur_y, int *cur_x);

/* Hjälpskärm och sökruta */
void run_help_modal(const StationSettings *st);
int run_search_bar(const StationSettings *st, char *search_filter, size_t filter_sz);

/* Navigering och fälthantering */
char *get_qso_field_ptr(QSO *qso, int idx, size_t *max_len);
void sync_cursor_to_field_end(QSO *qso, int active_field, int *cursor_pos);
int field_default_col(int fld);
int field_row_index(int fld);
int step_field_vertical(int current_field, int dir, int pref_col);
void reset_qso_form(QSO *qso, const StationSettings *st, int *time_frozen, int *active_field,
                    int *field_cursor_pos, int *editing_index, char *last_smcb_call,
                    char *current_qrb);

#endif /* TUI_H */
