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
    CP_SW_SEVERE,  /**< Magenta / Stark storm */
    CP_LOG_GREEN,
    CP_LOG_YELLOW,
    CP_LOG_RED,
    CP_LOG_GREEN_SEL,
    CP_LOG_YELLOW_SEL,
    CP_LOG_RED_SEL,
    CP_FLAG_NEW_GRN,
    CP_FLAG_HANDLED_GRN,
    CP_FLAG_NEW_YEL,
    CP_FLAG_HANDLED_YEL,
    CP_FLAG_NEW_RED,
    CP_FLAG_HANDLED_RED
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
 * @brief Sätter den aktiva egna färgprofilen för temat THEME_CUSTOM.
 * @param ct Pekare till CustomTheme-struktur.
 */
void set_custom_theme(const CustomTheme *ct);

/**
 * @brief Hämtar den aktiva egna färgprofilen.
 * @param ct Pekare till CustomTheme-struktur som fylls i.
 */
void get_custom_theme(CustomTheme *ct);

/**
 * @brief Beräknar hur många rader i terminalen som är tillgängliga för loggtabellen.
 * @param st Stationsinställningar (eller NULL för standard).
 * @return Antal synliga rader.
 */
int get_avail_log_rows(const StationSettings *st);

/**
 * @brief Struktur som beskriver den dynamiska kolumnlayouten för loggraderna baserat på fönsterbredd.
 */
typedef struct {
    int show_qso_num; /**< 1 om QSO-nummerkolumn (#) visas (COLS >= 90), 0 om den döljs (COLS < 90) */
    int col_date;     /**< X-position för datum (YYMMDD) */
    int col_time;     /**< X-position för tid (HHMM) */
    int col_call;     /**< X-position för anropssignal */
    int col_freq;     /**< X-position för frekvens */
    int col_band;     /**< X-position för band */
    int col_mode;     /**< X-position för mode */
    int col_rst;      /**< X-position för RST */
    int col_name;     /**< X-position för Namn */
    int w_name;       /**< Bredd för Namn-kolumn */
    int col_qth;      /**< X-position för QTH */
    int w_qth;        /**< Bredd för QTH-kolumn (min 10, max 30; döljs vid COLS < 80) */
    int col_qrb;      /**< X-position för QRB */
    int w_qrb;        /**< Bredd för QRB-kolumn (döljs före QTH vid platsbrist) */
    int col_quj;      /**< X-position för QUJ (bäring) */
    int w_quj;        /**< Bredd för QUJ-kolumn (döljs samtidigt som QRB) */
    int col_loc;      /**< X-position för Lokator */
    int w_loc;        /**< Bredd för Lokator-kolumn */
    int col_dxcc;     /**< X-position för DXCC */
    int w_dxcc;       /**< Bredd för DXCC-kolumn */
} LogColumnLayout;

/**
 * @brief Beräknar kolumnplaceringar och bredder för loggtabellen baserat på terminalens bredd.
 * QRB döljs före QTH vid minskande bredd, QTH kvarstår ned till 80 kolumner,
 * och QSO-nummerkolumnen tas bort vid < 90 kolumner.
 * @param total_cols Fönsterbredd (COLS).
 * @param layout Utdatapekare till LogColumnLayout-struktur.
 */
void calculate_log_column_layout(int total_cols, LogColumnLayout *layout);

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
 * @brief Ritar en centrerad förlopps-/statusruta i aktivt ncurses-fönster.
 * @param title Rutans titel.
 * @param message Meddelande som centreras inne i rutan.
 */
void draw_progress_modal(const char *title, const char *message);

/**
 * @brief Ritar den översta listen med programnamn, min stations signal/lokator, datum, lokal tid och UTC.
 * @param st Min stations inställningar.
 */
void draw_top_bar(const StationSettings *st);

/**
 * @brief Ritar den lilla statistikrutan ovanför QSO-inmatningen (2 rader data i ram).
 * Visar QSO, Stationer, DXCC, Lokatorer, Bandland och Rate (senaste 60 min).
 * @param top_y Översta raden för rutan.
 * @param left_x Vänster startkolumn.
 * @param width Rutans bredd.
 * @param log_list Pekare till logglistan.
 */
void draw_stats_box(int top_y, int left_x, int width, const QSOList *log_list);

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
 * @param show_cluster 1 om DX-klusterfönstret är aktivt.
 */
void draw_bottom_fkey_bar(int mode_type, FocusMode focus_mode, const char *search_filter, int show_cluster);

/**
 * @brief Ritar rymdvädersrutan med mätvärden och färgkoder från hf.avitec.se.
 * @param top_y Övre radposition.
 * @param left_x Vänster kolumnposition.
 * @param width Total tillgänglig bredd.
 */
void draw_space_weather_box(int top_y, int left_x, int width);

/**
 * @brief Ritar DX-klusterfönstret som en vertikal ruta till vänster i systemet.
 * @param top_y Övre radposition.
 * @param left_x Vänster kolumnposition (vanligen 0).
 * @param h Total höjd för rutan.
 * @param w Total bredd för rutan (vanligen 14 för 12 teckens innerbredd).
 * @param current_band Aktuellt inställt amatörband i loggen.
 * @param band_filter_active 1 om endast spots på inställt band ska visas, 0 för alla band.
 */
void draw_cluster_box(int top_y, int left_x, int h, int w,
                      const char *current_band, int band_filter_active,
                      const QSOList *log_list, int is_focused,
                      int selected_pos, int *scroll_offset);

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
 * @param quj_str Beräknad bäring.
 * @param selected_cluster_pos Vald rad i DX-klustret.
 * @param cluster_scroll_offset Rullningsposition i DX-klustret.
 * @param cur_y Utdatapekare för terminalmarkörens rad.
 * @param cur_x Utdatapekare för terminalmarkörens kolumn.
 */
void draw_main_screen(const StationSettings *st, const QSO *qso,
                      FocusMode focus_mode, int active_field, int band_selecting,
                      int field_cursor_pos, int time_frozen, int editing_index,
                      QSOList *log_list, int selected_log_pos, int *log_scroll_offset,
                      const char *search_filter, const char *status_msg,
                      const char *qrb_str, const char *quj_str,
                      int selected_cluster_pos, int *cluster_scroll_offset,
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
 * @param quj_str QUJ-sträng.
 * @param band_row Rad där bandfältet ligger.
 * @param colon_col Kolumn för kolon.
 * @return 1 vid val, 0 vid avbryt, 2 vid Tab, 3 vid Shift-Tab.
 */
int run_band_dropdown(StationSettings *st, QSO *qso,
                      FocusMode focus_mode, int active_field,
                      int field_cursor_pos, int *time_frozen, int editing_index,
                      QSOList *log_list, int selected_log_pos, int *log_scroll_offset,
                      const char *search_filter, char *status_msg, size_t status_sz,
                      const char *qrb_str, const char *quj_str, int band_row, int colon_col);

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
 * @param current_quj Buffer för QUJ.
 */
void reset_qso_form(QSO *qso, const StationSettings *st, int *time_frozen, int *active_field,
                    int *field_cursor_pos, int *editing_index, char *last_smcb_call,
                    char *current_qrb, char *current_quj);

#ifdef __cplusplus
}
#endif

#endif /* TUI_H */
