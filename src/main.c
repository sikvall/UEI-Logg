/**
 * @file main.c
 * @brief Huvudprogram och händelseloop för qsolog – ett modernt TUI-loggprogram för amatörradio.
 */

#include "qsolog.h"
#include "db.h"
#include "callbook.h"
#include "dxcc.h"
#include "logg.h"
#include "qrb.h"
#include "qrz.h"
#include "settings.h"
#include "space_weather.h"
#include "tui.h"
#include "util.h"
#include "help.h"
#include <curl/curl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wchar.h>
#include <wctype.h>

/**
 * @brief Loggar ett sparat QSO vid index qso_idx direkt mot QRZ.COM Logbook API,
 * uppdaterar statusfältet under och efter sändning samt sätter handled = 1 vid OK
 * eller låter raden vara kvar som ny (handled = 0) vid fel.
 */
static int log_qso_to_qrz(QSOList *log_list, int qso_idx,
                          const StationSettings *station,
                          const QSO *current_qso, FocusMode focus_mode,
                          int active_field, int band_selecting,
                          int field_cursor_pos, int time_frozen, int editing_index,
                          int selected_log_pos, int *log_scroll_offset,
                          const char *active_filter,
                          char *status_msg, size_t status_sz,
                          const char *current_qrb, const char *current_quj) {
    if (!log_list || qso_idx < 0 || (size_t)qso_idx >= log_list->count) return 0;
    QSO *target = &log_list->items[qso_idx];

    snprintf(status_msg, status_sz, "Loggar %s (%s %s) till QRZ.COM...",
             target->call, target->band, target->mode);
    int cur_y = 2, cur_x = 16;
    draw_main_screen(station, current_qso, focus_mode, active_field, band_selecting,
                     field_cursor_pos, time_frozen, editing_index,
                     log_list, selected_log_pos, log_scroll_offset,
                     active_filter, status_msg, current_qrb, current_quj, &cur_y, &cur_x);
    refresh();

    char err_buf[192] = "";
    int ok = qrz_logbook_insert(station->qrz_api_key, target, station, err_buf, sizeof(err_buf));
    if (ok) {
        target->handled = 1;
        if (db_is_open() && target->id > 0) {
            db_set_handled(target->id, 1);
        } else if (!db_is_open()) {
            save_all_qsos_adif(LOG_FILE, log_list, station);
        }
        snprintf(status_msg, status_sz, "Loggade %s (%s %s) – QRZ.COM OK (✓)",
                 target->call, target->band, target->mode);
        return 1;
    } else {
        target->handled = 0;
        if (db_is_open() && target->id > 0) {
            db_set_handled(target->id, 0);
        } else if (!db_is_open()) {
            save_all_qsos_adif(LOG_FILE, log_list, station);
        }
        snprintf(status_msg, status_sz, "Fel: QRZ.COM (%s): %s (kvar som ny ✗)",
                 target->call, err_buf[0] ? err_buf : "Misslyckades");
        return 0;
    }
}

/**
 * @brief Programmets huvudfunktion: initierar delsystem, laddar data och driver händelseloopen.
 * @return 0 vid normal avslutning.
 */
