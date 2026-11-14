/**
 * @file settings.c
 * @brief Stationsinställningar, ADIF-import och -export, fildialog samt inställningsmodalen (F12).
 */

#include "settings.h"
#include "db.h"
#include "dxcc.h"
#include "logg.h"
#include "qrb.h"
#include "tui.h"
#include "util.h"
#include "file_dialog.h"
#include "help.h"
#include <ctype.h>
#include <dirent.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

/**
 * @brief Läser in stationsinställningar från station.conf vid programstart.
 * @param filename Sökväg till konfigurationsfilen (station.conf).
 * @param st Struktur som fylls med inställningar.
 */
void load_station_settings(const char *filename, StationSettings *st) {
    memset(st, 0, sizeof(*st));
    st->smcb_enabled = 0;
    st->qrz_enabled = 0;
    st->show_stats = 1;
    st->show_space_weather = 1;
    st->qrz_username[0] = '\0';
    st->qrz_password[0] = '\0';
    copy_str(st->last_band, sizeof(st->last_band), "20 m");
    copy_str(st->last_freq, sizeof(st->last_freq), "14.200");
    copy_str(st->last_mode, sizeof(st->last_mode), "SSB");

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) return;

    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        trim_newline(line);
        char *eq = strchr(line, '=');
        if (eq == NULL) continue;
        *eq = '\0';
        const char *key = line;
        const char *val = eq + 1;

        if (strcasecmp(key, "CALL") == 0) {
            copy_str(st->call, sizeof(st->call), val);
        } else if (strcasecmp(key, "NAME") == 0) {
            copy_str(st->name, sizeof(st->name), val);
        } else if (strcasecmp(key, "QTH") == 0) {
            copy_str(st->qth, sizeof(st->qth), val);
        } else if (strcasecmp(key, "GRIDSQUARE") == 0) {
            copy_str(st->gridsquare, sizeof(st->gridsquare), val);
        } else if (strcasecmp(key, "TIMEZONE") == 0) {
            copy_str(st->timezone, sizeof(st->timezone), val);
        } else if (strcasecmp(key, "THEME") == 0) {
            int t = atoi(val);
            if (t >= 0 && t < THEME_COUNT) {
                st->theme = t;
            }
        } else if (strcasecmp(key, "SMCB") == 0) {
            st->smcb_enabled = (atoi(val) != 0) ? 1 : 0;
        } else if (strcasecmp(key, "QRZ_USER") == 0) {
            copy_str(st->qrz_username, sizeof(st->qrz_username), val);
        } else if (strcasecmp(key, "QRZ_PASS") == 0) {
            copy_str(st->qrz_password, sizeof(st->qrz_password), val);
        } else if (strcasecmp(key, "QRZ_ENABLED") == 0) {
            st->qrz_enabled = (atoi(val) != 0) ? 1 : 0;
        } else if (strcasecmp(key, "AUTO_RST") == 0) {
            st->auto_rst_59 = (atoi(val) != 0) ? 1 : 0;
        } else if (strcasecmp(key, "FILTER_LOG") == 0) {
            st->filter_log = (atoi(val) != 0) ? 1 : 0;
        } else if (strcasecmp(key, "SHOW_STATS") == 0 || strcasecmp(key, "STATS") == 0) {
            st->show_stats = (atoi(val) != 0) ? 1 : 0;
        } else if (strcasecmp(key, "SHOW_SW") == 0 || strcasecmp(key, "SHOW_SPACE_WEATHER") == 0 || strcasecmp(key, "SPACE_WEATHER") == 0) {
            st->show_space_weather = (atoi(val) != 0) ? 1 : 0;
        } else if (strcasecmp(key, "FREQ") == 0 && val[0] != '\0') {
            char norm[32];
            parse_and_normalize_freq(val, norm, sizeof(norm));
            copy_str(st->last_freq, sizeof(st->last_freq), norm[0] ? norm : val);
        } else if (strcasecmp(key, "BAND") == 0 && val[0] != '\0') {
            format_band_display(val, st->last_band, sizeof(st->last_band));
        } else if (strcasecmp(key, "MODE") == 0 && val[0] != '\0') {
            copy_str(st->last_mode, sizeof(st->last_mode), val);
            to_upper_str(st->last_mode);
        }
    }
    fclose(fp);
}

