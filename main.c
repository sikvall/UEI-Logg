/**
 * @file main.c
 * @brief Huvudprogram och händelseloop för qsolog – ett modernt TUI-loggprogram för amatörradio.
 */

#include "qsolog.h"
#include "callbook.h"
#include "dxcc.h"
#include "logg.h"
#include "qrb.h"
#include "settings.h"
#include "space_weather.h"
#include "tui.h"
#include "util.h"
#include <curl/curl.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/**
 * @brief Programmets huvudfunktion: initierar delsystem, laddar data och driver händelseloopen.
 * @return 0 vid normal avslutning.
 */
int main(void) {
    /* Sätt UTF-8 locale för korrekt teckenbredd och teckenuppsättning */
    setlocale(LC_ALL, "");

    /* Initiera globala nätverksresurser (libcurl) */
    curl_global_init(CURL_GLOBAL_DEFAULT);

    /* Läs in DXCC-tabellen från dxcc.txt vid programstart */
    load_dxcc_file(DXCC_FILE);

    /* Läs in min stations inställningar från station.conf */
    StationSettings station;
    load_station_settings(CONFIG_FILE, &station);

    /* Läs in loggboken från qsolog.adi */
    QSOList log_list = {NULL, 0, 0};
    load_qsos_from_adif(LOG_FILE, &log_list);

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
    int editing_index = -1;
    int idle_ticks_on_call = 0;
    int idle_ticks_on_qth = 0;
    int idle_ticks_on_grid = 0;
    char current_qrb[32] = "-";
    FocusMode focus_mode = FOCUS_INPUT;
    int selected_log_pos = 0;
    int log_scroll_offset = 0;
    char search_filter[32] = "";
    char last_smcb_call[32] = "";
    char status_msg[128] = "";

    reset_qso_form(&current_qso, &station, &time_frozen, &active_field, &field_cursor_pos, &editing_index, last_smcb_call, current_qrb);

    /* HUVUDLOOP */
    while (1) {
        /* Uppdatera kontinuerligt UTC-tid och datum om formuläret inte är fryst */
        if (!time_frozen && editing_index < 0) {
            get_utc_strings(current_qso.date, sizeof(current_qso.date),
                            current_qso.time_utc, sizeof(current_qso.time_utc),
                            NULL, 0);
        }

        /* Kontrollera om bakgrundssökningen i SM Callbook har blivit klar */
        poll_smcb_background_result(&current_qso, &station,
                                    active_field, &field_cursor_pos,
                                    status_msg, sizeof(status_msg),
                                    current_qrb, sizeof(current_qrb));

        /* Rita huvudfönstret */
        int cur_y = 2, cur_x = 16;
        const char *active_filter = (search_filter[0] != '\0') ? search_filter :
                                    (station.filter_log && current_qso.call[0] != '\0') ? current_qso.call : NULL;
        draw_main_screen(&station, &current_qso, focus_mode, active_field, band_selecting,
                         field_cursor_pos, time_frozen, editing_index,
                         &log_list, selected_log_pos, &log_scroll_offset,
                         active_filter, status_msg, current_qrb, &cur_y, &cur_x);

        /* Läs in användartangent med timeout */
        int ch = getch();
        if (ch == ERR) {
            /* Paus i inmatning: utför automatiska uppslag när användaren stannar upp */
            if (focus_mode == FOCUS_INPUT && active_field == FLD_CALL && current_qso.call[0] != '\0') {
                idle_ticks_on_call++;
                if (idle_ticks_on_call == 2) {
                    maybe_lookup_call_details(&current_qso, &log_list, &station,
                                              last_smcb_call, sizeof(last_smcb_call),
                                              status_msg, sizeof(status_msg));
                    compute_qrb_str(&station, &current_qso, current_qrb, sizeof(current_qrb));
                }
            }
            if (focus_mode == FOCUS_INPUT && active_field == FLD_QTH && current_qso.qth[0] != '\0') {
                idle_ticks_on_qth++;
                if (idle_ticks_on_qth == 3) {
                    compute_qrb_str(&station, &current_qso, current_qrb, sizeof(current_qrb));
                }
            }
            if (focus_mode == FOCUS_INPUT && active_field == FLD_GRID && current_qso.gridsquare[0] != '\0') {
                idle_ticks_on_grid++;
                if (idle_ticks_on_grid == 3) {
                    if (is_valid_locator(current_qso.gridsquare)) {
                        compute_qrb_str(&station, &current_qso, current_qrb, sizeof(current_qrb));
                    }
                }
            }
            continue;
        }

        idle_ticks_on_call = 0;
        idle_ticks_on_qth = 0;
        idle_ticks_on_grid = 0;

        /* GLOBALA FUNKTIONSTANGENTER */
        if (ch == KEY_F(1) || ch == 27) {
            /* F1 eller ESC: Rensa formuläret och låt UTC-klockan ticka igen */
            focus_mode = FOCUS_INPUT;
            band_selecting = 0;
            reset_qso_form(&current_qso, &station, &time_frozen, &active_field, &field_cursor_pos, &editing_index, last_smcb_call, current_qrb);
            preferred_col = 0;
            copy_str(status_msg, sizeof(status_msg), "Formulär rensat – UTC-klockan tickar igen.");
            continue;
        } else if (ch == KEY_F(3) || ch == 6) {
            /* F3 eller Ctrl+F: Sök i logglistan */
            band_selecting = 0;
            run_search_bar(&station, search_filter, sizeof(search_filter));
            selected_log_pos = 0;
            log_scroll_offset = 0;
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
                    copy_str(status_msg, sizeof(status_msg), "Loggrader: Ins=Märk, F5=Hanterad, F6=Ny, F8=Radera, Enter=Redigera.");
                } else {
                    copy_str(status_msg, sizeof(status_msg), "Inga loggrader att välja ännu.");
                }
            } else {
                focus_mode = FOCUS_INPUT;
                sync_cursor_to_field_end(&current_qso, active_field, &field_cursor_pos);
                copy_str(status_msg, sizeof(status_msg), "Åter i inmatningsfälten.");
            }
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
            run_settings_modal(&station, &current_qso, editing_index, &log_list,
                               &selected_log_pos, &log_scroll_offset,
                               status_msg, sizeof(status_msg));
            compute_qrb_str(&station, &current_qso, current_qrb, sizeof(current_qrb));
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
            } else if (ch == KEY_CTRL_HOME) {
                /* Ctrl-Home: gå till översta raden i hela loggen */
                selected_log_pos = 0;
                log_scroll_offset = 0;
            } else if (ch == KEY_CTRL_END) {
                /* Ctrl-End: gå till sista raden i hela loggen */
                if (f_cnt > 0) {
                    selected_log_pos = f_cnt - 1;
                    int avail = get_avail_log_rows();
                    log_scroll_offset = f_cnt - avail;
                    if (log_scroll_offset < 0) log_scroll_offset = 0;
                }
            } else if (ch == KEY_HOME || ch == KEY_SHOME || ch == KEY_A1) {
                /* Home: gå till första raden på den sida vi ser */
                if (f_cnt > 0) {
                    selected_log_pos = log_scroll_offset;
                    if (selected_log_pos >= f_cnt) selected_log_pos = f_cnt - 1;
                    if (selected_log_pos < 0) selected_log_pos = 0;
                }
            } else if (ch == KEY_END || ch == KEY_SEND || ch == KEY_LL || ch == KEY_C1) {
                /* End: gå till sista raden på den sida vi ser */
                if (f_cnt > 0) {
                    int avail = get_avail_log_rows();
                    int last_vis = log_scroll_offset + avail - 1;
                    if (last_vis >= f_cnt) last_vis = f_cnt - 1;
                    if (last_vis < 0) last_vis = 0;
                    selected_log_pos = last_vis;
                }
            } else if (ch == KEY_PPAGE) {
                /* PgUp: bläddra en sida uppåt */
                int avail = get_avail_log_rows();
                selected_log_pos -= avail;
                if (selected_log_pos < 0) selected_log_pos = 0;
                log_scroll_offset -= avail;
                if (log_scroll_offset < 0) log_scroll_offset = 0;
            } else if (ch == KEY_NPAGE) {
                /* PgDown: bläddra en sida nedåt */
                int avail = get_avail_log_rows();
                selected_log_pos += avail;
                if (selected_log_pos >= f_cnt) selected_log_pos = f_cnt > 0 ? f_cnt - 1 : 0;
                if (selected_log_pos < 0) selected_log_pos = 0;
                log_scroll_offset += avail;
                if (log_scroll_offset + avail > f_cnt) {
                    log_scroll_offset = f_cnt - avail;
                    if (log_scroll_offset < 0) log_scroll_offset = 0;
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
                /* F5: Markera som hanterad (✓) för markerade rader eller aktuell rad */
                int m_cnt = 0;
                for (size_t mi = 0; mi < log_list.count; ++mi) {
                    if (log_list.items[mi].marked) m_cnt++;
                }
                if (m_cnt > 0) {
                    for (size_t mi = 0; mi < log_list.count; ++mi) {
                        if (log_list.items[mi].marked) {
                            log_list.items[mi].handled = 1;
                        }
                    }
                    if (editing_index >= 0 && (size_t)editing_index < log_list.count && log_list.items[editing_index].marked) {
                        current_qso.handled = 1;
                    }
                    save_all_qsos_adif(LOG_FILE, &log_list, &station);
                    snprintf(status_msg, sizeof(status_msg), "Markerade %d rader som hanterade (✓).", m_cnt);
                } else if (f_cnt > 0 && selected_log_pos >= 0 && selected_log_pos < f_cnt) {
                    int orig_idx = f_idx[selected_log_pos];
                    log_list.items[orig_idx].handled = 1;
                    if (editing_index == orig_idx) {
                        current_qso.handled = 1;
                    }
                    save_all_qsos_adif(LOG_FILE, &log_list, &station);
                    snprintf(status_msg, sizeof(status_msg), "Markerade QSO med %s som hanterat (✓).", log_list.items[orig_idx].call);
                }
            } else if (ch == KEY_F(6)) {
                /* F6: Markera som ny (✗) för markerade rader eller aktuell rad */
                int m_cnt = 0;
                for (size_t mi = 0; mi < log_list.count; ++mi) {
                    if (log_list.items[mi].marked) m_cnt++;
                }
                if (m_cnt > 0) {
                    for (size_t mi = 0; mi < log_list.count; ++mi) {
                        if (log_list.items[mi].marked) {
                            log_list.items[mi].handled = 0;
                        }
                    }
                    if (editing_index >= 0 && (size_t)editing_index < log_list.count && log_list.items[editing_index].marked) {
                        current_qso.handled = 0;
                    }
                    save_all_qsos_adif(LOG_FILE, &log_list, &station);
                    snprintf(status_msg, sizeof(status_msg), "Markerade %d rader som nya (✗).", m_cnt);
                } else if (f_cnt > 0 && selected_log_pos >= 0 && selected_log_pos < f_cnt) {
                    int orig_idx = f_idx[selected_log_pos];
                    log_list.items[orig_idx].handled = 0;
                    if (editing_index == orig_idx) {
                        current_qso.handled = 0;
                    }
                    save_all_qsos_adif(LOG_FILE, &log_list, &station);
                    snprintf(status_msg, sizeof(status_msg), "Markerade QSO med %s som nytt (✗).", log_list.items[orig_idx].call);
                }
            } else if (ch == KEY_F(8)) {
                /* F8: Radera markerade rader eller aktuell rad */
                int m_cnt = 0;
                for (size_t mi = 0; mi < log_list.count; ++mi) {
                    if (log_list.items[mi].marked) m_cnt++;
                }
                if (m_cnt > 0) {
                    int del_cnt = qso_list_delete_marked(&log_list);
                    save_all_qsos_adif(LOG_FILE, &log_list, &station);
                    editing_index = -1;
                    snprintf(status_msg, sizeof(status_msg), "Raderade %d markerade QSO:n.", del_cnt);
                } else if (f_cnt > 0 && selected_log_pos >= 0 && selected_log_pos < f_cnt) {
                    int orig_idx = f_idx[selected_log_pos];
                    char del_call[32];
                    copy_str(del_call, sizeof(del_call), log_list.items[orig_idx].call);
                    qso_list_delete_at(&log_list, (size_t)orig_idx);
                    save_all_qsos_adif(LOG_FILE, &log_list, &station);
                    editing_index = -1;
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
                    } else if (selected_log_pos >= new_fcnt) {
                        selected_log_pos = new_fcnt - 1;
                    }
                }
            } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == KEY_F(2)) {
                /* Enter / F2: Ladda valt QSO till formuläret för redigering */
                if (f_cnt > 0 && selected_log_pos >= 0 && selected_log_pos < f_cnt) {
                    int orig_idx = f_idx[selected_log_pos];
                    current_qso = log_list.items[orig_idx];
                    editing_index = orig_idx;
                    time_frozen = 1;
                    active_field = FLD_CALL;
                    band_selecting = 0;
                    preferred_col = 0;
                    sync_cursor_to_field_end(&current_qso, active_field, &field_cursor_pos);
                    focus_mode = FOCUS_INPUT;
                    compute_qrb_str(&station, &current_qso, current_qrb, sizeof(current_qrb));
                    if (strcmp(current_qrb, "-") == 0 && current_qso.distance[0] != '\0') {
                        snprintf(current_qrb, sizeof(current_qrb), "%s km", current_qso.distance);
                    }
                    snprintf(status_msg, sizeof(status_msg),
                             "Redigerar QSO #%d (%s) – Enter sparar ändring.",
                             orig_idx + 1, current_qso.call);
                }
            }
            free(f_idx);
            continue;
        }

        /* FOKUS I INMATNINGSFÄLTEN (FOCUS_INPUT) */
        if (active_field == FLD_BAND && (ch == '\n' || ch == '\r' || ch == KEY_ENTER)) {
            band_selecting = !band_selecting;
            if (band_selecting) {
                copy_str(status_msg, sizeof(status_msg), "Välj band med piltangenterna, tryck Enter när du är klar.");
            } else {
                snprintf(status_msg, sizeof(status_msg), "Band valt: %s (%s MHz)", current_qso.band, current_qso.freq);
            }
        } else if (active_field == FLD_BAND && band_selecting && (ch == KEY_LEFT || ch == KEY_UP)) {
            if (!time_frozen) {
                get_utc_strings(current_qso.date, sizeof(current_qso.date),
                                current_qso.time_utc, sizeof(current_qso.time_utc),
                                NULL, 0);
                time_frozen = 1;
            }
            step_band_choice(&current_qso, &station, -1);
        } else if (active_field == FLD_BAND && band_selecting && (ch == KEY_RIGHT || ch == KEY_DOWN || ch == ' ')) {
            if (!time_frozen) {
                get_utc_strings(current_qso.date, sizeof(current_qso.date),
                                current_qso.time_utc, sizeof(current_qso.time_utc),
                                NULL, 0);
                time_frozen = 1;
            }
            step_band_choice(&current_qso, &station, +1);
        } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == KEY_F(2) || ch == 19) {
            /* Enter i övriga fält eller F2 loggar och sparar QSO:t */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg),
                                          current_qrb, sizeof(current_qrb));
            try_save_current_qso(&current_qso, &time_frozen, &active_field,
                                 &field_cursor_pos, &editing_index,
                                 &station, &log_list, last_smcb_call, status_msg, sizeof(status_msg),
                                 current_qrb);
            preferred_col = field_default_col(active_field);
        } else if (ch == '\t' || ch == KEY_RIGHT) {
            /* Tab / Högerpil stegar till nästa inmatningsfält */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg),
                                          current_qrb, sizeof(current_qrb));
            active_field = (active_field + 1) % QSO_FIELD_COUNT;
            preferred_col = field_default_col(active_field);
            sync_cursor_to_field_end(&current_qso, active_field, &field_cursor_pos);
        } else if (ch == KEY_BTAB || ch == KEY_LEFT) {
            /* Shift-Tab / Vänsterpil stegar till föregående fält */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg),
                                          current_qrb, sizeof(current_qrb));
            active_field = (active_field + QSO_FIELD_COUNT - 1) % QSO_FIELD_COUNT;
            preferred_col = field_default_col(active_field);
            sync_cursor_to_field_end(&current_qso, active_field, &field_cursor_pos);
        } else if (ch == KEY_DOWN) {
            /* Pil ned går till fältet rakt under */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg),
                                          current_qrb, sizeof(current_qrb));
            if (active_field != FLD_COMM) {
                preferred_col = field_default_col(active_field);
            }
            active_field = step_field_vertical(active_field, +1, preferred_col);
            sync_cursor_to_field_end(&current_qso, active_field, &field_cursor_pos);
        } else if (ch == KEY_UP) {
            /* Pil upp går till fältet rakt ovanför */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg),
                                          current_qrb, sizeof(current_qrb));
            if (active_field != FLD_COMM) {
                preferred_col = field_default_col(active_field);
            }
            active_field = step_field_vertical(active_field, -1, preferred_col);
            sync_cursor_to_field_end(&current_qso, active_field, &field_cursor_pos);
        } else if (ch == KEY_HOME || ch == KEY_SHOME || ch == KEY_A1 || ch == KEY_CTRL_HOME) {
            /* Home: hoppa till Signal (FLD_CALL) */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg),
                                          current_qrb, sizeof(current_qrb));
            active_field = FLD_CALL;
            preferred_col = 0;
            sync_cursor_to_field_end(&current_qso, active_field, &field_cursor_pos);
        } else if (ch == KEY_END || ch == KEY_SEND || ch == KEY_LL || ch == KEY_C1 || ch == KEY_CTRL_END) {
            /* End: hoppa till Not (FLD_COMM) */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg),
                                          current_qrb, sizeof(current_qrb));
            active_field = FLD_COMM;
            preferred_col = 0;
            sync_cursor_to_field_end(&current_qso, active_field, &field_cursor_pos);
        } else if (ch == KEY_PPAGE) {
            /* PgUp: bläddra loggraderna uppåt */
            int avail = get_avail_log_rows();
            log_scroll_offset -= avail;
            if (log_scroll_offset < 0) log_scroll_offset = 0;
            selected_log_pos -= avail;
            if (selected_log_pos < 0) selected_log_pos = 0;
        } else if (ch == KEY_NPAGE) {
            /* PgDown: bläddra loggraderna nedåt */
            int max_f = (int)log_list.count;
            int avail = get_avail_log_rows();
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
                            compute_qrb_str(&station, &current_qso, current_qrb, sizeof(current_qrb));
                        } else {
                            copy_str(current_qrb, sizeof(current_qrb), "-");
                        }
                    } else if (is_valid_locator(current_qso.gridsquare) == 6) {
                        compute_qrb_str(&station, &current_qso, current_qrb, sizeof(current_qrb));
                    } else {
                        copy_str(current_qrb, sizeof(current_qrb), "-");
                    }
                }
                if (active_field == FLD_QTH) {
                    if (current_qso.qth[0] == '\0') {
                        if (is_valid_locator(current_qso.gridsquare)) {
                            compute_qrb_str(&station, &current_qso, current_qrb, sizeof(current_qrb));
                        } else {
                            copy_str(current_qrb, sizeof(current_qrb), "-");
                        }
                    }
                }
            }
        } else if (active_field != FLD_BAND && ch == KEY_DC) {
            size_t max_len = 0;
            char *field_ptr = get_qso_field_ptr(&current_qso, active_field, &max_len);
            if (field_ptr != NULL) {
                utf8_delete_at(field_ptr, field_cursor_pos);
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
                            compute_qrb_str(&station, &current_qso, current_qrb, sizeof(current_qrb));
                        } else {
                            copy_str(current_qrb, sizeof(current_qrb), "-");
                        }
                    } else if (is_valid_locator(current_qso.gridsquare) == 6) {
                        compute_qrb_str(&station, &current_qso, current_qrb, sizeof(current_qrb));
                    } else {
                        copy_str(current_qrb, sizeof(current_qrb), "-");
                    }
                }
                if (active_field == FLD_QTH) {
                    if (current_qso.qth[0] == '\0') {
                        if (is_valid_locator(current_qso.gridsquare)) {
                            compute_qrb_str(&station, &current_qso, current_qrb, sizeof(current_qrb));
                        } else {
                            copy_str(current_qrb, sizeof(current_qrb), "-");
                        }
                    }
                }
            }
        } else if (active_field != FLD_BAND && ch >= 32 && ch != 127 && ch < KEY_MIN) {
            /* Frys datum och tid när användaren börjar mata in i ett fält */
            if (!time_frozen) {
                get_utc_strings(current_qso.date, sizeof(current_qso.date),
                                current_qso.time_utc, sizeof(current_qso.time_utc),
                                NULL, 0);
                time_frozen = 1;
                copy_str(status_msg, sizeof(status_msg), "Tid & datum låsta för QSO.");
            }

            size_t max_len = 0;
            char *field_ptr = get_qso_field_ptr(&current_qso, active_field, &max_len);
            if (field_ptr != NULL) {
                char c = (char)ch;
                if ((active_field == FLD_CALL || active_field == FLD_MODE || active_field == FLD_GRID) &&
                    (unsigned char)c < 128) {
                    c = (char)toupper((unsigned char)c);
                }
                utf8_insert_byte_at(field_ptr, max_len, &field_cursor_pos, c);

                if (active_field == FLD_CALL) {
                    last_smcb_call[0] = '\0';
                    lookup_dxcc(current_qso.call, current_qso.dxcc, sizeof(current_qso.dxcc));
                }
                if (active_field == FLD_FREQ) {
                    resolve_band_from_freq(current_qso.freq, current_qso.band, sizeof(current_qso.band));
                }
                if (active_field == FLD_GRID) {
                    if (is_valid_locator(current_qso.gridsquare) == 6) {
                        compute_qrb_str(&station, &current_qso, current_qrb, sizeof(current_qrb));
                    }
                }
            }
        }
    }

    /* Spara eventuella radioinställningar vid avslut */
    apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                  last_smcb_call, sizeof(last_smcb_call),
                                  status_msg, sizeof(status_msg),
                                  current_qrb, sizeof(current_qrb));
    persist_qso_radio_settings(&current_qso, &station);

    /* Rensa resurser och stäng bakgrundstrådar */
    callbook_cleanup();
    space_weather_cleanup();
    tui_cleanup();
    free(log_list.items);
    free_dxcc_db();
    curl_global_cleanup();

    return 0;
}
