/**
 * @file tui.h
 * @brief Terminal User Interface (TUI) med ncurses, färgscheman, komponenter och dialoger.
 */

#ifndef TUI_H
#define TUI_H

#include "qsolog.h"
#include <ncurses.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Färgpar för ncurses TUI.
 */
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
    CP_SW_OK,      /**< Grön (optimalt / lugnt) */
    CP_SW_WARN,    /**< Gul (förhöjt / måttligt) */
    CP_SW_BAD,     /**< Röd (stört / hög dämpning) */
    CP_SW_SEVERE   /**< Magenta / Stark storm */
};

/**
 * @brief Initierar ncurses, sätter upp tangenter, färger och döljer standardmarkören.
 */
void tui_init(void);

/**
 * @brief Avslutar ncurses och återställer terminalens normala läge.
 */
void tui_cleanup(void);

/**
 * @brief Aktiverar ett av de definierade färgschemana i THEME_NAMES.
 * @param theme_idx Index för färgschemat (0..THEME_COUNT-1).
 */
void apply_color_theme(int theme_idx);

/**
 * @brief Beräknar hur många rader i terminalen som är tillgängliga för loggtabellen.
 * @param st Stationsinställningar (eller NULL för standard).
 * @return Antal synliga rader.
 */
int get_avail_log_rows(const StationSettings *st);

/**
 * @brief Bygger en array med index över loggrader som matchar ett visst sökfilter.
 * @param log_list Logglistan.
 * @param search_filter Söktext (eller NULL för alla rader).
 * @param indices_out Array där matchande index sparas.
 * @param max_out Maximal storlek på indices_out.
 * @return Antal matchande rader.
 */
int build_filtered_indices(const QSOList *log_list, const char *search_filter,
                           int *indices_out, int max_out);

/**
 * @brief Ritar en ncurses-ram med titel och anpassad färg beroende på fokus.
 * @param y Övre rad.
 * @param x Vänstra kolumn.
 * @param h Höjd i rader.
 * @param w Bredd i kolumner.
 * @param title Titeltext som centreras i överkanten.
 * @param has_focus 1 om ramen har aktivt fokus, annars 0.
 */
void draw_box_with_title(int y, int x, int h, int w, const char *title, int has_focus);

/**
 * @brief Ritar den översta listen med programnamn, min stations signal/lokator, datum, lokal tid och UTC.
 * @param st Min stations inställningar.
 */
void draw_top_bar(const StationSettings *st);

/**
 * @brief Ritar den lilla statistikrutan ovanför QSO-inmatningen (2 rader data i ram).
 * Visar QSO, Stationer, DXCC, Lokatorer, Bandland och Rate (senaste 60 min).
 * @param top_y Översta raden för rutan.
 * @param width Rutans bredd.
 * @param log_list Pekare till logglistan.
 */
void draw_stats_box(int top_y, int width, const QSOList *log_list);

/**
 * @brief Ritar ett högerjusterat inmatningsfält med etikett, värde och aktiv markör.
 * @param row Rad i fönstret.
 * @param colon_col Kolumn för kolonet efter fältetiketten.
 * @param label Etikettens text.
 * @param val Fältets aktuella textvärde.
 * @param width Fältets synliga bredd.
 * @param is_active 1 om fältet har aktivt fokus, annars 0.
 * @param cursor_pos Markörens teckenposition i fältet.
 * @param cur_y Utdatapekare för markörens y-position.
 * @param cur_x Utdatapekare för markörens x-position.
 */
void draw_field_aligned(int row, int colon_col, const char *label, const char *val,
                        int width, int is_active, int cursor_pos,
                        int *cur_y, int *cur_x);

/**
 * @brief Ritar bandväljarfältet.
 * @param row Rad i fönstret.
 * @param colon_col Kolumn för kolon efter etiketten.
 * @param val Aktuellt valt band.
 * @param is_active 1 om fältet har fokus.
 * @param band_selecting 1 om bandväljaren är i aktivt val-läge.
 * @param cur_y Utdatapekare för markör y.
 * @param cur_x Utdatapekare för markör x.
 */
void draw_band_selector_aligned(int row, int colon_col, const char *val,
                                int is_active, int band_selecting,
                                int *cur_y, int *cur_x);

/**
 * @brief Ritar nedersta funktionsknappsraden (F1..F12) beroende på skärm och fokus.
 * @param mode_type 0 för huvudskärmen, 1 för inställningsskärmen.
 * @param focus_mode FOCUS_INPUT eller FOCUS_LOG.
 * @param search_filter Aktivt sökfilter (eller NULL).
 */
void draw_bottom_fkey_bar(int mode_type, FocusMode focus_mode, const char *search_filter);

/**
 * @brief Ritar rymdvädersrutan med mätvärden och färgkoder från hf.avitec.se.
 * @param top_y Övre radposition.
 * @param width Total tillgänglig bredd.
 */
void draw_space_weather_box(int top_y, int width);