/**
 * @brief Sparar stationsinställningar till station.conf på disk.
 * @param filename Sökväg till konfigurationsfilen (station.conf).
 * @param st Struktur med aktuella inställningar.
 * @return 0 vid framgång, -1 vid fel.
 */
int save_station_settings(const char *filename, const StationSettings *st) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) return -1;
    fprintf(fp, "CALL=%s\n", st->call);
    fprintf(fp, "NAME=%s\n", st->name);
    fprintf(fp, "QTH=%s\n", st->qth);
    fprintf(fp, "GRIDSQUARE=%s\n", st->gridsquare);
    fprintf(fp, "TIMEZONE=%s\n", st->timezone);
    fprintf(fp, "THEME=%d\n", st->theme);
    fprintf(fp, "SMCB=%d\n", st->smcb_enabled ? 1 : 0);
    fprintf(fp, "QRZ_USER=%s\n", st->qrz_username);
    fprintf(fp, "QRZ_PASS=%s\n", st->qrz_password);
    fprintf(fp, "QRZ_ENABLED=%d\n", st->qrz_enabled ? 1 : 0);
    fprintf(fp, "AUTO_RST=%d\n", st->auto_rst_59 ? 1 : 0);
    fprintf(fp, "FILTER_LOG=%d\n", st->filter_log ? 1 : 0);
    fprintf(fp, "SHOW_STATS=%d\n", st->show_stats ? 1 : 0);
    fprintf(fp, "SHOW_SW=%d\n", st->show_space_weather ? 1 : 0);
    fprintf(fp, "FREQ=%s\n", st->last_freq);
    fprintf(fp, "BAND=%s\n", st->last_band);
    fprintf(fp, "MODE=%s\n", st->last_mode);
    fclose(fp);
    return 0;
}

/**
 * @brief Rensa hela loggboken i minnet och på disk (qsolog.adi skrivs över med tom ADIF 3.1.8-header).
 * @param log_list Logglistan.
 * @param st Stationsinställningar.
 * @return 0 vid framgång, -1 vid fel.
 */
int clear_log_book(QSOList *log_list, const StationSettings *st) {
    (void)st;
    if (log_list != NULL) {
        log_list->count = 0;
    }
    if (db_is_open()) {
        db_clear_all_qsos();
    } else {
        FILE *fp = fopen(LOG_FILE, "w");
        if (fp == NULL) return -1;
        write_adif_header(fp);
        fclose(fp);
    }
    return 0;
}

/**
 * @brief Slutbehandlar ett importerat QSO:
 * Normaliserar versaler, datum, tid, frekvens, band, standard-RST, DXCC och beräknar QRB.
 * Sätter handled = 0 (importerade rader markeras alltid som nya).
 * @param current Det importerade QSO:t.
 * @param st Stationsinställningar.
 */