int main(int argc, char *argv[]) {
    /* Behandla kommandoradsargument */
    for (int i = 1; i < argc; ++i) {
        if (strcmp(argv[i], "-l") == 0 || strcmp(argv[i], "--logfile") == 0) {
            if (i + 1 < argc) {
                copy_str(g_log_filename, sizeof(g_log_filename), argv[++i]);
            } else {
                fprintf(stderr, "Fel: %s kräver ett filnamn som argument.\n", argv[i]);
                return 1;
            }
        } else if (strncmp(argv[i], "--logfile=", 10) == 0) {
            copy_str(g_log_filename, sizeof(g_log_filename), argv[i] + 10);
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            printf("Användning: ueilog [alternativ]\n\n"
                   "Alternativ:\n"
                   "  -l, --logfile <filnamn>  Använd angiven databasfil som loggbok (default: qsolog.sq3)\n"
                   "  -h, --help               Visa denna hjälpinformation\n"
                   "  -v, --version            Visa versionsinformation\n");
            return 0;
        } else if (strcmp(argv[i], "-v") == 0 || strcmp(argv[i], "--version") == 0) {
            printf("UEI-log v1.0.0\n");
            return 0;
        } else {
            fprintf(stderr, "Okänt argument: %s\nKör 'ueilog --help' för information om tillåtna argument.\n", argv[i]);
            return 1;
        }
    }

    /* Sätt UTF-8 locale för korrekt teckenbredd och teckenuppsättning */
    setlocale(LC_ALL, "");

    /* Initiera globala nätverksresurser (libcurl) */
    curl_global_init(CURL_GLOBAL_DEFAULT);

    /* Läs in DXCC-tabellen från dxcc.txt vid programstart */
    load_dxcc_file(DXCC_FILE);

    /* Läs in min stations inställningar från station.conf */
    StationSettings station;
    load_station_settings(CONFIG_FILE, &station);

    /* Öppna eller skapa SQLite3-databasen för lokal lagring */
    db_open(g_log_filename);

    /* Om databasen är tom och qsolog.adi finns, migrera automatiskt från tidigare ADIF-logg */
    if (strcmp(g_log_filename, DEFAULT_LOG_FILE) == 0) {
        db_migrate_from_adif("qsolog.adi", &station);
    }

    /* Läs in loggboken från databasen (eller ADIF som fallback om DB inte öppnades) */
    QSOList log_list = {NULL, 0, 0};
    if (db_is_open()) {
        db_load_all_qsos(&log_list);
    } else {
        load_qsos_from_adif(LOG_FILE, &log_list);
    }

    /* Beräkna och spara QRB för eventuella poster som saknar detta */
    update_all_missing_qrbs(&station, &log_list);

    /* Starta asynkron inhämtning av rymdväder från https://hf.avitec.se */
    space_weather_init();

    /* Initiera terminalgränssnittet (ncurses) och aktivera valt tema */
    tui_init();
    apply_color_theme(station.theme);

    /* Tillstånd för aktiv inmatning */
    QSO current_qso;
    memset(&current_qso, 0, sizeof(current_qso));
    copy_str(current_qso.freq, sizeof(current_qso.freq), station.last_freq);
    copy_str(current_qso.band, sizeof(current_qso.band), station.last_band);
    copy_str(current_qso.mode, sizeof(current_qso.mode), station.last_mode);

    int time_frozen = 0;
    int active_field = FLD_CALL;
    int band_selecting = 0;
    int preferred_col = 0;
    int field_cursor_pos = 0;
    int field_fresh = 1;
    int editing_index = -1;
    int edit_from_log = 0;
    int idle_ticks_on_call = 0;
    int idle_ticks_on_qth = 0;
    int idle_ticks_on_grid = 0;
    char current_qrb[32] = "-";
    char current_quj[32] = "-";
    FocusMode focus_mode = FOCUS_INPUT;
    int selected_log_pos = 0;
    int log_scroll_offset = 0;
    char search_filter[32] = "";
    char last_smcb_call[32] = "";
    char status_msg[256] = "";
    if (strcmp(g_log_filename, DEFAULT_LOG_FILE) != 0) {
        snprintf(status_msg, sizeof(status_msg), "Loggfil: %.*s (%zu QSO inlästa).",
                 120, g_log_filename, log_list.count);
    }

    reset_qso_form(&current_qso, &station, &time_frozen, &active_field, &field_cursor_pos, &editing_index, last_smcb_call, current_qrb, current_quj);

    /* HUVUDLOOP */
    while (1) {
        /* Uppdatera kontinuerligt UTC-tid och datum om formuläret inte är fryst */
        if (time_frozen <= 0 && (editing_index < 0 || time_frozen == -1)) {
            get_utc_strings(current_qso.date, sizeof(current_qso.date),
                            current_qso.time_utc, sizeof(current_qso.time_utc),
                            NULL, 0);
        }

        /* Kontrollera om bakgrundssökningen i SM Callbook har blivit klar */
        poll_smcb_background_result(&current_qso, &station,
                                    active_field, &field_cursor_pos,
                                    status_msg, sizeof(status_msg),
                                    current_qrb, sizeof(current_qrb));

        /* Kontrollera om bakgrundssökningen i QRZ.COM har blivit klar */
        poll_qrz_background_result(&current_qso, &station,
                                   active_field, &field_cursor_pos,
                                   status_msg, sizeof(status_msg),
                                   current_qrb, sizeof(current_qrb));

        if (current_qso.qth[0] != '\0' || current_qso.gridsquare[0] != '\0') {
            if (current_quj[0] == '-' && current_qrb[0] != '-') {
                compute_quj_str(&station, &current_qso, current_quj, sizeof(current_quj));
            }
        }

        /* Rita huvudfönstret */
        int cur_y = 2, cur_x = 16;
        const char *active_filter = (search_filter[0] != '\0') ? search_filter :
                                    (station.filter_log && current_qso.call[0] != '\0') ? current_qso.call : NULL;
        if (focus_mode == FOCUS_LOG) {
            int max_f = (int)log_list.count > 0 ? (int)log_list.count : 1;
            int *tmp_f = malloc((size_t)max_f * sizeof(int));
            int f_cnt = tmp_f ? build_filtered_indices(&log_list, active_filter, tmp_f, max_f) : 0;
            free(tmp_f);
            if (f_cnt == 0) {
                focus_mode = FOCUS_INPUT;
            }
        }
        draw_main_screen(&station, &current_qso, focus_mode, active_field, band_selecting,
                         field_cursor_pos, time_frozen, editing_index,
                         &log_list, selected_log_pos, &log_scroll_offset,
                         active_filter, status_msg, current_qrb, current_quj, &cur_y, &cur_x);

        /* Läs in användartangent med timeout */
        wint_t wch = 0;
        int input_res = get_wch(&wch);
        int ch = (input_res == ERR) ? ERR : (int)wch;
        if (ch == ERR) {
            /* Paus i inmatning: utför automatiska uppslag när användaren stannar upp */
            if (focus_mode == FOCUS_INPUT && active_field == FLD_CALL && current_qso.call[0] != '\0') {
                idle_ticks_on_call++;
                if (idle_ticks_on_call == 2) {
                    maybe_lookup_call_details(&current_qso, &log_list, &station,
                                              last_smcb_call, sizeof(last_smcb_call),
                                              status_msg, sizeof(status_msg));
                    compute_qrb_quj_str(&station, &current_qso, current_qrb, sizeof(current_qrb), current_quj, sizeof(current_quj));
                }
            }
            if (focus_mode == FOCUS_INPUT && active_field == FLD_QTH && current_qso.qth[0] != '\0') {
                idle_ticks_on_qth++;
                if (idle_ticks_on_qth == 3) {
                    compute_qrb_quj_str(&station, &current_qso, current_qrb, sizeof(current_qrb), current_quj, sizeof(current_quj));
                }
            }
            if (focus_mode == FOCUS_INPUT && active_field == FLD_GRID && current_qso.gridsquare[0] != '\0') {
                idle_ticks_on_grid++;
                if (idle_ticks_on_grid == 3) {
                    if (is_valid_locator(current_qso.gridsquare)) {
                        compute_qrb_quj_str(&station, &current_qso, current_qrb, sizeof(current_qrb), current_quj, sizeof(current_quj));
                    }
                }
            }
            continue;
        }

        idle_ticks_on_call = 0;
        idle_ticks_on_qth = 0;
        idle_ticks_on_grid = 0;

        /* GLOBALA FUNKTIONSTANGENTER */
        if (ch == 27) {
            /* ESC: Återställ sökning om aktiv, annars lämna loggläge eller rensa formulär */
            if (search_filter[0] != '\0') {
                search_filter[0] = '\0';
                selected_log_pos = 0;
                log_scroll_offset = 0;
                copy_str(status_msg, sizeof(status_msg), "Sökning återställd – hela loggen visas.");
                continue;
            }
            if (focus_mode == FOCUS_LOG) {
                focus_mode = FOCUS_INPUT;
                field_cursor_pos = 0;
                field_fresh = 1;
                copy_str(status_msg, sizeof(status_msg), "Åter i inmatningsfälten.");
                continue;
            }
            focus_mode = FOCUS_INPUT;
            band_selecting = 0;
            edit_from_log = 0;
            reset_qso_form(&current_qso, &station, &time_frozen, &active_field, &field_cursor_pos, &editing_index, last_smcb_call, current_qrb, current_quj);
            field_fresh = 1;
            preferred_col = 0;
            copy_str(status_msg, sizeof(status_msg), "Formulär rensat – UTC-klockan tickar igen.");
            continue;
        } else if (ch == KEY_F(1)) {
            /* F1: Rensa formuläret och låt UTC-klockan ticka igen */
            focus_mode = FOCUS_INPUT;
            band_selecting = 0;
            edit_from_log = 0;
            reset_qso_form(&current_qso, &station, &time_frozen, &active_field, &field_cursor_pos, &editing_index, last_smcb_call, current_qrb, current_quj);
            field_fresh = 1;
            preferred_col = 0;
            copy_str(status_msg, sizeof(status_msg), "Formulär rensat – UTC-klockan tickar igen.");
            continue;
        } else if (ch == KEY_F(3) || ch == 6) {
            /* F3 eller Ctrl+F: Sök i logglistan */
            band_selecting = 0;
            int confirmed = run_search_bar(&station, search_filter, sizeof(search_filter));
            selected_log_pos = 0;
            log_scroll_offset = 0;
            if (confirmed) {
                if (search_filter[0] != '\0') {
                    int max_f = (int)log_list.count > 0 ? (int)log_list.count : 1;
                    int *tmp = malloc((size_t)max_f * sizeof(int));
                    int hits = tmp ? build_filtered_indices(&log_list, search_filter, tmp, max_f) : 0;
                    free(tmp);
                    snprintf(status_msg, sizeof(status_msg), "Sökning '%s': %d träffar. (Esc återställer)", search_filter, hits);
                } else {
                    copy_str(status_msg, sizeof(status_msg), "Sökning återställd – hela loggen visas.");
                }
            }
            continue;
        } else if (ch == KEY_F(4) || ch == 15) {
            /* F4 eller Ctrl+O: Växla fokus mellan inmatningsfält och loggrader */
            band_selecting = 0;
            const char *cur_filter = (search_filter[0] != '\0') ? search_filter :
                                     (station.filter_log && current_qso.call[0] != '\0') ? current_qso.call : NULL;
            if (focus_mode == FOCUS_INPUT) {
                int max_f = (int)log_list.count > 0 ? (int)log_list.count : 1;
                int *tmp = malloc((size_t)max_f * sizeof(int));
                int f_cnt = tmp ? build_filtered_indices(&log_list, cur_filter, tmp, max_f) : 0;
                free(tmp);

                if (f_cnt > 0) {
                    focus_mode = FOCUS_LOG;
                    if (selected_log_pos >= f_cnt) selected_log_pos = 0;
                    copy_str(status_msg, sizeof(status_msg), "Loggrader: w/s=Flytta, c=Färg, Ins=Märk, ^L=QRZ, F5=Hant/Ny, F6=Noter, F7=Alla, F8=Radera, Enter=Redigera.");
                } else {
                    copy_str(status_msg, sizeof(status_msg), "Inga loggrader att välja ännu.");
                }
            } else {
                focus_mode = FOCUS_INPUT;
                edit_from_log = 0;
                field_cursor_pos = 0;
                field_fresh = 1;
                copy_str(status_msg, sizeof(status_msg), "Åter i inmatningsfälten.");
            }
            continue;
        } else if (ch == KEY_F(6)) {
            /* F6: Växla visning av noteringar i loggraderna (på/av) */
            station.show_notes = !station.show_notes;
            save_station_settings(CONFIG_FILE, &station);
            snprintf(status_msg, sizeof(status_msg), "Noteringar i loggen: %s",
                     station.show_notes ? "PÅ" : "AV");
            continue;
        } else if (ch == KEY_F(9)) {
            /* F9: Visa tangentbordshjälp */
            run_help_modal(&station);
            continue;
        } else if (ch == KEY_F(10) || ch == 17) {
            /* F10 eller Ctrl+Q: Avsluta programmet */
            break;
        } else if (ch == KEY_F(12)) {
            /* F12: Inställningar, import och export */
            band_selecting = 0;
            int was_editing = (editing_index >= 0);
            run_settings_modal(&station, &current_qso, &editing_index, &log_list,
                               &selected_log_pos, &log_scroll_offset,
                               status_msg, sizeof(status_msg));
            if (was_editing && editing_index < 0) {
                reset_qso_form(&current_qso, &station, &time_frozen, &active_field,
                               &field_cursor_pos, &editing_index, last_smcb_call, current_qrb, current_quj);
                field_fresh = 1;
            }
            if (focus_mode == FOCUS_LOG && log_list.count == 0) {
                focus_mode = FOCUS_INPUT;
            }
            compute_qrb_quj_str(&station, &current_qso, current_qrb, sizeof(current_qrb), current_quj, sizeof(current_quj));
            update_all_missing_qrbs(&station, &log_list);
            if (station.show_space_weather) {
                space_weather_refresh_async();
            }
            continue;
        }

        /* FOKUS I LOGGRADSKOMPONENTEN (FOCUS_LOG) */
        if (focus_mode == FOCUS_LOG) {
            const char *cur_filter = (search_filter[0] != '\0') ? search_filter :
                                     (station.filter_log && current_qso.call[0] != '\0') ? current_qso.call : NULL;
            int max_f = (int)log_list.count > 0 ? (int)log_list.count : 1;
            int *f_idx = malloc((size_t)max_f * sizeof(int));
            int f_cnt = f_idx ? build_filtered_indices(&log_list, cur_filter, f_idx, max_f) : 0;

            if (ch == KEY_UP || ch == KEY_LEFT) {
                if (selected_log_pos > 0) selected_log_pos--;
            } else if (ch == KEY_DOWN || ch == KEY_RIGHT) {
                if (selected_log_pos + 1 < f_cnt) selected_log_pos++;
            } else if (ch == KEY_HOME || ch == KEY_SHOME || ch == KEY_A1 || ch == KEY_CTRL_HOME) {
                /* Home: gå längst upp i aktuell lista */
                selected_log_pos = 0;
                log_scroll_offset = 0;
            } else if (ch == KEY_END || ch == KEY_SEND || ch == KEY_LL || ch == KEY_C1 || ch == KEY_CTRL_END) {
                /* End: gå längst ned i aktuell lista */
                if (f_cnt > 0) {
                    selected_log_pos = f_cnt - 1;
                    int avail = get_avail_log_rows(&station);
                    int lines = 0;
                    int start_idx = f_cnt - 1;
                    for (int idx = f_cnt - 1; idx >= 0; --idx) {
                        int orig = f_idx[idx];
                        int qso_lines = (station.show_notes && log_list.items[orig].comment[0] != '\0') ? 2 : 1;
                        if (lines + qso_lines > avail && lines > 0) {
                            break;
                        }
                        lines += qso_lines;
                        start_idx = idx;
                    }
                    log_scroll_offset = start_idx;
                }
            } else if (ch == KEY_PPAGE) {
                /* PgUp: bläddra en sida uppåt */
                int avail = get_avail_log_rows(&station);
                selected_log_pos -= avail;
                if (selected_log_pos < 0) selected_log_pos = 0;
                log_scroll_offset -= avail;
                if (log_scroll_offset < 0) log_scroll_offset = 0;
            } else if (ch == KEY_NPAGE) {
                /* PgDown: bläddra en sida nedåt */
                int avail = get_avail_log_rows(&station);
                selected_log_pos += avail;
                if (selected_log_pos >= f_cnt) selected_log_pos = f_cnt > 0 ? f_cnt - 1 : 0;
                if (selected_log_pos < 0) selected_log_pos = 0;
                log_scroll_offset += avail;
                if (log_scroll_offset + avail > f_cnt) {
                    log_scroll_offset = f_cnt - avail;
                    if (log_scroll_offset < 0) log_scroll_offset = 0;
                }
            } else if (ch == 'w' || ch == 'W') {
                /* w: Flytta aktuell loggrad uppåt i listan */
                if (f_cnt > 1 && selected_log_pos > 0) {
                    int orig_curr = f_idx[selected_log_pos];
                    int orig_above = f_idx[selected_log_pos - 1];
                    QSO tmp = log_list.items[orig_curr];
                    log_list.items[orig_curr] = log_list.items[orig_above];
                    log_list.items[orig_above] = tmp;

                    if (editing_index == orig_curr) editing_index = orig_above;
                    else if (editing_index == orig_above) editing_index = orig_curr;

                    selected_log_pos--;
                    if (selected_log_pos < log_scroll_offset) {
                        log_scroll_offset = selected_log_pos;
                    }
                    if (!db_is_open()) {
                        save_all_qsos_adif(LOG_FILE, &log_list, &station);
                    }
                    snprintf(status_msg, sizeof(status_msg), "Flyttade QSO med %s uppåt.", log_list.items[orig_above].call);
                }
            } else if (ch == 's' || ch == 'S') {
                /* s: Flytta aktuell loggrad nedåt i listan */
                if (f_cnt > 1 && selected_log_pos + 1 < f_cnt) {
                    int orig_curr = f_idx[selected_log_pos];
                    int orig_below = f_idx[selected_log_pos + 1];
                    QSO tmp = log_list.items[orig_curr];
                    log_list.items[orig_curr] = log_list.items[orig_below];
                    log_list.items[orig_below] = tmp;

                    if (editing_index == orig_curr) editing_index = orig_below;
                    else if (editing_index == orig_below) editing_index = orig_curr;

                    selected_log_pos++;
                    int avail = get_avail_log_rows(&station);
                    if (selected_log_pos >= log_scroll_offset + avail) {
                        log_scroll_offset = selected_log_pos - avail + 1;
                    }
                    if (!db_is_open()) {
                        save_all_qsos_adif(LOG_FILE, &log_list, &station);
                    }
                    snprintf(status_msg, sizeof(status_msg), "Flyttade QSO med %s nedåt.", log_list.items[orig_below].call);
                }
            } else if (ch == 'c' || ch == 'C') {
                /* c: Cykla bakgrundsfärgen på aktuell loggrad: Standard -> Grön -> Gul -> Röd -> Standard */
                if (f_cnt > 0 && selected_log_pos >= 0 && selected_log_pos < f_cnt) {
                    int orig_idx = f_idx[selected_log_pos];
                    static const char * const color_names[] = {"Standard", "Grön", "Gul", "Röd"};
                    int m_cnt = 0;
                    for (size_t mi = 0; mi < log_list.count; ++mi) {
                        if (log_list.items[mi].marked) m_cnt++;
                    }

                    if (m_cnt > 0) {
                        int new_color = (log_list.items[orig_idx].color + 1) % LOG_COLOR_COUNT;
                        for (size_t mi = 0; mi < log_list.count; ++mi) {
                            if (log_list.items[mi].marked) {
                                log_list.items[mi].color = new_color;
                                if (db_is_open() && log_list.items[mi].id > 0) {
                                    db_set_color(log_list.items[mi].id, new_color);
                                }
                            }
                        }
                        if (editing_index >= 0 && (size_t)editing_index < log_list.count && log_list.items[editing_index].marked) {
                            current_qso.color = new_color;
                        }
                        if (!db_is_open()) {
                            save_all_qsos_adif(LOG_FILE, &log_list, &station);
                        }
                        snprintf(status_msg, sizeof(status_msg),
                                 "Bakgrundsfärg för %d markerade QSO: %s.", m_cnt, color_names[new_color]);
                    } else {
                        int new_color = (log_list.items[orig_idx].color + 1) % LOG_COLOR_COUNT;
                        log_list.items[orig_idx].color = new_color;
                        if (editing_index == orig_idx) {
                            current_qso.color = new_color;
                        }
                        if (db_is_open() && log_list.items[orig_idx].id > 0) {
                            db_set_color(log_list.items[orig_idx].id, new_color);
                        } else if (!db_is_open()) {
                            save_all_qsos_adif(LOG_FILE, &log_list, &station);
                        }
                        snprintf(status_msg, sizeof(status_msg),
                                 "Bakgrundsfärg för QSO med %s: %s.",
                                 log_list.items[orig_idx].call, color_names[new_color]);
                    }
                }
            } else if (ch == KEY_IC || ch == 331) {
                /* INS: Markera aktuell rad och flytta till nästa rad */
                if (f_cnt > 0 && selected_log_pos >= 0 && selected_log_pos < f_cnt) {
                    int orig_idx = f_idx[selected_log_pos];
                    log_list.items[orig_idx].marked = !log_list.items[orig_idx].marked;
                    if (selected_log_pos + 1 < f_cnt) {
                        selected_log_pos++;
                    }
                    int m_cnt = 0;
                    for (size_t mi = 0; mi < log_list.count; ++mi) {
                        if (log_list.items[mi].marked) m_cnt++;
                    }
                    snprintf(status_msg, sizeof(status_msg),
                             log_list.items[orig_idx].marked ?
                             "Markerade rad (QSO med %s) [%d markerade]" :
                             "Avmarkerade rad (QSO med %s) [%d markerade]",
                             log_list.items[orig_idx].call, m_cnt);
                }
            } else if (ch == KEY_F(5)) {
                /* F5: Växla status mellan hanterad (✓) och ny (✗) för markerade rader eller aktuell rad */
                int m_cnt = 0;
                for (size_t mi = 0; mi < log_list.count; ++mi) {
                    if (log_list.items[mi].marked) m_cnt++;
                }
                if (m_cnt > 0) {
                    int all_handled = 1;
                    for (size_t mi = 0; mi < log_list.count; ++mi) {
                        if (log_list.items[mi].marked && !log_list.items[mi].handled) {
                            all_handled = 0;
                            break;
                        }
                    }
                    int new_status = all_handled ? 0 : 1;
                    for (size_t mi = 0; mi < log_list.count; ++mi) {
                        if (log_list.items[mi].marked) {
                            log_list.items[mi].handled = new_status;
                            if (db_is_open() && log_list.items[mi].id > 0) {
                                db_set_handled(log_list.items[mi].id, new_status);
                            }
                        }
                    }
                    if (editing_index >= 0 && (size_t)editing_index < log_list.count && log_list.items[editing_index].marked) {
                        current_qso.handled = new_status;
                    }
                    if (!db_is_open()) {
                        save_all_qsos_adif(LOG_FILE, &log_list, &station);
                    }
                    snprintf(status_msg, sizeof(status_msg),
                             new_status ? "Markerade %d rader som hanterade (✓)." : "Markerade %d rader som nya (✗).", m_cnt);
                } else if (f_cnt > 0 && selected_log_pos >= 0 && selected_log_pos < f_cnt) {
                    int orig_idx = f_idx[selected_log_pos];
                    int new_status = !log_list.items[orig_idx].handled;
                    log_list.items[orig_idx].handled = new_status;
                    if (editing_index == orig_idx) {
                        current_qso.handled = new_status;
                    }
                    if (db_is_open() && log_list.items[orig_idx].id > 0) {
                        db_set_handled(log_list.items[orig_idx].id, new_status);
                    } else if (!db_is_open()) {
                        save_all_qsos_adif(LOG_FILE, &log_list, &station);
                    }
                    snprintf(status_msg, sizeof(status_msg),
                             new_status ? "Markerade QSO med %s som hanterat (✓)." : "Markerade QSO med %s som nytt (✗).",
                             log_list.items[orig_idx].call);
                }
            } else if (ch == KEY_F(7)) {
                /* F7: Markera ALLA loggrader som hanterade (✓) */
                if (log_list.count > 0) {
                    for (size_t mi = 0; mi < log_list.count; ++mi) {
                        log_list.items[mi].handled = 1;
                    }
                    if (editing_index >= 0 && (size_t)editing_index < log_list.count) {
                        current_qso.handled = 1;
                    }
                    invalidate_log_stats_cache();
                    if (db_is_open()) {
                        db_set_all_handled(1);
                    } else {
                        save_all_qsos_adif(LOG_FILE, &log_list, &station);
                    }
                    snprintf(status_msg, sizeof(status_msg), "Markerade alla %zu rader som hanterade (✓).", log_list.count);
                } else {
                    copy_str(status_msg, sizeof(status_msg), "Loggen är tom.");
                }
            } else if (ch == KEY_F(8)) {
                /* F8: Radera markerade rader eller aktuell rad */
                int m_cnt = 0;
                for (size_t mi = 0; mi < log_list.count; ++mi) {
                    if (log_list.items[mi].marked) m_cnt++;
                }
                if (m_cnt > 0) {
                    int edit_was_marked = 0;
                    int marked_before_edit = 0;
                    if (editing_index >= 0 && (size_t)editing_index < log_list.count) {
                        edit_was_marked = log_list.items[editing_index].marked;
                        for (size_t mi = 0; mi < (size_t)editing_index; ++mi) {
                            if (log_list.items[mi].marked) marked_before_edit++;
                        }
                    }
                    if (db_is_open()) {
                        db_delete_marked(&log_list);
                    }
                    int del_cnt = qso_list_delete_marked(&log_list);
                    if (!db_is_open()) {
                        save_all_qsos_adif(LOG_FILE, &log_list, &station);
                    }
                    if (editing_index >= 0) {
                        if (edit_was_marked) {
                            reset_qso_form(&current_qso, &station, &time_frozen, &active_field, &field_cursor_pos, &editing_index, last_smcb_call, current_qrb, current_quj);
                        } else {
                            editing_index -= marked_before_edit;
                        }
                    }
                    snprintf(status_msg, sizeof(status_msg), "Raderade %d markerade QSO:n.", del_cnt);
                } else if (f_cnt > 0 && selected_log_pos >= 0 && selected_log_pos < f_cnt) {
                    int orig_idx = f_idx[selected_log_pos];
                    char del_call[32];
                    copy_str(del_call, sizeof(del_call), log_list.items[orig_idx].call);
                    if (db_is_open() && log_list.items[orig_idx].id > 0) {
                        db_delete_qso(log_list.items[orig_idx].id);
                    }
                    qso_list_delete_at(&log_list, (size_t)orig_idx);
                    if (!db_is_open()) {
                        save_all_qsos_adif(LOG_FILE, &log_list, &station);
                    }
                    if (editing_index == orig_idx) {
                        reset_qso_form(&current_qso, &station, &time_frozen, &active_field, &field_cursor_pos, &editing_index, last_smcb_call, current_qrb, current_quj);
                    } else if (editing_index > orig_idx) {
                        editing_index--;
                    }
                    snprintf(status_msg, sizeof(status_msg), "Raderade QSO med %s.", del_call);
                }
                if (log_list.count == 0) {
                    selected_log_pos = 0;
                    log_scroll_offset = 0;
                    focus_mode = FOCUS_INPUT;
                    copy_str(status_msg, sizeof(status_msg), "Loggen är nu tom.");
                } else {
                    int max_f_new = (int)log_list.count;
                    int *tmp_f = malloc((size_t)max_f_new * sizeof(int));
                    int new_fcnt = tmp_f ? build_filtered_indices(&log_list, cur_filter, tmp_f, max_f_new) : 0;
                    if (tmp_f) free(tmp_f);
                    if (new_fcnt == 0) {
                        selected_log_pos = 0;
                        log_scroll_offset = 0;
                        focus_mode = FOCUS_INPUT;
                    } else if (selected_log_pos >= new_fcnt) {
                        selected_log_pos = new_fcnt - 1;
                    }
                }
            } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
                /* Enter: Ladda valt QSO till formuläret för redigering */
                if (f_cnt > 0 && selected_log_pos >= 0 && selected_log_pos < f_cnt) {
                    int orig_idx = f_idx[selected_log_pos];
                    current_qso = log_list.items[orig_idx];
                    current_qso.handled = 0;
                    editing_index = orig_idx;
                    edit_from_log = 1;
                    time_frozen = 1;
                    active_field = FLD_CALL;
                    band_selecting = 0;
                    preferred_col = 0;
                    field_cursor_pos = 0;
                    field_fresh = 1;
                    focus_mode = FOCUS_INPUT;
                    if (current_qso.dxcc[0] == '\0' && current_qso.call[0] != '\0') {
                        lookup_dxcc(current_qso.call, current_qso.dxcc, sizeof(current_qso.dxcc));
                    }
                    compute_qrb_quj_str(&station, &current_qso, current_qrb, sizeof(current_qrb), current_quj, sizeof(current_quj));
                    if (strcmp(current_qrb, "-") == 0 && current_qso.distance[0] != '\0') {
                        snprintf(current_qrb, sizeof(current_qrb), "%s km", current_qso.distance);
                    }
                    snprintf(status_msg, sizeof(status_msg),
                             "Redigerar QSO #%d (%s) – Enter sparar ändring.",
                             orig_idx + 1, current_qso.call);
                }
            } else if (ch == 12) {
                /* Ctrl+L: Logga markerade rader eller vald loggrad direkt mot QRZ.COM */
                if (station.qrz_log_enabled == QRZ_LOG_OFF) {
                    copy_str(status_msg, sizeof(status_msg), "Direktlogg mot QRZ.COM är avslagen i inställningarna (F12).");
                } else {
                    int m_cnt = 0;
                    for (size_t mi = 0; mi < log_list.count; ++mi) {
                        if (log_list.items[mi].marked) m_cnt++;
                    }
                    if (m_cnt > 0) {
                        int ok_cnt = 0;
                        int step = 0;
                        char last_err[192] = "";
                        for (size_t mi = 0; mi < log_list.count; ++mi) {
                            if (!log_list.items[mi].marked) continue;
                            step++;
                            snprintf(status_msg, sizeof(status_msg), "Loggar %d/%d (%s) till QRZ.COM...",
                                     step, m_cnt, log_list.items[mi].call);
                            draw_main_screen(&station, &current_qso, focus_mode, active_field, band_selecting,
                                             field_cursor_pos, time_frozen, editing_index,
                                             &log_list, selected_log_pos, &log_scroll_offset,
                                             cur_filter, status_msg, current_qrb, current_quj, &cur_y, &cur_x);
                            refresh();

                            char err_buf[192] = "";
                            int ok = qrz_logbook_insert(station.qrz_api_key, &log_list.items[mi], &station,
                                                        err_buf, sizeof(err_buf));
                            if (ok) {
                                log_list.items[mi].handled = 1;
                                if (db_is_open() && log_list.items[mi].id > 0) {
                                    db_set_handled(log_list.items[mi].id, 1);
                                }
                                ok_cnt++;
                            } else {
                                log_list.items[mi].handled = 0;
                                if (db_is_open() && log_list.items[mi].id > 0) {
                                    db_set_handled(log_list.items[mi].id, 0);
                                }
                                if (last_err[0] == '\0' && err_buf[0] != '\0') {
                                    copy_str(last_err, sizeof(last_err), err_buf);
                                }
                            }
                            if (editing_index == (int)mi) {
                                current_qso.handled = log_list.items[mi].handled;
                            }
                        }
                        if (!db_is_open()) {
                            save_all_qsos_adif(LOG_FILE, &log_list, &station);
                        }
                        if (ok_cnt == m_cnt) {
                            snprintf(status_msg, sizeof(status_msg),
                                     "QRZ.COM: Loggade %d markerade QSO OK (✓).", ok_cnt);
                        } else {
                            snprintf(status_msg, sizeof(status_msg),
                                     "Fel: QRZ.COM (%d/%d OK): %s",
                                     ok_cnt, m_cnt, last_err[0] ? last_err : "Misslyckades");
                        }
                    } else if (f_cnt > 0 && selected_log_pos >= 0 && selected_log_pos < f_cnt) {
                        int orig_idx = f_idx[selected_log_pos];
                        log_qso_to_qrz(&log_list, orig_idx, &station, &current_qso, focus_mode,
                                       active_field, band_selecting, field_cursor_pos, time_frozen,
                                       editing_index, selected_log_pos, &log_scroll_offset, cur_filter,
                                       status_msg, sizeof(status_msg), current_qrb, current_quj);
                        if (editing_index == orig_idx) {
                            current_qso.handled = log_list.items[orig_idx].handled;
                        }
                    }
                }
            }
            free(f_idx);
            continue;
        }

        /* FOKUS I INMATNINGSFÄLTEN (FOCUS_INPUT) */
        if (ch == KEY_F(2)) {
            /* F2: Starta UTC-klockan igen så att den kör live även om inmatning börjat */
            band_selecting = 0;
            time_frozen = -1;
            get_utc_strings(current_qso.date, sizeof(current_qso.date),
                            current_qso.time_utc, sizeof(current_qso.time_utc),
                            NULL, 0);
            if (active_field == FLD_DATE || active_field == FLD_TIME) {
                field_cursor_pos = 0;
                field_fresh = 1;
            }
            copy_str(status_msg, sizeof(status_msg), "UTC-klockan startad – tickar live.");
            continue;
        } else if (ch == KEY_F(5)) {
            /* F5: Manuellt uppslag (SMCB vid svensk signal & påslaget, annars QRZ.COM om på & inloggning ifylld) */
            band_selecting = 0;
            if (current_qso.call[0] == '\0') {
                copy_str(status_msg, sizeof(status_msg), "Ange en anropssignal först.");
                continue;
            }

            char base_call[32];
            int is_swedish = extract_swedish_base_call(current_qso.call, base_call, sizeof(base_call));
            int smcb_queried = 0;
            int smcb_blocked = 0;

            /* Om det är en svensk signal OCH SM Callbook är påslaget: slå på SM Callbook */
            if (is_swedish && station.smcb_enabled) {
                smcb_queried = 1;
                copy_str(status_msg, sizeof(status_msg), "Söker i SM Callbook...");
                draw_main_screen(&station, &current_qso, focus_mode, active_field, band_selecting,
                                 field_cursor_pos, time_frozen, editing_index,
                                 &log_list, selected_log_pos, &log_scroll_offset,
                                 active_filter, status_msg, current_qrb, current_quj, &cur_y, &cur_x);
                refresh();

                char sm_name[64] = "", sm_qth[64] = "";
                int sm_hit = lookup_sm_callbook(base_call, sm_name, sizeof(sm_name), sm_qth, sizeof(sm_qth));
                if (sm_hit > 0) {
                    /* Om det är träff så räcker det */
                    if (strcmp(sm_name, "-SK-") == 0) {
                        copy_str(current_qso.name, sizeof(current_qso.name), "-SK-");
                        current_qso.qth[0] = '\0';
                        copy_str(current_qso.comment, sizeof(current_qso.comment), "SILENT KEY");
                        store_smcb_cache(base_call, "-SK-", "", 1);
                        copy_str(current_qrb, sizeof(current_qrb), "-");
                        copy_str(current_quj, sizeof(current_quj), "-");
                        field_cursor_pos = 0;
                        copy_str(status_msg, sizeof(status_msg), "SMCB: SILENT KEY (-SK-)");
                        continue;
                    }
                    copy_str(current_qso.name, sizeof(current_qso.name), sm_name);
                    copy_str(current_qso.qth, sizeof(current_qso.qth), sm_qth);
                    store_smcb_cache(base_call, sm_name, sm_qth, 1);
                    compute_qrb_quj_str(&station, &current_qso, current_qrb, sizeof(current_qrb), current_quj, sizeof(current_quj));
                    field_cursor_pos = 0;
                    snprintf(status_msg, sizeof(status_msg), "SMCB: %s, %s", sm_name, sm_qth);
                    continue;
                }
                store_smcb_cache(base_call, "", "", sm_hit);
                smcb_blocked = (sm_hit == -1);
            }

            /* Om QRZ är aktiverat (Auto eller Manuell) och inloggningsuppgifter finns: prova QRZ.COM */
            int qrz_ready = (station.qrz_enabled != QRZ_MODE_OFF) &&
                            (station.qrz_username[0] != '\0') &&
                            (station.qrz_password[0] != '\0');

            if (qrz_ready) {
                if (smcb_queried) {
                    if (smcb_blocked) {
                        copy_str(status_msg, sizeof(status_msg), "SMCB blockerat -> Söker i QRZ.COM...");
                    } else {
                        copy_str(status_msg, sizeof(status_msg), "SMCB: Ingen träff -> Söker i QRZ.COM...");
                    }
                } else {
                    copy_str(status_msg, sizeof(status_msg), "Söker i QRZ.COM...");
                }
                draw_main_screen(&station, &current_qso, focus_mode, active_field, band_selecting,
                                 field_cursor_pos, time_frozen, editing_index,
                                 &log_list, selected_log_pos, &log_scroll_offset,
                                 active_filter, status_msg, current_qrb, current_quj, &cur_y, &cur_x);
                refresh();

                char qrz_name[64] = "", qrz_qth[64] = "", qrz_grid[16] = "";
                int qrz_hit = qrz_lookup(station.qrz_username, station.qrz_password, current_qso.call,
                                         qrz_name, sizeof(qrz_name), qrz_qth, sizeof(qrz_qth),
                                         qrz_grid, sizeof(qrz_grid));
                if (qrz_hit) {
                    if (strcmp(qrz_name, "-SK-") == 0) {
                        copy_str(current_qso.name, sizeof(current_qso.name), "-SK-");
                        current_qso.qth[0] = '\0';
                        current_qso.gridsquare[0] = '\0';
                        copy_str(current_qso.comment, sizeof(current_qso.comment), "SILENT KEY");
                        copy_str(current_qrb, sizeof(current_qrb), "-");
                        copy_str(current_quj, sizeof(current_quj), "-");
                        field_cursor_pos = 0;
                        copy_str(status_msg, sizeof(status_msg), "QRZ: SILENT KEY (-SK-)");
                        continue;
                    }
                    copy_str(current_qso.name, sizeof(current_qso.name), qrz_name);
                    copy_str(current_qso.qth, sizeof(current_qso.qth), qrz_qth);
                    if (qrz_grid[0] != '\0') {
                        copy_str(current_qso.gridsquare, sizeof(current_qso.gridsquare), qrz_grid);
                    }
                    if (current_qso.dxcc[0] == '\0') {
                        lookup_dxcc(current_qso.call, current_qso.dxcc, sizeof(current_qso.dxcc));
                    }
                    compute_qrb_quj_str(&station, &current_qso, current_qrb, sizeof(current_qrb), current_quj, sizeof(current_quj));
                    field_cursor_pos = 0;
                    if (qrz_grid[0] != '\0') {
                        snprintf(status_msg, sizeof(status_msg), "QRZ: %s, %s (%s)", qrz_name, qrz_qth, qrz_grid);
                    } else {
                        snprintf(status_msg, sizeof(status_msg), "QRZ: %s, %s", qrz_name, qrz_qth);
                    }
                } else {
                    if (smcb_queried) {
                        if (smcb_blocked) {
                            snprintf(status_msg, sizeof(status_msg), "SMCB blockerat av SSA. QRZ: Ingen träff för %s", current_qso.call);
                        } else {
                            snprintf(status_msg, sizeof(status_msg), "Ingen träff för %s i SMCB eller QRZ.COM", current_qso.call);
                        }
                    } else {
                        snprintf(status_msg, sizeof(status_msg), "QRZ.COM: Ingen träff för %s", current_qso.call);
                    }
                }
            } else {
                /* QRZ.COM kunde inte sökas (helt avslagen eller uppgifter saknas) */
                if (station.qrz_enabled == QRZ_MODE_OFF) {
                    copy_str(status_msg, sizeof(status_msg), "QRZ.COM är helt avslaget i inställningarna (F12).");
                } else if (station.qrz_username[0] == '\0' || station.qrz_password[0] == '\0') {
                    copy_str(status_msg, sizeof(status_msg), "QRZ.COM användarnamn/lösen saknas (F12).");
                }
            }
            continue;
        } else if (ch == KEY_F(7)) {
            /* F7 i inmatningsfältet: Ny beräkning av QRB & QUJ för angiven lokator (föredras) och om den saknas i stället QTH */
            band_selecting = 0;
            trim_str(current_qso.gridsquare);
            to_upper_str(current_qso.gridsquare);
            trim_str(current_qso.qth);

            if (is_valid_locator(current_qso.gridsquare)) {
                compute_qrb_quj_str(&station, &current_qso, current_qrb, sizeof(current_qrb), current_quj, sizeof(current_quj));
                if (current_qrb[0] != '\0' && strcmp(current_qrb, "-") != 0) {
                    snprintf(current_qso.distance, sizeof(current_qso.distance), "%ld", atol(current_qrb));
                    snprintf(status_msg, sizeof(status_msg), "QRB/QUJ beräknat från lokator (%s): %s, %s",
                             current_qso.gridsquare, current_qrb, current_quj);
                } else {
                    current_qso.distance[0] = '\0';
                    copy_str(status_msg, sizeof(status_msg), "Fel: Kan ej beräkna QRB/QUJ (kontrollera egen lokator/QTH i F12).");
                }
            } else if (current_qso.qth[0] != '\0') {
                invalidate_qth_cache(current_qso.qth);
                copy_str(status_msg, sizeof(status_msg), "Beräknar QRB/QUJ från QTH...");
                draw_main_screen(&station, &current_qso, focus_mode, active_field, band_selecting,
                                 field_cursor_pos, time_frozen, editing_index,
                                 &log_list, selected_log_pos, &log_scroll_offset,
                                 active_filter, status_msg, current_qrb, current_quj, &cur_y, &cur_x);
                refresh();

                compute_qrb_quj_str(&station, &current_qso, current_qrb, sizeof(current_qrb), current_quj, sizeof(current_quj));
                if (current_qrb[0] != '\0' && strcmp(current_qrb, "-") != 0) {
                    snprintf(current_qso.distance, sizeof(current_qso.distance), "%ld", atol(current_qrb));
                    snprintf(status_msg, sizeof(status_msg), "QRB/QUJ beräknat från QTH (%s): %s, %s",
                             current_qso.qth, current_qrb, current_quj);
                } else {
                    current_qso.distance[0] = '\0';
                    snprintf(status_msg, sizeof(status_msg), "Fel: Kunde inte beräkna QRB/QUJ för QTH '%s'.",
                             current_qso.qth);
                }
            } else if (current_qso.gridsquare[0] != '\0') {
                copy_str(current_qrb, sizeof(current_qrb), "-");
                copy_str(current_quj, sizeof(current_quj), "-");
                current_qso.distance[0] = '\0';
                snprintf(status_msg, sizeof(status_msg), "Fel: Ogiltig lokator '%s' (och QTH saknas).",
                         current_qso.gridsquare);
            } else {
                copy_str(current_qrb, sizeof(current_qrb), "-");
                copy_str(current_quj, sizeof(current_quj), "-");
                current_qso.distance[0] = '\0';
                copy_str(status_msg, sizeof(status_msg), "Ange lokator eller QTH först för att beräkna QRB och bäring.");
            }
            continue;
        } else if (active_field == FLD_BAND && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' ' || (ch >= '0' && ch <= '9'))) {
            if (ch >= '0' && ch <= '9') {
                for (int b = 0; b < BAND_OPTION_COUNT; ++b) {
                    if (BAND_OPTIONS[b][0] == (char)ch) {
                        copy_str(current_qso.band, sizeof(current_qso.band), BAND_OPTIONS[b]);
                        break;
                    }
                }
            }
            int stat_h = (station.show_stats && LINES >= 30) ? 4 : 0;
            int band_row = 1 + stat_h + 3;
            int band_col = (COLS >= 95) ? 33 : (COLS >= 85 ? 31 : 29);
            int ret = run_band_dropdown(&station, &current_qso, focus_mode, active_field,
                                        field_cursor_pos, &time_frozen, editing_index,
                                        &log_list, selected_log_pos, &log_scroll_offset,
                                        active_filter, status_msg, sizeof(status_msg),
                                        current_qrb, current_quj, band_row, band_col);
            if (ret == 2) {
                active_field = (active_field + 1) % QSO_FIELD_COUNT;
                preferred_col = field_default_col(active_field);
                field_cursor_pos = 0;
                field_fresh = 1;
            } else if (ret == 3) {
                active_field = (active_field + QSO_FIELD_COUNT - 1) % QSO_FIELD_COUNT;
                preferred_col = field_default_col(active_field);
                field_cursor_pos = 0;
                field_fresh = 1;
            }
            continue;
        } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == 12) {
            /* Enter loggar lokalt (och mot QRZ vid Automatisk); Ctrl+L (ch == 12) loggar lokalt + mot QRZ */
            band_selecting = 0;
            if (ch == 12 && station.qrz_log_enabled == QRZ_LOG_OFF) {
                copy_str(status_msg, sizeof(status_msg), "Direktlogg mot QRZ.COM är avslagen i inställningarna (F12).");
                continue;
            }
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg),
                                          current_qrb, sizeof(current_qrb),
                                          current_quj, sizeof(current_quj));
            int prev_editing_index = editing_index;
            size_t count_before = log_list.count;
            try_save_current_qso(&current_qso, &time_frozen, &active_field,
                                 &field_cursor_pos, &editing_index,
                                 &station, &log_list, last_smcb_call, status_msg, sizeof(status_msg),
                                 current_qrb, current_quj);
            int saved_idx = -1;
            if (prev_editing_index >= 0 && editing_index < 0 && (size_t)prev_editing_index < log_list.count) {
                saved_idx = prev_editing_index;
                if (edit_from_log) {
                    focus_mode = FOCUS_LOG;
                    edit_from_log = 0;
                }
            } else if (log_list.count > count_before) {
                saved_idx = (int)log_list.count - 1;
                selected_log_pos = 0;
                log_scroll_offset = 0;
            }
            if (saved_idx >= 0 && (ch == 12 || station.qrz_log_enabled == QRZ_LOG_AUTO)) {
                log_qso_to_qrz(&log_list, saved_idx, &station, &current_qso, focus_mode,
                               active_field, band_selecting, field_cursor_pos, time_frozen,
                               editing_index, selected_log_pos, &log_scroll_offset, active_filter,
                               status_msg, sizeof(status_msg), current_qrb, current_quj);
            }
            preferred_col = field_default_col(active_field);
            field_fresh = 1;
        } else if (ch == '\t' || ch == KEY_RIGHT) {
            /* Tab / Högerpil stegar till nästa inmatningsfält */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg),
                                          current_qrb, sizeof(current_qrb),
                                          current_quj, sizeof(current_quj));
            active_field = (active_field + 1) % QSO_FIELD_COUNT;
            preferred_col = field_default_col(active_field);
            field_cursor_pos = 0;
            field_fresh = 1;
        } else if (ch == KEY_BTAB || ch == KEY_LEFT) {
            /* Shift-Tab / Vänsterpil stegar till föregående fält */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg),
                                          current_qrb, sizeof(current_qrb),
                                          current_quj, sizeof(current_quj));
            active_field = (active_field + QSO_FIELD_COUNT - 1) % QSO_FIELD_COUNT;
            preferred_col = field_default_col(active_field);
            field_cursor_pos = 0;
            field_fresh = 1;
        } else if (ch == KEY_DOWN) {
            /* Pil ned går till fältet rakt under */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg),
                                          current_qrb, sizeof(current_qrb),
                                          current_quj, sizeof(current_quj));
            preferred_col = field_default_col(active_field);
            active_field = step_field_vertical(active_field, +1, preferred_col);
            field_cursor_pos = 0;
            field_fresh = 1;
        } else if (ch == KEY_UP) {
            /* Pil upp går till fältet rakt ovanför */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg),
                                          current_qrb, sizeof(current_qrb),
                                          current_quj, sizeof(current_quj));
            preferred_col = field_default_col(active_field);
            active_field = step_field_vertical(active_field, -1, preferred_col);
            field_cursor_pos = 0;
            field_fresh = 1;
        } else if (ch == KEY_HOME || ch == KEY_SHOME || ch == KEY_A1 || ch == KEY_CTRL_HOME) {
            /* Home: hoppa till Signal (FLD_CALL) */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg),
                                          current_qrb, sizeof(current_qrb),
                                          current_quj, sizeof(current_quj));
            active_field = FLD_CALL;
            preferred_col = 0;
            field_cursor_pos = 0;
            field_fresh = 1;
        } else if (ch == KEY_END || ch == KEY_SEND || ch == KEY_LL || ch == KEY_C1 || ch == KEY_CTRL_END) {
            /* End: hoppa till Not (FLD_COMM) */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg),
                                          current_qrb, sizeof(current_qrb),
                                          current_quj, sizeof(current_quj));
            active_field = FLD_COMM;
            preferred_col = 4;
            field_cursor_pos = 0;
            field_fresh = 1;
        } else if (ch == KEY_PPAGE) {
            /* PgUp: bläddra loggraderna uppåt */
            int avail = get_avail_log_rows(&station);
            log_scroll_offset -= avail;
            if (log_scroll_offset < 0) log_scroll_offset = 0;
            selected_log_pos -= avail;
            if (selected_log_pos < 0) selected_log_pos = 0;
        } else if (ch == KEY_NPAGE) {
            /* PgDown: bläddra loggraderna nedåt */
            const char *cur_filter = (search_filter[0] != '\0') ? search_filter :
                                     (station.filter_log && current_qso.call[0] != '\0') ? current_qso.call : NULL;
            int max_entries = (int)log_list.count > 0 ? (int)log_list.count : 1;
            int *tmp_f = malloc((size_t)max_entries * sizeof(int));
            int max_f = tmp_f ? build_filtered_indices(&log_list, cur_filter, tmp_f, max_entries) : 0;
            if (tmp_f) free(tmp_f);

            int avail = get_avail_log_rows(&station);
            log_scroll_offset += avail;
            if (log_scroll_offset + avail > max_f) {
                log_scroll_offset = max_f - avail;
                if (log_scroll_offset < 0) log_scroll_offset = 0;
            }
            selected_log_pos += avail;
            if (selected_log_pos >= max_f) selected_log_pos = max_f > 0 ? max_f - 1 : 0;
        } else if (active_field != FLD_BAND && (ch == KEY_BACKSPACE || ch == 127 || ch == '\b')) {
            size_t max_len = 0;
            char *field_ptr = get_qso_field_ptr(&current_qso, active_field, &max_len);
            if (field_ptr != NULL) {
                field_fresh = 0;
                utf8_backspace_at(field_ptr, &field_cursor_pos);
                if (active_field == FLD_CALL) {
                    last_smcb_call[0] = '\0';
                    lookup_dxcc(current_qso.call, current_qso.dxcc, sizeof(current_qso.dxcc));
                }
                if (active_field == FLD_FREQ && current_qso.freq[0] != '\0') {
                    resolve_band_from_freq(current_qso.freq, current_qso.band, sizeof(current_qso.band));
                }
                if (active_field == FLD_GRID) {
                    if (current_qso.gridsquare[0] == '\0') {
                        if (current_qso.qth[0] != '\0') {
                            compute_qrb_quj_str(&station, &current_qso, current_qrb, sizeof(current_qrb), current_quj, sizeof(current_quj));
                        } else {
                            copy_str(current_qrb, sizeof(current_qrb), "-");
                            copy_str(current_quj, sizeof(current_quj), "-");
                        }
                    } else if (is_valid_locator(current_qso.gridsquare) == 6) {
                        compute_qrb_quj_str(&station, &current_qso, current_qrb, sizeof(current_qrb), current_quj, sizeof(current_quj));
                    } else {
                        copy_str(current_qrb, sizeof(current_qrb), "-");
                        copy_str(current_quj, sizeof(current_quj), "-");
                    }
                }
                if (active_field == FLD_QTH) {
                    if (current_qso.qth[0] == '\0') {
                        if (is_valid_locator(current_qso.gridsquare)) {
                            compute_qrb_quj_str(&station, &current_qso, current_qrb, sizeof(current_qrb), current_quj, sizeof(current_quj));
                        } else {
                            copy_str(current_qrb, sizeof(current_qrb), "-");
                            copy_str(current_quj, sizeof(current_quj), "-");
                        }
                    }
                }
            }
        } else if (active_field != FLD_BAND && ch == KEY_DC) {
            /* Trycka DEL i ett inmatningsfält ska rensa hela fältet */
            size_t max_len = 0;
            char *field_ptr = get_qso_field_ptr(&current_qso, active_field, &max_len);
            if (field_ptr != NULL) {
                field_ptr[0] = '\0';
                field_cursor_pos = 0;
                field_fresh = 0;
                if (active_field == FLD_CALL) {
                    last_smcb_call[0] = '\0';
                    lookup_dxcc(current_qso.call, current_qso.dxcc, sizeof(current_qso.dxcc));
                    copy_str(current_qrb, sizeof(current_qrb), "-");
                    copy_str(current_quj, sizeof(current_quj), "-");
                }
                if (active_field == FLD_FREQ && current_qso.freq[0] != '\0') {
                    resolve_band_from_freq(current_qso.freq, current_qso.band, sizeof(current_qso.band));
                }
                if (active_field == FLD_GRID) {
                    if (current_qso.gridsquare[0] == '\0') {
                        if (current_qso.qth[0] != '\0') {
                            compute_qrb_quj_str(&station, &current_qso, current_qrb, sizeof(current_qrb), current_quj, sizeof(current_quj));
                        } else {
                            copy_str(current_qrb, sizeof(current_qrb), "-");
                            copy_str(current_quj, sizeof(current_quj), "-");
                        }
                    } else if (is_valid_locator(current_qso.gridsquare) == 6) {
                        compute_qrb_quj_str(&station, &current_qso, current_qrb, sizeof(current_qrb), current_quj, sizeof(current_quj));
                    } else {
                        copy_str(current_qrb, sizeof(current_qrb), "-");
                        copy_str(current_quj, sizeof(current_quj), "-");
                    }
                }
                if (active_field == FLD_QTH) {
                    if (current_qso.qth[0] == '\0') {
                        if (is_valid_locator(current_qso.gridsquare)) {
                            compute_qrb_quj_str(&station, &current_qso, current_qrb, sizeof(current_qrb), current_quj, sizeof(current_quj));
                        } else {
                            copy_str(current_qrb, sizeof(current_qrb), "-");
                            copy_str(current_quj, sizeof(current_quj), "-");
                        }
                    }
                }
            }
        } else if (active_field != FLD_BAND && input_res == OK && wch >= 32 && wch != 127) {
            /* Frys datum och tid när användaren börjar mata in i ett fält (eller redigerar datum/tid manuellt) */
            if (time_frozen == 0 || active_field == FLD_DATE || active_field == FLD_TIME) {
                if (time_frozen <= 0) {
                    get_utc_strings(current_qso.date, sizeof(current_qso.date),
                                    current_qso.time_utc, sizeof(current_qso.time_utc),
                                    NULL, 0);
                    time_frozen = 1;
                    copy_str(status_msg, sizeof(status_msg), "Tid & datum låsta för QSO.");
                }
            }

            size_t max_len = 0;
            char *field_ptr = get_qso_field_ptr(&current_qso, active_field, &max_len);
            if (field_ptr != NULL) {
                /* När man börjar skriva i ett fält ska fältet rensas innan första bokstaven visas */
                if (field_fresh) {
                    field_ptr[0] = '\0';
                    field_cursor_pos = 0;
                    field_fresh = 0;
                    if (active_field == FLD_CALL) {
                        last_smcb_call[0] = '\0';
                        current_qso.dxcc[0] = '\0';
                    }
                }

                if (active_field == FLD_CALL || active_field == FLD_MODE || active_field == FLD_GRID) {
                    wch = towupper(wch);
                }

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

                utf8_insert_char_at(field_ptr, max_len, &field_cursor_pos, u8, u8_len);

                if (active_field == FLD_CALL) {
                    last_smcb_call[0] = '\0';
                    lookup_dxcc(current_qso.call, current_qso.dxcc, sizeof(current_qso.dxcc));
                }
                if (active_field == FLD_FREQ) {
                    resolve_band_from_freq(current_qso.freq, current_qso.band, sizeof(current_qso.band));
                }
                if (active_field == FLD_GRID) {
                    if (is_valid_locator(current_qso.gridsquare) == 6) {
                        compute_qrb_quj_str(&station, &current_qso, current_qrb, sizeof(current_qrb), current_quj, sizeof(current_quj));
                    }
                }
            }
        }
    }

    /* Spara eventuella radioinställningar vid avslut */
    apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                  last_smcb_call, sizeof(last_smcb_call),
                                  status_msg, sizeof(status_msg),
                                  current_qrb, sizeof(current_qrb),
                                  current_quj, sizeof(current_quj));
    persist_qso_radio_settings(&current_qso, &station);

    /* Rensa resurser och stäng bakgrundstrådar */
    callbook_cleanup();
    qrz_cleanup();
    space_weather_cleanup();
    tui_cleanup();
    free(log_list.items);
    free_dxcc_db();
    db_close();
    curl_global_cleanup();

    return 0;
}
