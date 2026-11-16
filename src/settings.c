/**
 * @file settings.c
 * @brief Stationsinställningar, ADIF-import och -export, fildialog samt inställningsmodalen (F12).
 */

#include "settings.h"
#include "db.h"
#include "dxcc.h"
#include "logg.h"
#include "qrb.h"
#include "qrz.h"
#include "tui.h"
#include "util.h"
#include "file_dialog.h"
#include "help.h"
#include "theme_editor.h"
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
#include <wchar.h>
#include <wctype.h>

/**
 * @brief Läser in stationsinställningar från station.conf vid programstart.
 * @param filename Sökväg till konfigurationsfilen (station.conf).
 * @param st Struktur som fylls med inställningar.
 */
void load_station_settings(const char *filename, StationSettings *st) {
    memset(st, 0, sizeof(*st));
    st->smcb_enabled = 0;
    st->qrz_enabled = QRZ_MODE_OFF;
    st->qrz_log_enabled = QRZ_LOG_OFF;
    st->show_stats = 1;
    st->show_space_weather = 1;
    st->show_notes = 1;
    st->theme = THEME_DEFAULT;
    st->qrz_username[0] = '\0';
    st->qrz_password[0] = '\0';
    st->qrz_api_key[0] = '\0';
    custom_theme_init_defaults(&st->custom_theme);
    copy_str(st->last_band, sizeof(st->last_band), "20 m");
    copy_str(st->last_freq, sizeof(st->last_freq), "14.200");
    copy_str(st->last_mode, sizeof(st->last_mode), "SSB");

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) {
        set_custom_theme(&st->custom_theme);
        return;
    }

    int has_qrz_log_setting = 0;
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
        } else if (strcasecmp(key, "CUSTOM_THEME") == 0) {
            sscanf(val, "%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d",
                   &st->custom_theme.bg_main,
                   &st->custom_theme.fg_text,
                   &st->custom_theme.border,
                   &st->custom_theme.border_active,
                   &st->custom_theme.title,
                   &st->custom_theme.label,
                   &st->custom_theme.field_act_bg,
                   &st->custom_theme.field_act_fg,
                   &st->custom_theme.sel_bg,
                   &st->custom_theme.sel_fg,
                   &st->custom_theme.topbar_bg,
                   &st->custom_theme.topbar_fg,
                   &st->custom_theme.status_ok,
                   &st->custom_theme.status_warn);
        } else if (strcasecmp(key, "SMCB") == 0) {
            /* SM Callbook hålls avslaget tills vidare */
            st->smcb_enabled = 0;
        } else if (strcasecmp(key, "QRZ_USER") == 0) {
            copy_str(st->qrz_username, sizeof(st->qrz_username), val);
        } else if (strcasecmp(key, "QRZ_PASS") == 0) {
            copy_str(st->qrz_password, sizeof(st->qrz_password), val);
        } else if (strcasecmp(key, "QRZ_API_KEY") == 0) {
            copy_str(st->qrz_api_key, sizeof(st->qrz_api_key), val);
        } else if (strcasecmp(key, "QRZ_ENABLED") == 0) {
            if (strcasecmp(val, "auto") == 0 || strcmp(val, "1") == 0) {
                st->qrz_enabled = QRZ_MODE_AUTO;
            } else if (strcasecmp(val, "manual") == 0 || strcasecmp(val, "manuell") == 0 || strcmp(val, "2") == 0) {
                st->qrz_enabled = QRZ_MODE_MANUAL;
            } else {
                st->qrz_enabled = QRZ_MODE_OFF;
            }
        } else if (strcasecmp(key, "QRZ_LOG_ENABLED") == 0) {
            has_qrz_log_setting = 1;
            if (strcasecmp(val, "auto") == 0 || strcasecmp(val, "automatisk") == 0 || strcmp(val, "1") == 0) {
                st->qrz_log_enabled = QRZ_LOG_AUTO;
            } else if (strcasecmp(val, "manual") == 0 || strcasecmp(val, "manuell") == 0 || strcmp(val, "2") == 0) {
                st->qrz_log_enabled = QRZ_LOG_MANUAL;
            } else {
                st->qrz_log_enabled = QRZ_LOG_OFF;
            }
        } else if (strcasecmp(key, "AUTO_RST") == 0) {
            st->auto_rst_59 = (atoi(val) != 0) ? 1 : 0;
        } else if (strcasecmp(key, "FILTER_LOG") == 0) {
            st->filter_log = (atoi(val) != 0) ? 1 : 0;
        } else if (strcasecmp(key, "SHOW_STATS") == 0 || strcasecmp(key, "STATS") == 0) {
            st->show_stats = (atoi(val) != 0) ? 1 : 0;
        } else if (strcasecmp(key, "SHOW_SW") == 0 || strcasecmp(key, "SHOW_SPACE_WEATHER") == 0 || strcasecmp(key, "SPACE_WEATHER") == 0) {
            st->show_space_weather = (atoi(val) != 0) ? 1 : 0;
        } else if (strcasecmp(key, "SHOW_NOTES") == 0 || strcasecmp(key, "NOTES") == 0) {
            st->show_notes = (atoi(val) != 0) ? 1 : 0;
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
    if (!has_qrz_log_setting) {
        if (st->qrz_username[0] != '\0' && st->qrz_password[0] != '\0') {
            st->qrz_log_enabled = QRZ_LOG_MANUAL;
        } else {
            st->qrz_log_enabled = QRZ_LOG_OFF;
        }
    }
    set_custom_theme(&st->custom_theme);
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
    fprintf(fp, "CUSTOM_THEME=%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d,%d\n",
            st->custom_theme.bg_main,
            st->custom_theme.fg_text,
            st->custom_theme.border,
            st->custom_theme.border_active,
            st->custom_theme.title,
            st->custom_theme.label,
            st->custom_theme.field_act_bg,
            st->custom_theme.field_act_fg,
            st->custom_theme.sel_bg,
            st->custom_theme.sel_fg,
            st->custom_theme.topbar_bg,
            st->custom_theme.topbar_fg,
            st->custom_theme.status_ok,
            st->custom_theme.status_warn);
    fprintf(fp, "SMCB=%d\n", st->smcb_enabled ? 1 : 0);
    fprintf(fp, "QRZ_USER=%s\n", st->qrz_username);
    fprintf(fp, "QRZ_PASS=%s\n", st->qrz_password);
    fprintf(fp, "QRZ_API_KEY=%s\n", st->qrz_api_key);
    fprintf(fp, "QRZ_ENABLED=%d\n", st->qrz_enabled);
    fprintf(fp, "QRZ_LOG_ENABLED=%d\n", st->qrz_log_enabled);
    fprintf(fp, "AUTO_RST=%d\n", st->auto_rst_59 ? 1 : 0);
    fprintf(fp, "FILTER_LOG=%d\n", st->filter_log ? 1 : 0);
    fprintf(fp, "SHOW_STATS=%d\n", st->show_stats ? 1 : 0);
    fprintf(fp, "SHOW_SW=%d\n", st->show_space_weather ? 1 : 0);
    fprintf(fp, "SHOW_NOTES=%d\n", st->show_notes ? 1 : 0);
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
    invalidate_log_stats_cache();
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
 * @brief Importerar och slår samman ADIF-poster från en minnesbuffert till aktuell logg.
 * Sorterar listan kronologiskt och sparar till databasen.
 * @param content Pekare till minnesbuffert innehållande ADIF-data.
 * @param content_len Storlek på bufferten.
 * @param list Logglistan.
 * @param st Stationsinställningar.
 * @param mark_as_handled 1 om de importerade QSO:na ska markeras som hanterade (handled=1), 0 för nya.
 * @param progress_title Titel för förloppsrutan (eller NULL för tyst läge utan UI-uppritning).
 * @return Antal importerade poster, eller -1 vid fel.
 */
int import_qsos_from_memory(const char *content, size_t content_len, QSOList *list,
                            const StationSettings *st, int mark_as_handled,
                            const char *progress_title) {
    if (content == NULL || list == NULL) return -1;
    if (content_len == 0) return 0;

    char *unescaped_copy = NULL;
    if (strchr(content, '<') == NULL && strstr(content, "&lt;") != NULL) {
        unescaped_copy = strdup(content);
        if (unescaped_copy != NULL) {
            char *p_in = unescaped_copy;
            char *w = unescaped_copy;
            while (*p_in) {
                if (*p_in == '&') {
                    if (strncmp(p_in, "&lt;", 4) == 0) {
                        *w++ = '<'; p_in += 4;
                    } else if (strncmp(p_in, "&gt;", 4) == 0) {
                        *w++ = '>'; p_in += 4;
                    } else if (strncmp(p_in, "&amp;", 5) == 0) {
                        *w++ = '&'; p_in += 5;
                    } else if (strncmp(p_in, "&quot;", 6) == 0) {
                        *w++ = '"'; p_in += 6;
                    } else if (strncmp(p_in, "&apos;", 6) == 0) {
                        *w++ = '\''; p_in += 6;
                    } else if (strncmp(p_in, "&#039;", 6) == 0) {
                        *w++ = '\''; p_in += 6;
                    } else if (strncmp(p_in, "&#39;", 5) == 0) {
                        *w++ = '\''; p_in += 5;
                    } else {
                        *w++ = *p_in++;
                    }
                } else {
                    *w++ = *p_in++;
                }
            }
            *w = '\0';
            content = unescaped_copy;
            content_len = strlen(unescaped_copy);
        }
    }

    const char *p = content;
    const char *cursor = content;
    while ((cursor = strchr(cursor, '<')) != NULL) {
        if (strncasecmp(cursor, "<EOH>", 5) == 0) {
            p = cursor + 5;
            break;
        }
        cursor++;
    }
    if (p == NULL) p = content;

    int imported_count = 0;
    QSO current;
    memset(&current, 0, sizeof(current));

    int lines_processed = 0;
    const char *line_scan = content;
    time_t last_status_update = 0;

    while ((p = strchr(p, '<')) != NULL) {
        while (line_scan < p) {
            if (*line_scan == '\n') lines_processed++;
            line_scan++;
        }

        time_t now = time(NULL);
        if (progress_title != NULL && now != last_status_update) {
            last_status_update = now;
            int percent = (content_len > 0) ? (int)(((size_t)(p - content) * 100) / content_len) : 0;
            if (percent > 100) percent = 100;
            if (stdscr != NULL && !isendwin()) {
                int box_w = 46;
                int box_h = 5;
                int box_y = (LINES - box_h) / 2;
                int box_x = (COLS - box_w) / 2;
                if (box_y < 0) box_y = 0;
                if (box_x < 0) box_x = 0;
                attron(COLOR_PAIR(CP_BORDER_ACTIVE) | A_BOLD);
                draw_box_with_title(box_y, box_x, box_h, box_w, progress_title, 1);
                char stat_str[64];
                snprintf(stat_str, sizeof(stat_str), "Processat: %d rader (%d%%)", lines_processed, percent);
                int slen = utf8_display_width(stat_str);
                mvprintw(box_y + 2, box_x + (box_w - slen) / 2, "%s", stat_str);
                attroff(COLOR_PAIR(CP_BORDER_ACTIVE) | A_BOLD);
                refresh();
            }
        }

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
                if (mark_as_handled) {
                    current.handled = 1;
                }
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
        if (mark_as_handled) {
            current.handled = 1;
        }
        if (!qso_is_duplicate(list, &current)) {
            qso_list_push(list, &current);
            imported_count++;
        }
    }

    if (progress_title != NULL && stdscr != NULL && !isendwin()) {
        while (line_scan < content + content_len) {
            if (*line_scan == '\n') lines_processed++;
            line_scan++;
        }
        int box_w = 46;
        int box_h = 5;
        int box_y = (LINES - box_h) / 2;
        int box_x = (COLS - box_w) / 2;
        if (box_y < 0) box_y = 0;
        if (box_x < 0) box_x = 0;
        attron(COLOR_PAIR(CP_BORDER_ACTIVE) | A_BOLD);
        draw_box_with_title(box_y, box_x, box_h, box_w, progress_title, 1);
        char stat_str[64];
        snprintf(stat_str, sizeof(stat_str), "Processat: %d rader (100%%)", lines_processed);
        int slen = utf8_display_width(stat_str);
        mvprintw(box_y + 2, box_x + (box_w - slen) / 2, "%s", stat_str);
        attroff(COLOR_PAIR(CP_BORDER_ACTIVE) | A_BOLD);
        refresh();
    }

    if (imported_count > 0) {
        qsort(list->items, list->count, sizeof(QSO), cmp_qso_chronological);
        if (db_is_open()) {
            db_clear_all_qsos();
            db_batch_insert(list->items, list->count);
        } else {
            save_all_qsos_adif(LOG_FILE, list, st);
        }
    }

    if (unescaped_copy != NULL) {
        free(unescaped_copy);
    }

    return imported_count;
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

    int res = import_qsos_from_memory(content, read_bytes, list, st, 0, "ADIF-import");
    free(content);
    return res;
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
    memset(&exp_tm, 0, sizeof(exp_tm));
    long offset_sec = 0;
    if (st != NULL && parse_tz_offset_seconds(st->timezone, &offset_sec)) {
        time_t adj = now + offset_sec;
        if (!gmtime_r(&adj, &exp_tm)) {
            gmtime_r(&now, &exp_tm);
        }
    } else {
        if (!localtime_r(&now, &exp_tm)) {
            gmtime_r(&now, &exp_tm);
        }
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
 * @brief Ritar inställningsfönstret (F12) med alla fält och knappar.
 */
static void draw_settings_dialog_view(const StationSettings *edit, int active, int cursor_pos,
                                      const QSOList *log_list, const char *status_msg,
                                      int *cur_y, int *cur_x) {
    int dummy_y = 3, dummy_x = 24;
    if (!cur_y) cur_y = &dummy_y;
    if (!cur_x) cur_x = &dummy_x;

    erase();

    if (COLS < MIN_COLS || LINES < MIN_LINES) {
        mvprintw(LINES / 2, 2, "Minsta fönsterstorlek är %dx%d.", MIN_COLS, MIN_LINES);
        return;
    }

    draw_top_bar(edit);
    draw_box_with_title(1, 0, LINES - 2, COLS, "Inställningar – Min Station (F12)", 1);

    int start_y = (LINES >= 26) ? 3 : 2;
    int colon_col = 22;
    int fw = COLS - colon_col - 5;
    if (fw > 32) fw = 32;
    if (fw < 12) fw = 12;

    draw_field_aligned(start_y,     colon_col, "Signal",        edit->call,       fw, active == 0, cursor_pos, cur_y, cur_x);
    draw_field_aligned(start_y + 1, colon_col, "Namn",          edit->name,       fw, active == 1, cursor_pos, cur_y, cur_x);
    draw_field_aligned(start_y + 2, colon_col, "QTH",           edit->qth,        fw, active == 2, cursor_pos, cur_y, cur_x);
    draw_field_aligned(start_y + 3, colon_col, "Lokator",       edit->gridsquare, fw, active == 3, cursor_pos, cur_y, cur_x);
    draw_field_aligned(start_y + 4, colon_col, "Lokal tidszon", edit->timezone,   fw, active == 4, cursor_pos, cur_y, cur_x);

    /* Fält 5: QRZ.COM Användarnamn */
    draw_field_aligned(start_y + 5, colon_col, "QRZ.COM Användare", edit->qrz_username, fw, active == 5, cursor_pos, cur_y, cur_x);

    /* Fält 6: QRZ.COM Lösenord (visas i klartext vid inmatning, maskeras som ********** när sparat/inaktivt) */
    char pass_display[64] = "";
    if (active == 6) {
        copy_str(pass_display, sizeof(pass_display), edit->qrz_password);
    } else if (edit->qrz_password[0] != '\0') {
        copy_str(pass_display, sizeof(pass_display), "**********");
    }
    draw_field_aligned(start_y + 6, colon_col, "QRZ.COM Lösenord", pass_display, fw, active == 6, cursor_pos, cur_y, cur_x);

    /* Fält 7: QRZ Logbook API-nyckel (visas i klartext vid inmatning, maskeras som ********** när inaktivt) */
    char key_display[64] = "";
    if (active == 7) {
        copy_str(key_display, sizeof(key_display), edit->qrz_api_key);
    } else if (edit->qrz_api_key[0] != '\0') {
        copy_str(key_display, sizeof(key_display), "**********");
    }
    draw_field_aligned(start_y + 7, colon_col, "QRZ API-nyckel", key_display, fw, active == 7, cursor_pos, cur_y, cur_x);

    int r_qrz_auto = start_y + 8;
    int r_qrz_log  = start_y + 9;
    int r_theme    = start_y + 10;
    int r_rst      = start_y + 11;
    int r_flt      = start_y + 12;
    int r_stats    = start_y + 13;
    int r_sw       = start_y + 14;
    int r_import   = start_y + 15;
    int r_export   = start_y + 16;
    int r_sync     = start_y + 17;
    int r_clear    = start_y + 18;

    int fcol = colon_col + 2;

    /* Fält 8: Auto QRZ.COM toggle (Av / Auto / Manuell) */
    const char *lbl_qrz = "Auto QRZ.COM";
    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(r_qrz_auto, colon_col - utf8_display_width(lbl_qrz), "%s:", lbl_qrz);
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

    const char *qrz_txt = (edit->qrz_enabled == QRZ_MODE_AUTO)   ? "Auto" :
                          (edit->qrz_enabled == QRZ_MODE_MANUAL) ? "Manuell" : "Av";
    if (active == 8) {
        attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        mvprintw(r_qrz_auto, fcol, "< %-28s >", qrz_txt);
        attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        *cur_y = r_qrz_auto;
        *cur_x = fcol + 2;
    } else {
        attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
        mvprintw(r_qrz_auto, fcol, "< %-28s >", qrz_txt);
        attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
    }

    /* Fält 9: Direktlogg QRZ.COM toggle (Av / Automatisk / Manuell) */
    const char *lbl_qrz_log = "Direktlogg QRZ.COM";
    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(r_qrz_log, colon_col - utf8_display_width(lbl_qrz_log), "%s:", lbl_qrz_log);
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

    const char *qrz_log_txt = (edit->qrz_log_enabled == QRZ_LOG_AUTO)   ? "Automatisk" :
                              (edit->qrz_log_enabled == QRZ_LOG_MANUAL) ? "Manuell" : "Av";
    if (active == 9) {
        attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        mvprintw(r_qrz_log, fcol, "< %-28s >", qrz_log_txt);
        attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        *cur_y = r_qrz_log;
        *cur_x = fcol + 2;
    } else {
        attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
        mvprintw(r_qrz_log, fcol, "< %-28s >", qrz_log_txt);
        attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
    }

    /* Fält 10: Färgschema (Drop-down lista) */
    const char *lbl_theme = "Färgschema";
    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(r_theme, colon_col - utf8_display_width(lbl_theme), "%s:", lbl_theme);
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

    int t_idx = (edit->theme >= 0 && edit->theme < THEME_COUNT) ? edit->theme : THEME_DEFAULT;
    char theme_disp[64];
    utf8_pad_or_clip(theme_disp, sizeof(theme_disp), THEME_NAMES[t_idx], 26, 0);

    if (active == 10) {
        attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        mvprintw(r_theme, fcol, "[ %s v ]", theme_disp);
        attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        *cur_y = r_theme;
        *cur_x = fcol + 2;
    } else {
        attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
        mvprintw(r_theme, fcol, "[ %s v ]", theme_disp);
        attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
    }

    if (COLS >= 78) {
        if (active == 10) {
            attron(COLOR_PAIR(CP_TOPBAR) | A_BOLD);
            mvprintw(r_theme, fcol + 32, "[ F7: Egen färgprofil... ]");
            attroff(COLOR_PAIR(CP_TOPBAR) | A_BOLD);
        } else {
            attron(COLOR_PAIR(CP_BORDER));
            mvprintw(r_theme, fcol + 32, "[ F7: Egen färgprofil... ]");
            attroff(COLOR_PAIR(CP_BORDER));
        }
    }

    /* Fält 11: Automatisk RST 59/59 vid nytt QSO */
    const char *lbl_rst = "Auto RST 59/59";
    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(r_rst, colon_col - utf8_display_width(lbl_rst), "%s:", lbl_rst);
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

    const char *rst_txt = edit->auto_rst_59 ? "På (Fyll i 59 / 59 vid nytt QSO)" : "Av";
    if (active == 11) {
        attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        mvprintw(r_rst, fcol, "< %-28s >", rst_txt);
        attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        *cur_y = r_rst;
        *cur_x = fcol + 2;
    } else {
        attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
        mvprintw(r_rst, fcol, "< %-28s >", rst_txt);
        attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
    }

    /* Fält 12: Filtrera loggrader efter anropssignal */
    const char *lbl_flt = "Filtrera loggrader";
    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(r_flt, colon_col - utf8_display_width(lbl_flt), "%s:", lbl_flt);
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

    const char *flt_txt = edit->filter_log ? "På" : "Av";
    if (active == 12) {
        attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        mvprintw(r_flt, fcol, "< %-28s >", flt_txt);
        attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        *cur_y = r_flt;
        *cur_x = fcol + 2;
    } else {
        attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
        mvprintw(r_flt, fcol, "< %-28s >", flt_txt);
        attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
    }

    /* Fält 13: Statistikruta toggle */
    const char *lbl_stats = "Statistikruta";
    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(r_stats, colon_col - utf8_display_width(lbl_stats), "%s:", lbl_stats);
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

    const char *stats_txt = edit->show_stats ? "På (Visa ovanför QSO)" : "Av";
    if (active == 13) {
        attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        mvprintw(r_stats, fcol, "< %-28s >", stats_txt);
        attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        *cur_y = r_stats;
        *cur_x = fcol + 2;
    } else {
        attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
        mvprintw(r_stats, fcol, "< %-28s >", stats_txt);
        attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
    }

    /* Fält 14: Rymdväderruta toggle */
    const char *lbl_sw = "Rymdväderruta";
    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(r_sw, colon_col - utf8_display_width(lbl_sw), "%s:", lbl_sw);
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

    const char *sw_txt = edit->show_space_weather ? "På (hf.avitec.se)" : "Av";
    if (active == 14) {
        attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        mvprintw(r_sw, fcol, "< %-28s >", sw_txt);
        attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        *cur_y = r_sw;
        *cur_x = fcol + 2;
    } else {
        attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
        mvprintw(r_sw, fcol, "< %-28s >", sw_txt);
        attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
    }

    /* Fält 15: Importera ADIF från fil (F5) */
    const char *lbl_import = "Importera ADIF (F5)";
    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(r_import, colon_col - utf8_display_width(lbl_import), "%s:", lbl_import);
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

    if (active == 15) {
        attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        mvprintw(r_import, fcol, "[ Tryck F5 eller Enter för att bläddra... ]");
        attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        *cur_y = r_import;
        *cur_x = fcol + 2;
    } else {
        attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
        mvprintw(r_import, fcol, "[ Tryck F5 eller Enter för att bläddra... ]");
        attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
    }

    /* Fält 16: Exportera ADIF till fil (F6) */
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

    if (active == 16) {
        attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        mvprintw(r_export, fcol, "%-s", export_hint);
        attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        *cur_y = r_export;
        *cur_x = fcol + 2;
    } else {
        attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
        mvprintw(r_export, fcol, "%-s", export_hint);
        attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
    }

    /* Fält 17: Synka nya QSO från QRZ.COM (F8) */
    const char *lbl_sync = "Synka QRZ.COM (F8)";
    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(r_sync, colon_col - utf8_display_width(lbl_sync), "%s:", lbl_sync);
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

    const char *sync_hint = "[ F8 / Enter: Hämta nya QSO från QRZ.COM ]";
    if (active == 17) {
        attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        mvprintw(r_sync, fcol, "%-s", sync_hint);
        attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        *cur_y = r_sync;
        *cur_x = fcol + 2;
    } else {
        attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
        mvprintw(r_sync, fcol, "%-s", sync_hint);
        attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
    }

    /* Fält 18: Töm loggbok och börja på nytt */
    const char *lbl_clear = "Töm loggbok";
    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(r_clear, colon_col - utf8_display_width(lbl_clear), "%s:", lbl_clear);
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

    if (active == 18) {
        attron(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);
        mvprintw(r_clear, fcol, "[ Tryck Enter för att tömma ]");
        attroff(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);
        *cur_y = r_clear;
        *cur_x = fcol + 2;
    } else {
        attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
        mvprintw(r_clear, fcol, "[ Tryck Enter för att tömma ]");
        attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
    }

    if (status_msg != NULL && status_msg[0] != '\0' && LINES >= 20) {
        attron(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);
        mvprintw(LINES - 3, 3, "Status: %-s", status_msg);
        attroff(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);
    }

    if (LINES >= 17) {
        if (active == 10) {
            mvprintw(LINES - 2, 3, "Enter: Välj tema. F7 / E: Redigera egen profil. F2/F12: Spara.");
        } else {
            mvprintw(LINES - 2, 3, "Tab/Pilar: Byt fält. F5=Import, F6=Export, F8=QRZ-synk. F2/F12: Spara.");
        }
    }

    draw_bottom_fkey_bar(1, FOCUS_INPUT, NULL);
    curs_set(1);
    move(*cur_y, *cur_x);
}

/**
 * @brief Visar en interaktiv rullgardinsmeny (drop-down lista) för val av färgschema (THEME_COUNT teman).
 * Förhandsgranskar schemat i realtid vid navigering. Bekräfta med Enter/Mellanslag, avbryt med Esc.
 * @param edit Pekare till stationsinställningar som uppdateras vid val.
 * @param log_list Logglistan för bakgrundsritning.
 * @param status_msg Statusmeddelande.
 * @return 0 vid Esc/avbryt, 1 vid bekräftat val (Enter/Mellanslag), 2 vid Tab, 3 vid Shift-Tab.
 */
static int run_theme_dropdown(StationSettings *edit, const QSOList *log_list, const char *status_msg) {
    int orig_theme = edit->theme;
    if (orig_theme < 0 || orig_theme >= THEME_COUNT) {
        orig_theme = THEME_DEFAULT;
    }
    int sel_idx = orig_theme;

    int start_y = (LINES >= 26) ? 3 : 2;
    int colon_col = 22;
    int fcol = colon_col + 2;
    int r_theme = start_y + 10;

    int list_w = 35;
    int pop_x = fcol;
    if (pop_x + list_w >= COLS) {
        pop_x = COLS - list_w - 1;
    }
    if (pop_x < 1) pop_x = 1;

    int pop_y = r_theme + 1;
    int avail_h = (LINES - 2) - pop_y;
    int visible_items = THEME_COUNT;
    if (visible_items + 2 > avail_h) {
        if (LINES >= THEME_COUNT + 4) {
            pop_y = (LINES - 2) - (THEME_COUNT + 2);
            if (pop_y < 2) pop_y = 2;
            avail_h = (LINES - 2) - pop_y;
            visible_items = THEME_COUNT;
        } else {
            visible_items = avail_h - 2;
            if (visible_items < 4) visible_items = 4;
            if (visible_items > THEME_COUNT) visible_items = THEME_COUNT;
        }
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
        if (scroll_offset + visible_items > THEME_COUNT) {
            scroll_offset = THEME_COUNT - visible_items;
            if (scroll_offset < 0) scroll_offset = 0;
        }

        draw_settings_dialog_view(edit, 10, 0, log_list, status_msg, NULL, NULL);
        curs_set(0);

        /* Rita popup-ram */
        draw_box_with_title(pop_y, pop_x, list_h, list_w, "Färgscheman", 1);

        if (scroll_offset > 0) {
            mvprintw(pop_y, pop_x + list_w - 2, "^");
        }
        if (scroll_offset + visible_items < THEME_COUNT) {
            mvprintw(pop_y + list_h - 1, pop_x + list_w - 2, "v");
        }

        for (int i = 0; i < visible_items; ++i) {
            int item_idx = scroll_offset + i;
            if (item_idx >= THEME_COUNT) break;
            int r = pop_y + 1 + i;
            int is_sel = (item_idx == sel_idx);

            char item_buf[64];
            utf8_pad_or_clip(item_buf, sizeof(item_buf), THEME_NAMES[item_idx], 27, 0);

            if (is_sel) {
                attron(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
                mvprintw(r, pop_x + 1, " > %s < ", item_buf);
                attroff(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
            } else {
                attron(COLOR_PAIR(CP_FIELD_INACTIVE));
                mvprintw(r, pop_x + 1, "   %s   ", item_buf);
                attroff(COLOR_PAIR(CP_FIELD_INACTIVE));
            }
        }

        refresh();

        wint_t wch = 0;
        int input_res = get_wch(&wch);
        int ch = (input_res == ERR) ? ERR : (int)wch;
        if (ch == ERR) continue;

        if (ch == 27) {
            edit->theme = orig_theme;
            apply_color_theme(orig_theme);
            curs_set(1);
            return 0;
        } else if (ch == KEY_F(7) || ch == 'e' || ch == 'E') {
            int changed = run_color_theme_editor(edit);
            if (changed) {
                edit->theme = THEME_CUSTOM;
                set_custom_theme(&edit->custom_theme);
                apply_color_theme(THEME_CUSTOM);
                curs_set(1);
                return 1;
            } else {
                apply_color_theme(sel_idx);
                curs_set(0);
            }
        } else if (ch == KEY_F(12)) {
            edit->theme = sel_idx;
            apply_color_theme(sel_idx);
            curs_set(1);
            return 4;
        } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' ') {
            edit->theme = sel_idx;
            apply_color_theme(sel_idx);
            curs_set(1);
            return 1;
        } else if (ch == '\t') {
            edit->theme = sel_idx;
            apply_color_theme(sel_idx);
            curs_set(1);
            return 2;
        } else if (ch == KEY_BTAB) {
            edit->theme = sel_idx;
            apply_color_theme(sel_idx);
            curs_set(1);
            return 3;
        } else if (ch == KEY_UP || ch == 'k' || ch == 'K') {
            if (sel_idx > 0) sel_idx--;
            else sel_idx = THEME_COUNT - 1;
            apply_color_theme(sel_idx);
        } else if (ch == KEY_DOWN || ch == 'j' || ch == 'J') {
            if (sel_idx + 1 < THEME_COUNT) sel_idx++;
            else sel_idx = 0;
            apply_color_theme(sel_idx);
        } else if (ch == KEY_HOME) {
            sel_idx = 0;
            apply_color_theme(sel_idx);
        } else if (ch == KEY_END) {
            sel_idx = THEME_COUNT - 1;
            apply_color_theme(sel_idx);
        } else if (ch == KEY_PPAGE) {
            sel_idx -= visible_items;
            if (sel_idx < 0) sel_idx = 0;
            apply_color_theme(sel_idx);
        } else if (ch == KEY_NPAGE) {
            sel_idx += visible_items;
            if (sel_idx >= THEME_COUNT) sel_idx = THEME_COUNT - 1;
            apply_color_theme(sel_idx);
        } else if (ch >= '1' && ch <= '9') {
            int target = ch - '1';
            if (target < THEME_COUNT) {
                sel_idx = target;
                apply_color_theme(sel_idx);
            }
        }
    }
}

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
void run_settings_modal(StationSettings *st, QSO *qso, int *editing_index,
                        QSOList *log_list, int *selected_log_pos, int *log_scroll_offset,
                        char *status_msg, size_t status_sz) {
    StationSettings edit = *st;
    int had_qrz_creds = (st->qrz_username[0] != '\0' && st->qrz_password[0] != '\0');
    int user_changed_qrz_log = 0;
    int active = 0;
    int cursor_pos = utf8_display_width(edit.call);

    while (1) {
        int cur_y = 3, cur_x = 24;
        draw_settings_dialog_view(&edit, active, cursor_pos, log_list, status_msg, &cur_y, &cur_x);
        refresh();

        wint_t wch = 0;
        int input_res = get_wch(&wch);
        int ch = (input_res == ERR) ? ERR : (int)wch;
        if (ch == ERR) {
            continue;
        } else if (ch == KEY_F(12)) {
            to_upper_str(edit.call);
            to_upper_str(edit.gridsquare);
            *st = edit;
            apply_color_theme(st->theme);
            save_station_settings(CONFIG_FILE, st);
            copy_str(status_msg, status_sz, "Inställningar sparade.");
            return;
        } else if (ch == 27) {
            apply_color_theme(st->theme);
            copy_str(status_msg, status_sz, "Inställningar stängda utan att spara.");
            return;
        } else if (ch == KEY_F(9)) {
            run_help_modal(&edit);
            apply_color_theme(edit.theme);
            continue;
        } else if (ch == KEY_F(7) || (active == 10 && (ch == 'e' || ch == 'E'))) {
            int changed = run_color_theme_editor(&edit);
            if (changed) {
                edit.theme = THEME_CUSTOM;
                set_custom_theme(&edit.custom_theme);
                apply_color_theme(THEME_CUSTOM);
                copy_str(status_msg, status_sz, "Egen färgprofil sparad och aktiverad!");
            } else {
                apply_color_theme(edit.theme);
            }
            continue;
        } else if (ch == KEY_F(5) || (active == 15 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' '))) {
            char chosen_path[PATH_MAX] = {0};
            int chosen = run_file_dialog("Välj ADIF-fil att importera", ".", ".adi;.adif", chosen_path, sizeof(chosen_path), 0, NULL);
            if (chosen && chosen_path[0] != '\0') {
                QSO orig_edited;
                int had_edit = 0;
                if (editing_index && *editing_index >= 0 && log_list && (size_t)*editing_index < log_list->count) {
                    orig_edited = log_list->items[*editing_index];
                    had_edit = 1;
                }
                int added = import_qsos_from_adif(chosen_path, log_list, &edit);
                if (added < 0) {
                    snprintf(status_msg, status_sz, "Kunde inte läsa filen: %s", get_path_basename(chosen_path));
                } else if (added == 0) {
                    snprintf(status_msg, status_sz, "Import klar: Inga nya QSO hittades i %s.", get_path_basename(chosen_path));
                } else {
                    snprintf(status_msg, status_sz, "Importerade %d QSO från %s!", added, get_path_basename(chosen_path));
                    if (had_edit && editing_index && log_list) {
                        int found_new_idx = -1;
                        for (size_t k = 0; k < log_list->count; ++k) {
                            const QSO *item = &log_list->items[k];
                            if (strcasecmp(item->call, orig_edited.call) == 0 &&
                                strcmp(item->date, orig_edited.date) == 0 &&
                                strcmp(item->time_utc, orig_edited.time_utc) == 0 &&
                                strcasecmp(item->band, orig_edited.band) == 0) {
                                found_new_idx = (int)k;
                                break;
                            }
                        }
                        *editing_index = found_new_idx;
                        if (found_new_idx >= 0 && qso) {
                            qso->id = log_list->items[found_new_idx].id;
                        }
                    }
                    if (selected_log_pos && log_list && log_list->count > 0) {
                        *selected_log_pos = (int)log_list->count - 1;
                    }
                }
            } else {
                copy_str(status_msg, status_sz, "ADIF-import avbröts.");
            }
            apply_color_theme(edit.theme);
            continue;
        } else if (ch == KEY_F(6) || (active == 16 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' '))) {
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
                    if (editing_index && *editing_index >= 0 && (size_t)*editing_index < log_list->count) {
                        qso->handled = log_list->items[*editing_index].handled;
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
        } else if (ch == KEY_F(8) || (active == 17 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' '))) {
            if (edit.qrz_api_key[0] == '\0') {
                copy_str(status_msg, status_sz, "QRZ API-nyckel saknas i inställningar.");
                apply_color_theme(edit.theme);
                continue;
            }

            if (stdscr != NULL && !isendwin()) {
                int box_w = 46;
                int box_h = 5;
                int box_y = (LINES - box_h) / 2;
                int box_x = (COLS - box_w) / 2;
                if (box_y < 0) box_y = 0;
                if (box_x < 0) box_x = 0;
                attron(COLOR_PAIR(CP_BORDER_ACTIVE) | A_BOLD);
                draw_box_with_title(box_y, box_x, box_h, box_w, "QRZ.COM Synk", 1);
                const char *stat_str = "Hämtar loggbok från QRZ.COM...";
                int slen = utf8_display_width(stat_str);
                mvprintw(box_y + 2, box_x + (box_w - slen) / 2, "%s", stat_str);
                attroff(COLOR_PAIR(CP_BORDER_ACTIVE) | A_BOLD);
                refresh();
            }

            QSO orig_edited;
            int had_edit = 0;
            if (editing_index && *editing_index >= 0 && log_list && (size_t)*editing_index < log_list->count) {
                orig_edited = log_list->items[*editing_index];
                had_edit = 1;
            }

            char err_buf[256] = "";
            char *adif_data = NULL;
            size_t adif_sz = 0;
            int fetch_ok = qrz_logbook_fetch(edit.qrz_api_key, &adif_data, &adif_sz, err_buf, sizeof(err_buf));
            if (!fetch_ok) {
                snprintf(status_msg, status_sz, "QRZ-synk misslyckades: %s", err_buf[0] ? err_buf : "Okänt fel");
            } else {
                int added = import_qsos_from_memory(adif_data ? adif_data : "", adif_sz, log_list, &edit, 1, "Synkar från QRZ.COM");
                if (added < 0) {
                    copy_str(status_msg, status_sz, "Kunde inte tolka data från QRZ.COM.");
                } else if (added == 0) {
                    copy_str(status_msg, status_sz, "Synk klar: Inga nya QSO hittades på QRZ.COM.");
                } else {
                    snprintf(status_msg, status_sz, "Synkroniserade %d nya QSO från QRZ.COM!", added);
                    if (had_edit && editing_index && log_list) {
                        int found_new_idx = -1;
                        for (size_t k = 0; k < log_list->count; ++k) {
                            const QSO *item = &log_list->items[k];
                            if (strcasecmp(item->call, orig_edited.call) == 0 &&
                                strcmp(item->date, orig_edited.date) == 0 &&
                                strcmp(item->time_utc, orig_edited.time_utc) == 0 &&
                                strcasecmp(item->band, orig_edited.band) == 0) {
                                found_new_idx = (int)k;
                                break;
                            }
                        }
                        *editing_index = found_new_idx;
                        if (found_new_idx >= 0 && qso) {
                            qso->id = log_list->items[found_new_idx].id;
                        }
                    }
                    if (selected_log_pos && log_list && log_list->count > 0) {
                        *selected_log_pos = (int)log_list->count - 1;
                    }
                }
            }
            if (adif_data) free(adif_data);
            apply_color_theme(edit.theme);
            continue;
        } else if (active == 18 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' ')) {
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
                if (editing_index) *editing_index = -1;
                if (selected_log_pos) *selected_log_pos = 0;
                if (log_scroll_offset) *log_scroll_offset = 0;
                apply_color_theme(st->theme);
                copy_str(status_msg, status_sz, "Loggboken har tömts!");
                return;
            } else {
                apply_color_theme(st->theme);
            }
        } else if (ch == KEY_F(2) || ch == 19 || (active < 15 && active != 10 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER))) {
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
                         (active == 6) ? utf8_display_width(edit.qrz_password) :
                         (active == 7) ? utf8_display_width(edit.qrz_api_key) : 0;
        } else if (ch == KEY_BTAB || ch == KEY_UP) {
            active = (active + STATION_FIELD_COUNT - 1) % STATION_FIELD_COUNT;
            cursor_pos = (active == 0) ? utf8_display_width(edit.call) :
                         (active == 1) ? utf8_display_width(edit.name) :
                         (active == 2) ? utf8_display_width(edit.qth) :
                         (active == 3) ? utf8_display_width(edit.gridsquare) :
                         (active == 4) ? utf8_display_width(edit.timezone) :
                         (active == 5) ? utf8_display_width(edit.qrz_username) :
                         (active == 6) ? utf8_display_width(edit.qrz_password) :
                         (active == 7) ? utf8_display_width(edit.qrz_api_key) : 0;
        } else if (active == 8 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')) {
            if (ch == KEY_LEFT) {
                edit.qrz_enabled = (edit.qrz_enabled + 2) % 3;
            } else {
                edit.qrz_enabled = (edit.qrz_enabled + 1) % 3;
            }
        } else if (active == 9 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')) {
            user_changed_qrz_log = 1;
            if (ch == KEY_LEFT) {
                edit.qrz_log_enabled = (edit.qrz_log_enabled + 2) % 3;
            } else {
                edit.qrz_log_enabled = (edit.qrz_log_enabled + 1) % 3;
            }
        } else if (active == 10 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' ' || ch == KEY_LEFT || ch == KEY_RIGHT)) {
            int ret = run_theme_dropdown(&edit, log_list, status_msg);
            if (ret == 4) {
                to_upper_str(edit.call);
                to_upper_str(edit.gridsquare);
                *st = edit;
                apply_color_theme(st->theme);
                save_station_settings(CONFIG_FILE, st);
                copy_str(status_msg, status_sz, "Inställningar sparade.");
                return;
            } else if (ret == 2) {
                active = (active + 1) % STATION_FIELD_COUNT;
            } else if (ret == 3) {
                active = (active + STATION_FIELD_COUNT - 1) % STATION_FIELD_COUNT;
            }
        } else if (active == 11 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')) {
            edit.auto_rst_59 = !edit.auto_rst_59;
        } else if (active == 12 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')) {
            edit.filter_log = !edit.filter_log;
        } else if (active == 13 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')) {
            edit.show_stats = !edit.show_stats;
        } else if (active == 14 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')) {
            edit.show_space_weather = !edit.show_space_weather;
        } else if (active < 8) {
            char *buf = (active == 0) ? edit.call :
                        (active == 1) ? edit.name :
                        (active == 2) ? edit.qth :
                        (active == 3) ? edit.gridsquare :
                        (active == 4) ? edit.timezone :
                        (active == 5) ? edit.qrz_username :
                        (active == 6) ? edit.qrz_password : edit.qrz_api_key;
            size_t max_len = (active == 0) ? sizeof(edit.call) :
                             (active == 1) ? sizeof(edit.name) :
                             (active == 2) ? sizeof(edit.qth) :
                             (active == 3) ? sizeof(edit.gridsquare) :
                             (active == 4) ? sizeof(edit.timezone) :
                             (active == 7) ? sizeof(edit.qrz_api_key) : 33;

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
            } else if (input_res == OK && wch >= 32 && wch != 127) {
                if ((active == 5 || active == 6) && strlen(buf) >= 32) {
                    continue;
                }
                if (active == 0 || active == 3) {
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
                utf8_insert_char_at(buf, max_len, &cursor_pos, u8, u8_len);
                if ((active == 5 || active == 6) && !had_qrz_creds && !user_changed_qrz_log &&
                    edit.qrz_username[0] != '\0' && edit.qrz_password[0] != '\0' &&
                    edit.qrz_log_enabled == QRZ_LOG_OFF) {
                    edit.qrz_log_enabled = QRZ_LOG_MANUAL;
                }
            }
        }
    }
}