void finalize_imported_qso(QSO *current, const StationSettings *st) {
    to_upper_str(current->call);
    to_upper_str(current->gridsquare);

    /* Formatera datum och tid */
    if (current->date[0] != '\0') {
        format_date_yyyy_mm_dd(current->date, current->date, sizeof(current->date));
    }
    if (current->time_utc[0] != '\0') {
        format_time_hh_mm(current->time_utc, current->time_utc, sizeof(current->time_utc));
    }

    /* Frekvens och band */
    if (current->freq[0] != '\0') {
        char norm_freq[32];
        parse_and_normalize_freq_adif(current->freq, norm_freq, sizeof(norm_freq));
        if (norm_freq[0] != '\0') {
            copy_str(current->freq, sizeof(current->freq), norm_freq);
        }
    }
    if (current->band[0] != '\0') {
        format_band_display(current->band, current->band, sizeof(current->band));
    } else if (current->freq[0] != '\0') {
        resolve_band_from_freq(current->freq, current->band, sizeof(current->band));
    }
    if (current->band[0] == '\0') {
        copy_str(current->band, sizeof(current->band), "20 m");
    }
    if (current->freq[0] == '\0') {
        int bidx = find_band_index(current->band);
        if (bidx >= 0) copy_str(current->freq, sizeof(current->freq), BAND_DEFAULT_FREQ[bidx]);
        else copy_str(current->freq, sizeof(current->freq), "14.200");
    }

    /* Trafiksätt */
    if (current->mode[0] == '\0') {
        copy_str(current->mode, sizeof(current->mode), "SSB");
    }
    to_upper_str(current->mode);

    /* RST */
    if (current->rst_sent[0] == '\0') {
        copy_str(current->rst_sent, sizeof(current->rst_sent), strcmp(current->mode, "CW") == 0 ? "599" : "59");
    }
    if (current->rst_rcvd[0] == '\0') {
        copy_str(current->rst_rcvd, sizeof(current->rst_rcvd), strcmp(current->mode, "CW") == 0 ? "599" : "59");
    }

    /* DXCC */
    if (current->dxcc[0] == '\0' && current->call[0] != '\0') {
        lookup_dxcc(current->call, current->dxcc, sizeof(current->dxcc));
    }

    /* Avstånd (QRB i hela kilometer) */
    if (current->distance[0] == '\0' && st != NULL) {
        char calc_qrb[32];
        compute_qrb_str(st, current, calc_qrb, sizeof(calc_qrb));
        if (calc_qrb[0] != '\0' && strcmp(calc_qrb, "-") != 0) {
            int d_val = (int)atol(calc_qrb);
            if (d_val > 0) {
                snprintf(current->distance, sizeof(current->distance), "%d", d_val);
            }
        }
    } else if (current->distance[0] != '\0') {
        int d_val = (int)atol(current->distance);
        if (d_val > 0) {
            snprintf(current->distance, sizeof(current->distance), "%d", d_val);
        }
    }

    /* Rader som importeras markeras alltid som nya (handled = 0) */
    current->handled = 0;
}

/**
 * @brief Importerar och slår samman ADIF-poster från en extern fil till aktuell logg.
 * Sorterar listan kronologiskt och sparar till qsolog.adi.
 * @param filename Sökvägen till ADIF-filen som ska importeras.
 * @param list Logglistan.
 * @param st Stationsinställningar.
 * @return Antal importerade poster, eller -1 vid filfel.
 */
int import_qsos_from_adif(const char *filename, QSOList *list, const StationSettings *st) {
    if (filename == NULL || filename[0] == '\0' || list == NULL) return -1;
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) return -1;

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fsize <= 0) {
        fclose(fp);
        return 0;
    }

    char *content = malloc((size_t)fsize + 1);
    if (content == NULL) {
        fclose(fp);
        return -1;
    }
    size_t read_bytes = fread(content, 1, (size_t)fsize, fp);
    content[read_bytes] = '\0';
    fclose(fp);

    const char *p = content;
    const char *cursor = content;
    while ((cursor = strchr(cursor, '<')) != NULL) {
        if (strncasecmp(cursor, "<EOH>", 5) == 0) {
            p = cursor + 5;
            break;
        }
        cursor++;
    }

    int imported_count = 0;
    QSO current;
    memset(&current, 0, sizeof(current));

    while ((p = strchr(p, '<')) != NULL) {
        const char *end_tag = strchr(p, '>');
        if (end_tag == NULL) break;

        char tag_buf[64];
        size_t tag_len = (size_t)(end_tag - (p + 1));
        if (tag_len >= sizeof(tag_buf)) tag_len = sizeof(tag_buf) - 1;
        memcpy(tag_buf, p + 1, tag_len);
        tag_buf[tag_len] = '\0';

        if (strcasecmp(tag_buf, "EOR") == 0) {
            if (current.call[0] != '\0') {
                finalize_imported_qso(&current, st);
                if (!qso_is_duplicate(list, &current)) {
                    qso_list_push(list, &current);
                    imported_count++;
                }
            }
            memset(&current, 0, sizeof(current));
            p = end_tag + 1;
            continue;
        }

        char *colon = strchr(tag_buf, ':');
        if (colon != NULL) {
            *colon = '\0';
            int val_len = atoi(colon + 1);
            if (val_len < 0) val_len = 0;
            const char *val_start = end_tag + 1;
            size_t avail = strlen(val_start);
            if ((size_t)val_len > avail) val_len = (int)avail;

            char val_buf[512];
            size_t copy_len = (size_t)val_len < sizeof(val_buf) - 1 ? (size_t)val_len : sizeof(val_buf) - 1;
            memcpy(val_buf, val_start, copy_len);
            val_buf[copy_len] = '\0';

            assign_adif_field(&current, tag_buf, val_buf);
            p = val_start + val_len;
        } else {
            p = end_tag + 1;
        }
    }

    if (current.call[0] != '\0') {
        finalize_imported_qso(&current, st);
        if (!qso_is_duplicate(list, &current)) {
            qso_list_push(list, &current);
            imported_count++;
        }
    }

    free(content);

    if (imported_count > 0) {
        qsort(list->items, list->count, sizeof(QSO), cmp_qso_chronological);
        if (db_is_open()) {
            db_clear_all_qsos();
            db_batch_insert(list->items, list->count);
        } else {
            save_all_qsos_adif(LOG_FILE, list, st);
        }
    }

    return imported_count;
}