/**
 * @brief Ritar hela huvudskärmen (inmatningsformulär, rymdväder och loggtabell).
 * @param st Min stations inställningar.
 * @param qso Det aktiva QSO:t i formuläret.
 * @param focus_mode Aktivt fokus (inmatning eller logglista).
 * @param active_field Det för tillfället aktiva inmatningsfältet.
 * @param band_selecting 1 om bandval är aktivt.
 * @param field_cursor_pos Markörens position i aktivt fält.
 * @param time_frozen 1 om tiden är fryst för QSO:t.
 * @param editing_index Index för rad som redigeras (-1 vid nytt QSO).
 * @param log_list Logglistan.
 * @param selected_log_pos Vald rad i logglistan.
 * @param log_scroll_offset Rullningsposition i loggen.
 * @param search_filter Aktivt sökfilter.
 * @param status_msg Meddelande att visa i statuslisten.
 * @param qrb_str Beräknat avstånd.
 * @param cur_y Utdatapekare för terminalmarkörens rad.
 * @param cur_x Utdatapekare för terminalmarkörens kolumn.
 */
void draw_main_screen(const StationSettings *st, const QSO *qso,
                      FocusMode focus_mode, int active_field, int band_selecting,
                      int field_cursor_pos, int time_frozen, int editing_index,
                      const QSOList *log_list, int selected_log_pos, int *log_scroll_offset,
                      const char *search_filter, const char *status_msg,
                      const char *qrb_str,
                      int *cur_y, int *cur_x);

/**
 * @brief Visar och hanterar en sökdialog i fönstrets nederkant för att filtrera loggrader (F3).
 * @param st Min stations inställningar.
 * @param search_filter Sökfilterbuffer.
 * @param filter_sz Storlek på målbuffer.
 * @return 1 om sökningen bekräftades med Enter, 0 om den avbröts med Esc/F3.
 */
int run_search_bar(const StationSettings *st, char *search_filter, size_t filter_sz);

/**
 * @brief Visar en interaktiv modal drop-down meny för val av amatörband.
 * @param st Min stations inställningar.
 * @param qso Aktuellt QSO.
 * @param focus_mode Fokusläge.
 * @param active_field Aktivt fält.
 * @param field_cursor_pos Markörposition.
 * @param time_frozen Pekare till frystidsflagga.
 * @param editing_index Redigeringsindex.
 * @param log_list Logglistan.
 * @param selected_log_pos Vald loggrad.
 * @param log_scroll_offset Rullningsposition.
 * @param search_filter Sökfilter.
 * @param status_msg Statusmeddelande.
 * @param status_sz Storlek på statusbuffert.
 * @param qrb_str QRB-sträng.
 * @param band_row Rad där bandfältet ligger.
 * @param colon_col Kolumn för kolon.
 * @return 1 vid val, 0 vid avbryt, 2 vid Tab, 3 vid Shift-Tab.
 */
int run_band_dropdown(StationSettings *st, QSO *qso,
                      FocusMode focus_mode, int active_field,
                      int field_cursor_pos, int *time_frozen, int editing_index,
                      const QSOList *log_list, int selected_log_pos, int *log_scroll_offset,
                      const char *search_filter, char *status_msg, size_t status_sz,
                      const char *qrb_str, int band_row, int colon_col);

/**
 * @brief Hämtar pekare och maxlängd för det strängfält i QSO-strukturen som motsvarar ett fältindex.
 * @param qso Det aktuella QSO:t.
 * @param idx Fältindex (FLD_CALL, FLD_NAME, etc.).
 * @param max_len Utdatapekare för fältets maximala bufferstorlek.
 * @return Pekare till fältets textsträng, eller NULL vid ogiltigt index.
 */
char *get_qso_field_ptr(QSO *qso, int idx, size_t *max_len);

/**
 * @brief Synkroniserar markörpositionen till slutet av fältets aktuella text.
 * @param qso Det aktuella QSO:t.
 * @param active_field Fältindex.
 * @param cursor_pos Pekare till markörpositionen.
 */
void sync_cursor_to_field_end(QSO *qso, int active_field, int *cursor_pos);

/**
 * @brief Returnerar förvald visningskolumn (0..4) för ett inmatningsfält.
 * @param fld Fältindex.
 * @return Kolumnindex.
 */
int field_default_col(int fld);

/**
 * @brief Returnerar formulärrad (0..2) för ett inmatningsfält.
 * @param fld Fältindex.
 * @return Radindex.
 */
int field_row_index(int fld);

/**
 * @brief Beräknar nästa fältindex vid vertikal navigering med upp/ned-pilar.
 * @param current_field Aktuellt fält.
 * @param dir Riktning (-1 för uppåt, +1 för nedåt).
 * @param pref_col Föredragen kolumn.
 * @return Nytt fältindex.
 */
int step_field_vertical(int current_field, int dir, int pref_col);

/**
 * @brief Återställer formuläret inför inmatning av ett nytt QSO.
 * Fyller i senast använda frekvens, band, mode och sätter klockan i live-läge.
 * @param qso Det QSO som återställs.
 * @param st Min stations inställningar.
 * @param time_frozen Pekare till frystidsflaggan (sätts till 0).
 * @param active_field Pekare till aktivt fält (sätts till FLD_CALL).
 * @param field_cursor_pos Pekare till markörpositionen (sätts till 0).
 * @param editing_index Pekare till redigeringsindex (sätts till -1).
 * @param last_smcb_call Buffer för senast uppslagna signal.
 * @param current_qrb Buffer för QRB.
 */
void reset_qso_form(QSO *qso, const StationSettings *st, int *time_frozen, int *active_field,
                    int *field_cursor_pos, int *editing_index, char *last_smcb_call,
                    char *current_qrb);

#ifdef __cplusplus
}
#endif

#endif /* TUI_H */