/**
 * @brief Exporterar QSO till en extern ADIF-fil.
 * export_mode:
 *   EXPORT_MODE_ALL (0)      = alla QSO i listan
 *   EXPORT_MODE_MARKED (1)   = endast markerade poster (marked == 1)
 *   EXPORT_MODE_ONLY_NEW (2) = endast nya poster (handled == 0)
 * Rader som exporteras markeras som hanterade (handled = 1).
 * @param filename Sökvägen till målfilen.
 * @param list Logglistan.
 * @param st Stationsinställningar.
 * @param export_mode Exportläge.
 * @return Antal exporterade poster, eller -1 vid fel.
 */
int export_qsos_adif(const char *filename, QSOList *list, const StationSettings *st, int export_mode) {
    if (filename == NULL || filename[0] == '\0' || list == NULL) return -1;
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) return -1;

    write_adif_header(fp);

    int count = 0;
    for (size_t i = 0; i < list->count; ++i) {
        int should_export = 0;
        if (export_mode == EXPORT_MODE_MARKED) {
            should_export = list->items[i].marked;
        } else if (export_mode == EXPORT_MODE_ONLY_NEW) {
            should_export = (list->items[i].handled == 0);
        } else {
            should_export = 1;
        }

        if (should_export) {
            list->items[i].handled = 1;
            if (db_is_open() && list->items[i].id > 0) {
                db_set_handled(list->items[i].id, 1);
            }
            write_qso_record_adif(fp, &list->items[i], st);
            count++;
        }
    }

    fclose(fp);

    /* Spara loggen om databas inte är öppen (fallback till ADIF) */
    if (count > 0 && !db_is_open()) {
        save_all_qsos_adif(LOG_FILE, list, st);
    }

    return count;
}

/**
 * @brief Bygger standardiserat filnamn för export:
 * <stationssignal>_<YYMMDD>_<HHMMSS>[_nya].adi
 * @param out Buffer för filnamnet.
 * @param out_sz Storlek på målbuffer.
 * @param st Stationsinställningar.
 * @param only_new 1 om endast nya rader exporteras (lägger till "_nya" före filändelsen).
 */
void build_default_export_filename(char *out, size_t out_sz, const StationSettings *st, int only_new) {
    if (out == NULL || out_sz == 0) return;

    char clean_call[32];
    if (st != NULL && st->call[0] != '\0') {
        copy_str(clean_call, sizeof(clean_call), st->call);
    } else {
        copy_str(clean_call, sizeof(clean_call), "QSOLOG");
    }
    to_upper_str(clean_call);
    trim_str(clean_call);
    if (clean_call[0] == '\0') {
        copy_str(clean_call, sizeof(clean_call), "QSOLOG");
    }

    for (char *c = clean_call; *c; ++c) {
        if (!isalnum((unsigned char)*c) && *c != '-') {
            *c = '-';
        }
    }

    time_t now = time(NULL);
    struct tm exp_tm;
    long offset_sec = 0;
    if (st != NULL && parse_tz_offset_seconds(st->timezone, &offset_sec)) {
        time_t adj = now + offset_sec;
        struct tm *l = gmtime(&adj);
        if (l != NULL) exp_tm = *l;
        else exp_tm = *gmtime(&now);
    } else {
        struct tm *l = localtime(&now);
        if (l != NULL) exp_tm = *l;
        else exp_tm = *gmtime(&now);
    }

    char dt_buf[32];
    strftime(dt_buf, sizeof(dt_buf), "%y%m%d_%H%M%S", &exp_tm);

    if (only_new) {
        snprintf(out, out_sz, "%s_%s_nya.adi", clean_call, dt_buf);
    } else {
        snprintf(out, out_sz, "%s_%s.adi", clean_call, dt_buf);
    }
}

/* Fildialogen är utbruten till file_dialog.c / file_dialog.h */

/**
 * @brief Hanterar inställningsdialogen (F12) där användaren kan redigera min stations signal, namn, QTH,
 * lokator, tidszon, färgschema, autofyll samt utföra import (F5), export (F6) och loggtömning.
 * @param st Stationsinställningar.
 * @param qso Aktuellt QSO i huvudfönstret.
 * @param editing_index Index för redigerat QSO (-1 vid nytt).
 * @param log_list Logglistan.
 * @param selected_log_pos Pekare till vald rad i loggen.
 * @param log_scroll_offset Pekare till loggens rullningsposition.
 * @param status_msg Buffer för statusmeddelande.
 * @param status_sz Storlek på statusbuffert.
 */
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

            int start_y = (LINES >= 24) ? 3 : 2;
            int colon_col = 22;
            int fw = COLS - colon_col - 5;
            if (fw > 32) fw = 32;
            if (fw < 12) fw = 12;

            draw_field_aligned(start_y,     colon_col, "Signal",        edit.call,       fw, active == 0, cursor_pos, &cur_y, &cur_x);
            draw_field_aligned(start_y + 1, colon_col, "Namn",          edit.name,       fw, active == 1, cursor_pos, &cur_y, &cur_x);
            draw_field_aligned(start_y + 2, colon_col, "QTH",           edit.qth,        fw, active == 2, cursor_pos, &cur_y, &cur_x);
            draw_field_aligned(start_y + 3, colon_col, "Lokator",       edit.gridsquare, fw, active == 3, cursor_pos, &cur_y, &cur_x);
            draw_field_aligned(start_y + 4, colon_col, "Lokal tidszon", edit.timezone,   fw, active == 4, cursor_pos, &cur_y, &cur_x);

            /* Fält 5: QRZ.COM Användarnamn */
            draw_field_aligned(start_y + 5, colon_col, "QRZ.COM Användare", edit.qrz_username, fw, active == 5, cursor_pos, &cur_y, &cur_x);

            /* Fält 6: QRZ.COM Lösenord (visas i klartext vid inmatning, maskeras som ********** när sparat/inaktivt) */
            char pass_display[64] = "";
            if (active == 6) {
                copy_str(pass_display, sizeof(pass_display), edit.qrz_password);
            } else if (edit.qrz_password[0] != '\0') {
                copy_str(pass_display, sizeof(pass_display), "**********");
            }
            draw_field_aligned(start_y + 6, colon_col, "QRZ.COM Lösenord", pass_display, fw, active == 6, cursor_pos, &cur_y, &cur_x);

            int r_qrz_auto = start_y + 7;
            int r_theme    = start_y + 8;
            int r_smcb     = start_y + 9;
            int r_rst      = start_y + 10;
            int r_flt      = start_y + 11;
            int r_stats    = start_y + 12;
            int r_sw       = start_y + 13;
            int r_import   = start_y + 14;
            int r_export   = start_y + 15;
            int r_clear    = start_y + 16;

            int fcol = colon_col + 2;

            /* Fält 7: Auto QRZ.COM toggle */
            const char *lbl_qrz = "Auto QRZ.COM";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_qrz_auto, colon_col - utf8_display_width(lbl_qrz), "%s:", lbl_qrz);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            const char *qrz_txt = edit.qrz_enabled ? "På (Slå upp via qrz.com)" : "Av";
            if (active == 7) {
                attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                mvprintw(r_qrz_auto, fcol, "< %-28s >", qrz_txt);
                attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                cur_y = r_qrz_auto;
                cur_x = fcol + 2;
            } else {
                attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
                mvprintw(r_qrz_auto, fcol, "< %-28s >", qrz_txt);
                attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
            }

            /* Fält 8: Färgschema */
            const char *lbl_theme = "Färgschema";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_theme, colon_col - utf8_display_width(lbl_theme), "%s:", lbl_theme);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            if (active == 8) {
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

            /* Fält 9: SM Callbook toggle */
            const char *lbl_smcb = "SM Callbook (ssa.se)";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_smcb, colon_col - utf8_display_width(lbl_smcb), "%s:", lbl_smcb);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            const char *smcb_txt = edit.smcb_enabled ? "På (Slå upp SA-SM, 7S, 8S)" : "Av";
            if (active == 9) {
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

            /* Fält 10: Automatisk RST 59/59 vid nytt QSO */
            const char *lbl_rst = "Auto RST 59/59";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_rst, colon_col - utf8_display_width(lbl_rst), "%s:", lbl_rst);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            const char *rst_txt = edit.auto_rst_59 ? "På (Fyll i 59 / 59 vid nytt QSO)" : "Av";
            if (active == 10) {
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

            /* Fält 11: Filtrera loggrader efter anropssignal */
            const char *lbl_flt = "Filtrera loggrader";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_flt, colon_col - utf8_display_width(lbl_flt), "%s:", lbl_flt);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            const char *flt_txt = edit.filter_log ? "På" : "Av";
            if (active == 11) {
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

            /* Fält 12: Statistikruta toggle */
            const char *lbl_stats = "Statistikruta";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_stats, colon_col - utf8_display_width(lbl_stats), "%s:", lbl_stats);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            const char *stats_txt = edit.show_stats ? "På (Visa ovanför QSO)" : "Av";
            if (active == 12) {
                attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                mvprintw(r_stats, fcol, "< %-28s >", stats_txt);
                attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                cur_y = r_stats;
                cur_x = fcol + 2;
            } else {
                attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
                mvprintw(r_stats, fcol, "< %-28s >", stats_txt);
                attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
            }

            /* Fält 13: Rymdväderruta toggle */
            const char *lbl_sw = "Rymdväderruta";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_sw, colon_col - utf8_display_width(lbl_sw), "%s:", lbl_sw);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            const char *sw_txt = edit.show_space_weather ? "På (hf.avitec.se)" : "Av";
            if (active == 13) {
                attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                mvprintw(r_sw, fcol, "< %-28s >", sw_txt);
                attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                cur_y = r_sw;
                cur_x = fcol + 2;
            } else {
                attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
                mvprintw(r_sw, fcol, "< %-28s >", sw_txt);
                attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
            }

            /* Fält 14: Importera ADIF från fil (F5) */
            const char *lbl_import = "Importera ADIF (F5)";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_import, colon_col - utf8_display_width(lbl_import), "%s:", lbl_import);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            if (active == 14) {
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

            /* Fält 15: Exportera ADIF till fil (F6) */
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

            if (active == 15) {
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

            /* Fält 16: Töm loggbok och börja på nytt */
            const char *lbl_clear = "Töm loggbok";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_clear, colon_col - utf8_display_width(lbl_clear), "%s:", lbl_clear);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            if (active == 16) {
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
        } else if (ch == KEY_F(9)) {
            run_help_modal(&edit);
            apply_color_theme(edit.theme);
            continue;
        } else if (ch == KEY_F(5) || (active == 14 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' '))) {
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
        } else if (ch == KEY_F(6) || (active == 15 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' '))) {
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
        } else if (active == 16 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' ')) {
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
        } else if (ch == KEY_F(2) || ch == 19 || (active < 14 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER))) {
            to_upper_str(edit.call);
            to_upper_str(edit.gridsquare);
            *st = edit;
            apply_color_theme(st->theme);
            save_station_settings(CONFIG_FILE, st);
            copy_str(status_msg, status_sz, "Inställningar sparade.");
            return;
        } else if (ch == '\t' || ch == KEY_DOWN) {
            active = (active + 1) % STATION_FIELD_COUNT;
            cursor_pos = (active == 0) ? utf8_display_width(edit.call) :
                         (active == 1) ? utf8_display_width(edit.name) :
                         (active == 2) ? utf8_display_width(edit.qth) :
                         (active == 3) ? utf8_display_width(edit.gridsquare) :
                         (active == 4) ? utf8_display_width(edit.timezone) :
                         (active == 5) ? utf8_display_width(edit.qrz_username) :
                         (active == 6) ? utf8_display_width(edit.qrz_password) : 0;
        } else if (ch == KEY_BTAB || ch == KEY_UP) {
            active = (active + STATION_FIELD_COUNT - 1) % STATION_FIELD_COUNT;
            cursor_pos = (active == 0) ? utf8_display_width(edit.call) :
                         (active == 1) ? utf8_display_width(edit.name) :
                         (active == 2) ? utf8_display_width(edit.qth) :
                         (active == 3) ? utf8_display_width(edit.gridsquare) :
                         (active == 4) ? utf8_display_width(edit.timezone) :
                         (active == 5) ? utf8_display_width(edit.qrz_username) :
                         (active == 6) ? utf8_display_width(edit.qrz_password) : 0;
        } else if (active == 7 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')) {
            edit.qrz_enabled = !edit.qrz_enabled;
        } else if (active == 8 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')) {
            int delta = (ch == KEY_LEFT) ? -1 : 1;
            edit.theme = (edit.theme + delta + THEME_COUNT) % THEME_COUNT;
            apply_color_theme(edit.theme);
        } else if (active == 9 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')) {
            edit.smcb_enabled = !edit.smcb_enabled;
        } else if (active == 10 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')) {
            edit.auto_rst_59 = !edit.auto_rst_59;
        } else if (active == 11 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')) {
            edit.filter_log = !edit.filter_log;
        } else if (active == 12 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')) {
            edit.show_stats = !edit.show_stats;
        } else if (active == 13 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')) {
            edit.show_space_weather = !edit.show_space_weather;
        } else if (active < 7) {
            char *buf = (active == 0) ? edit.call :
                        (active == 1) ? edit.name :
                        (active == 2) ? edit.qth :
                        (active == 3) ? edit.gridsquare :
                        (active == 4) ? edit.timezone :
                        (active == 5) ? edit.qrz_username : edit.qrz_password;
            size_t max_len = (active == 0) ? sizeof(edit.call) :
                             (active == 1) ? sizeof(edit.name) :
                             (active == 2) ? sizeof(edit.qth) :
                             (active == 3) ? sizeof(edit.gridsquare) :
                             (active == 4) ? sizeof(edit.timezone) : 33;

            if (ch == KEY_LEFT) {
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
            } else if (ch >= 32 && ch != 127 && ch < KEY_MIN) {
                if ((active == 5 || active == 6) && strlen(buf) >= 32) {
                    continue;
                }
                char c = (char)ch;
                if ((active == 0 || active == 3) && (unsigned char)c < 128) {
                    c = (char)toupper((unsigned char)c);
                }
                utf8_insert_byte_at(buf, max_len, &cursor_pos, c);
            }
        }
    }
}
