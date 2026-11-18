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
    copy_str(st->db_name, sizeof(st->db_name), DEFAULT_LOG_NAME);

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
        } else if ((strcasecmp(key, "DB_FILE") == 0 || strcasecmp(key, "DATABASE") == 0) && val[0] != '\0') {
            normalize_db_name(val, st->db_name, sizeof(st->db_name));
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
    const char *app_dir = get_app_data_dir();
    if (filename != NULL && strncmp(filename, app_dir, strlen(app_dir)) == 0) {
        ensure_app_data_dir();
    }
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
    fprintf(fp, "DB_FILE=%s\n", st->db_name[0] ? st->db_name : DEFAULT_LOG_NAME);
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
            xml_unescape_str(unescaped_copy);
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
            char stat_str[64];
            snprintf(stat_str, sizeof(stat_str), "Processat: %d rader (%d%%)", lines_processed, percent);
            draw_progress_modal(progress_title, stat_str);
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

            char val_buf[512];
            size_t consumed = adif_extract_field_value(val_start, val_len, val_buf, sizeof(val_buf));

            assign_adif_field(&current, tag_buf, val_buf);
            p = val_start + consumed;
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
        char stat_str[64];
        snprintf(stat_str, sizeof(stat_str), "Processat: %d rader (100%%)", lines_processed);
        draw_progress_modal(progress_title, stat_str);
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

static void write_csv_cell(FILE *fp, const char *val, int is_first) {
    if (!is_first) {
        fputc(',', fp);
    }
    if (val == NULL || val[0] == '\0') {
        return;
    }
    fputc('"', fp);
    for (const char *p = val; *p != '\0'; ++p) {
        if (*p == '"') {
            fputc('"', fp);
            fputc('"', fp);
        } else {
            fputc(*p, fp);
        }
    }
    fputc('"', fp);
}

/**
 * @brief Exporterar QSO till en extern CSV-fil (Excel, Google Docs, LibreOffice).
 * Skriver UTF-8 BOM för korrekt å, ä, ö-visning i kalkylprogram och fältnamn på första raden.
 * @param filename Sökvägen till målfilen.
 * @param list Logglistan.
 * @param st Stationsinställningar.
 * @param export_mode Exportläge (EXPORT_MODE_ALL, EXPORT_MODE_MARKED, EXPORT_MODE_ONLY_NEW).
 * @return Antal exporterade poster, eller -1 vid fel.
 */
int export_qsos_csv(const char *filename, QSOList *list, const StationSettings *st, int export_mode) {
    if (filename == NULL || filename[0] == '\0' || list == NULL) return -1;
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) return -1;

    /* Skriv UTF-8 BOM (\xEF\xBB\xBF) så att Excel, Google Docs och LibreOffice automatiskt läser UTF-8 */
    fputs("\xEF\xBB\xBF", fp);

    /* Första raden: Fältnamnen i samma ordning som de visas i loggraderna */
    fputs("#,Datum,Tid,Signal,Frekvens,Band,Mode,RST,Namn,QTH,QRB,QUJ,Lokator,DXCC,Notering\r\n", fp);

    int count = 0;
    for (size_t i = 0; i < list->count; ++i) {
        QSO *item = &list->items[i];
        int should_export = 0;
        if (export_mode == EXPORT_MODE_MARKED) {
            should_export = item->marked;
        } else if (export_mode == EXPORT_MODE_ONLY_NEW) {
            should_export = (item->handled == 0);
        } else {
            should_export = 1;
        }

        if (!should_export) {
            continue;
        }

        count++;

        /* 1. # (Radnummer / QSO-nummer) */
        fprintf(fp, "%d", count);

        /* 2. Datum */
        char d_buf[32] = "";
        format_date_yyyy_mm_dd(item->date, d_buf, sizeof(d_buf));
        write_csv_cell(fp, d_buf, 0);

        /* 3. Tid */
        char t_buf[32] = "";
        format_time_hh_mm(item->time_utc, t_buf, sizeof(t_buf));
        write_csv_cell(fp, t_buf, 0);

        /* 4. Signal */
        write_csv_cell(fp, item->call, 0);

        /* 5. Frekvens */
        write_csv_cell(fp, item->freq, 0);

        /* 6. Band */
        write_csv_cell(fp, item->band, 0);

        /* 7. Mode */
        write_csv_cell(fp, item->mode, 0);

        /* 8. RST */
        char rst_buf[32] = "";
        format_rst_combined(item->rst_sent, item->rst_rcvd, rst_buf, sizeof(rst_buf));
        write_csv_cell(fp, rst_buf, 0);

        /* 9. Namn */
        write_csv_cell(fp, item->name, 0);

        /* 10. QTH */
        write_csv_cell(fp, item->qth, 0);

        /* 11. QRB */
        if (item->distance[0] == '\0' || strcmp(item->distance, "-") == 0) {
            update_qso_qrb(st, item);
        }
        char qrb_buf[32] = "";
        format_qrb_display(item, qrb_buf, sizeof(qrb_buf));
        write_csv_cell(fp, qrb_buf, 0);

        /* 12. QUJ */
        char quj_buf[32] = "";
        format_quj_display(st, item, quj_buf, sizeof(quj_buf));
        write_csv_cell(fp, quj_buf, 0);

        /* 13. Lokator */
        write_csv_cell(fp, item->gridsquare, 0);

        /* 14. DXCC */
        write_csv_cell(fp, item->dxcc, 0);

        /* 15. Notering */
        write_csv_cell(fp, item->comment, 0);

        fputs("\r\n", fp);
    }

    fclose(fp);
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
static void build_default_export_filename_with_ext(char *out, size_t out_sz,
                                                   const StationSettings *st, int only_new,
                                                   const char *ext) {
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

    snprintf(out, out_sz, "%s_%s%s.%s", clean_call, dt_buf, only_new ? "_nya" : "", ext);
}

void build_default_export_filename(char *out, size_t out_sz, const StationSettings *st, int only_new) {
    build_default_export_filename_with_ext(out, out_sz, st, only_new, "adi");
}

void build_default_export_csv_filename(char *out, size_t out_sz, const StationSettings *st, int only_new) {
    build_default_export_filename_with_ext(out, out_sz, st, only_new, "csv");
}

/**
 * @brief Synkroniserar alla ej hanterade QSO (handled == 0) direkt mot QRZ.COM Logbook API.
 * Lyckade poster markeras som hanterade (handled = 1) i minnet och SQLite/ADIF.
 * @param log_list Logglistan med QSO.
 * @param st Stationsinställningar (innehåller QRZ API-nyckel).
 * @param status_msg Buffer för statusmeddelande.
 * @param status_sz Storlek på statusmeddelandebuffert.
 * @return Antal framgångsrikt synkade QSO, 0 om inga behövde synkas, eller negativt vid fel/saknad nyckel.
 */
int sync_unhandled_qsos_to_qrz(QSOList *log_list, const StationSettings *st,
                               char *status_msg, size_t status_sz) {
    if (st == NULL || st->qrz_api_key[0] == '\0') {
        if (status_msg && status_sz > 0) {
            copy_str(status_msg, status_sz, "QRZ API-nyckel saknas i inställningar.");
        }
        return -1;
    }

    if (log_list == NULL || log_list->count == 0) {
        if (status_msg && status_sz > 0) {
            copy_str(status_msg, status_sz, "Loggen är tom.");
        }
        return 0;
    }

    int unhandled_count = 0;
    for (size_t i = 0; i < log_list->count; ++i) {
        if (log_list->items[i].handled == 0) {
            unhandled_count++;
        }
    }

    if (unhandled_count == 0) {
        if (status_msg && status_sz > 0) {
            copy_str(status_msg, status_sz, "Inga ohanterade QSO att synka mot QRZ.COM.");
        }
        return 0;
    }

    int ok_cnt = 0;
    int fail_cnt = 0;
    int step = 0;
    char last_err[192] = "";

    for (size_t i = 0; i < log_list->count; ++i) {
        if (log_list->items[i].handled != 0) continue;
        step++;

        char stat_str[128];
        snprintf(stat_str, sizeof(stat_str), "Skickar %d/%d (%s)...",
                 step, unhandled_count, log_list->items[i].call);
        draw_progress_modal("QRZ.COM Synk", stat_str);

        char err_buf[192] = "";
        int ok = qrz_logbook_insert(st->qrz_api_key, &log_list->items[i], st,
                                    err_buf, sizeof(err_buf));
        if (ok) {
            log_list->items[i].handled = 1;
            if (db_is_open() && log_list->items[i].id > 0) {
                db_set_handled(log_list->items[i].id, 1);
            }
            ok_cnt++;
        } else {
            fail_cnt++;
            if (last_err[0] == '\0' && err_buf[0] != '\0') {
                copy_str(last_err, sizeof(last_err), err_buf);
            }
        }
    }

    if (!db_is_open()) {
        save_all_qsos_adif(LOG_FILE, log_list, st);
    }

    if (status_msg && status_sz > 0) {
        if (fail_cnt == 0) {
            snprintf(status_msg, status_sz, "Synkade %d QSO till QRZ.COM (✓)!", ok_cnt);
        } else if (ok_cnt > 0) {
            snprintf(status_msg, status_sz, "Synkade %d QSO till QRZ.COM, %d misslyckades: %s",
                     ok_cnt, fail_cnt, last_err[0] ? last_err : "Fel");
        } else {
            snprintf(status_msg, status_sz, "QRZ-synk misslyckades: %s",
                     last_err[0] ? last_err : "Okänt fel");
        }
    }

    return ok_cnt;
}

/* Fildialogen är utbruten till file_dialog.c / file_dialog.h */

static void draw_settings_toggle_row(int y, int colon_col, int fcol, const char *label,
                                     const char *value, int is_active, int *cur_y, int *cur_x) {
    if (y < 0) return;
    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(y, colon_col - utf8_display_width(label), "%s:", label);
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

    int pair = is_active ? CP_FIELD_ACTIVE : CP_FIELD_INACTIVE;
    attron(COLOR_PAIR(pair) | A_BOLD);
    mvprintw(y, fcol, "< %-28s >", value);
    attroff(COLOR_PAIR(pair) | A_BOLD);
    if (is_active) {
        *cur_y = y;
        *cur_x = fcol + 2;
    }
}

static void draw_settings_action_row(int y, int colon_col, int fcol, const char *label,
                                     const char *hint, int is_active, int warn_when_active,
                                     int *cur_y, int *cur_x) {
    if (y < 0) return;
    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(y, colon_col - utf8_display_width(label), "%s:", label);
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

    int pair = is_active ? (warn_when_active ? CP_STATUS_WARN : CP_FIELD_ACTIVE) : CP_FIELD_INACTIVE;
    attron(COLOR_PAIR(pair) | A_BOLD);
    mvprintw(y, fcol, "%-s", hint);
    attroff(COLOR_PAIR(pair) | A_BOLD);
    if (is_active) {
        *cur_y = y;
        *cur_x = fcol + 2;
    }
}

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

    int start_y = (LINES >= 28) ? 3 : 2;
    int last_row = (LINES >= 26) ? (LINES - 4) : (LINES - 3);
    int visible_rows = last_row - start_y + 1;
    if (visible_rows < 1) visible_rows = 1;
    int scroll_offset = 0;
    if (visible_rows < STATION_FIELD_COUNT && active >= visible_rows) {
        scroll_offset = active - visible_rows + 1;
    }
    #define FIELD_Y(idx) (((idx) >= scroll_offset && (idx) < scroll_offset + visible_rows) \
                          ? (start_y + ((idx) - scroll_offset)) : -1)

    int colon_col = 22;
    int fw = COLS - colon_col - 5;
    if (fw > 32) fw = 32;
    if (fw < 12) fw = 12;
    int fcol = colon_col + 2;

    if (FIELD_Y(0) >= 0) draw_field_aligned(FIELD_Y(0), colon_col, "Signal",        edit->call,       fw, active == 0, cursor_pos, cur_y, cur_x);
    if (FIELD_Y(1) >= 0) draw_field_aligned(FIELD_Y(1), colon_col, "Namn",          edit->name,       fw, active == 1, cursor_pos, cur_y, cur_x);
    if (FIELD_Y(2) >= 0) draw_field_aligned(FIELD_Y(2), colon_col, "QTH",           edit->qth,        fw, active == 2, cursor_pos, cur_y, cur_x);
    if (FIELD_Y(3) >= 0) draw_field_aligned(FIELD_Y(3), colon_col, "Lokator",       edit->gridsquare, fw, active == 3, cursor_pos, cur_y, cur_x);
    if (FIELD_Y(4) >= 0) draw_field_aligned(FIELD_Y(4), colon_col, "Lokal tidszon", edit->timezone,   fw, active == 4, cursor_pos, cur_y, cur_x);
    if (FIELD_Y(5) >= 0) draw_field_aligned(FIELD_Y(5), colon_col, "QRZ.COM Användare", edit->qrz_username, fw, active == 5, cursor_pos, cur_y, cur_x);

    /* Fält 6: QRZ.COM Lösenord */
    if (FIELD_Y(6) >= 0) {
        char pass_display[64] = "";
        if (active == 6) {
            copy_str(pass_display, sizeof(pass_display), edit->qrz_password);
        } else if (edit->qrz_password[0] != '\0') {
            copy_str(pass_display, sizeof(pass_display), "**********");
        }
        draw_field_aligned(FIELD_Y(6), colon_col, "QRZ.COM Lösenord", pass_display, fw, active == 6, cursor_pos, cur_y, cur_x);
    }

    /* Fält 7: QRZ Logbook API-nyckel */
    if (FIELD_Y(7) >= 0) {
        char key_display[64] = "";
        if (active == 7) {
            copy_str(key_display, sizeof(key_display), edit->qrz_api_key);
        } else if (edit->qrz_api_key[0] != '\0') {
            copy_str(key_display, sizeof(key_display), "**********");
        }
        draw_field_aligned(FIELD_Y(7), colon_col, "QRZ API-nyckel", key_display, fw, active == 7, cursor_pos, cur_y, cur_x);
    }

    /* Fält 8: Auto QRZ.COM */
    const char *qrz_txt = (edit->qrz_enabled == QRZ_MODE_AUTO)   ? "Auto" :
                          (edit->qrz_enabled == QRZ_MODE_MANUAL) ? "Manuell" : "Av";
    draw_settings_toggle_row(FIELD_Y(8), colon_col, fcol, "Auto QRZ.COM", qrz_txt, active == 8, cur_y, cur_x);

    /* Fält 9: Direktlogg QRZ.COM */
    const char *qrz_log_txt = (edit->qrz_log_enabled == QRZ_LOG_AUTO)   ? "Automatisk" :
                              (edit->qrz_log_enabled == QRZ_LOG_MANUAL) ? "Manuell" : "Av";
    draw_settings_toggle_row(FIELD_Y(9), colon_col, fcol, "Direktlogg QRZ.COM", qrz_log_txt, active == 9, cur_y, cur_x);

    /* Fält 10: Färgschema */
    int r_theme = FIELD_Y(10);
    if (r_theme >= 0) {
        const char *lbl_theme = "Färgschema";
        attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
        mvprintw(r_theme, colon_col - utf8_display_width(lbl_theme), "%s:", lbl_theme);
        attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

        int t_idx = (edit->theme >= 0 && edit->theme < THEME_COUNT) ? edit->theme : THEME_DEFAULT;
        char theme_disp[64];
        utf8_pad_or_clip(theme_disp, sizeof(theme_disp), THEME_NAMES[t_idx], 26, 0);

        int pair = (active == 10) ? CP_FIELD_ACTIVE : CP_FIELD_INACTIVE;
        attron(COLOR_PAIR(pair) | A_BOLD);
        mvprintw(r_theme, fcol, "[ %s v ]", theme_disp);
        attroff(COLOR_PAIR(pair) | A_BOLD);
        if (active == 10) {
            *cur_y = r_theme;
            *cur_x = fcol + 2;
        }
        if (COLS >= 78) {
            int p_hint = (active == 10) ? CP_TOPBAR : CP_BORDER;
            int a_hint = (active == 10) ? A_BOLD : 0;
            attron(COLOR_PAIR(p_hint) | a_hint);
            mvprintw(r_theme, fcol + 32, "[ F7: Egen färgprofil... ]");
            attroff(COLOR_PAIR(p_hint) | a_hint);
        }
    }

    /* Fält 11..14: Växlar */
    draw_settings_toggle_row(FIELD_Y(11), colon_col, fcol, "Auto RST 59/59",
                             edit->auto_rst_59 ? "På (Fyll i 59 / 59 vid nytt QSO)" : "Av",
                             active == 11, cur_y, cur_x);
    draw_settings_toggle_row(FIELD_Y(12), colon_col, fcol, "Filtrera loggrader",
                             edit->filter_log ? "På" : "Av", active == 12, cur_y, cur_x);
    draw_settings_toggle_row(FIELD_Y(13), colon_col, fcol, "Statistikruta",
                             edit->show_stats ? "På (Visa ovanför QSO)" : "Av",
                             active == 13, cur_y, cur_x);
    draw_settings_toggle_row(FIELD_Y(14), colon_col, fcol, "Rymdväderruta",
                             edit->show_space_weather ? "På (hf.avitec.se)" : "Av",
                             active == 14, cur_y, cur_x);

    /* Fält 15: Databas i ~/.ueilogg */
    int r_db = FIELD_Y(15);
    if (r_db >= 0) {
        draw_field_aligned(r_db, colon_col, "Databas", edit->db_name, fw, active == 15, cursor_pos, cur_y, cur_x);
        if (COLS >= fcol + fw + 28) {
            int p_hint = (active == 15) ? CP_TOPBAR : CP_BORDER;
            int a_hint = (active == 15) ? A_BOLD : 0;
            attron(COLOR_PAIR(p_hint) | a_hint);
            mvprintw(r_db, fcol + fw + 2, "[ F3: Välj i ~/.ueilogg ]");
            attroff(COLOR_PAIR(p_hint) | a_hint);
        }
    }

    /* Fält 16: Importera ADIF från fil (F5) */
    draw_settings_action_row(FIELD_Y(16), colon_col, fcol, "Importera ADIF (F5)",
                             "[ Tryck F5 eller Enter för att bläddra... ]", active == 16, 0, cur_y, cur_x);

    /* Fält 17..19: Export & synk */
    int marked_count = 0, new_count = 0, unhandled_count = 0;
    if (log_list != NULL) {
        for (size_t i = 0; i < log_list->count; ++i) {
            if (log_list->items[i].marked) marked_count++;
            if (log_list->items[i].handled == 0) {
                new_count++;
                unhandled_count++;
            }
        }
    }

    char export_hint[128];
    if (marked_count > 0) {
        snprintf(export_hint, sizeof(export_hint), "[ F6 / Enter: Exportera %d markerade QSO ]", marked_count);
    } else if (log_list != NULL && log_list->count > 0) {
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
    draw_settings_action_row(FIELD_Y(17), colon_col, fcol, "Exportera ADIF (F6)",
                             export_hint, active == 17, 0, cur_y, cur_x);

    char csv_export_hint[128];
    if (marked_count > 0) {
        snprintf(csv_export_hint, sizeof(csv_export_hint), "[ Enter: Exportera %d markerade QSO till CSV ]", marked_count);
    } else if (log_list != NULL && log_list->count > 0) {
        snprintf(csv_export_hint, sizeof(csv_export_hint), "[ Enter: Exportera alla (%zu QSO) till CSV ]", log_list->count);
    } else {
        snprintf(csv_export_hint, sizeof(csv_export_hint), "[ Enter: Exportera alla (0 QSO) till CSV ]");
    }
    draw_settings_action_row(FIELD_Y(18), colon_col, fcol, "Exportera som CSV",
                             csv_export_hint, active == 18, 0, cur_y, cur_x);

    char sync_up_hint[128];
    if (unhandled_count > 0) {
        snprintf(sync_up_hint, sizeof(sync_up_hint), "[ Enter: Synka %d ej hanterade QSO till QRZ.COM ]", unhandled_count);
    } else {
        snprintf(sync_up_hint, sizeof(sync_up_hint), "[ Enter: Synka ej hanterade QSO till QRZ.COM (0 nya) ]");
    }
    draw_settings_action_row(FIELD_Y(19), colon_col, fcol, "Synka till QRZ.COM",
                             sync_up_hint, active == 19, 0, cur_y, cur_x);

    /* Fält 20: Synka nya QSO från QRZ.COM (F8) */
    draw_settings_action_row(FIELD_Y(20), colon_col, fcol, "Synka från QRZ (F8)",
                             "[ F8 / Enter: Hämta nya QSO från QRZ.COM ]", active == 20, 0, cur_y, cur_x);

    /* Fält 21: Töm loggbok */
    draw_settings_action_row(FIELD_Y(21), colon_col, fcol, "Töm loggbok",
                             "[ Tryck Enter för att tömma ]", active == 21, 1, cur_y, cur_x);

    if (scroll_offset > 0) {
        attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvprintw(1, COLS - 5, " ^ ");
        attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
    }
    if (scroll_offset + visible_rows < STATION_FIELD_COUNT) {
        attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvprintw(LINES - 2, COLS - 5, " v ");
        attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
    }

    #undef FIELD_Y

    const char *footer_hint =
        (active == 10) ? "Enter: Välj tema. F7 / E: Redigera egen profil. F2/F12: Spara." :
        (active == 15) ? "Skriv databasnamn (~/.ueilogg) eller F3: Välj befintlig. Enter/F2: Byt." :
                         "Tab/Pilar: Byt fält. F5=Import, F6=Export, F8=QRZ-synk. F2/F12: Spara.";

    if (LINES >= 26) {
        if (status_msg != NULL && status_msg[0] != '\0') {
            attron(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);
            mvprintw(LINES - 3, 3, "Status: %-s", status_msg);
            attroff(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);
        }
        mvprintw(LINES - 2, 3, "%s", footer_hint);
    } else {
        if (status_msg != NULL && status_msg[0] != '\0') {
            attron(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);
            mvprintw(LINES - 2, 3, "Status: %-s", status_msg);
            attroff(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);
        } else if (LINES >= 17) {
            mvprintw(LINES - 2, 3, "%s", footer_hint);
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

    int start_y = (LINES >= 27) ? 3 : 2;
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

static int cmp_db_names(const void *a, const void *b) {
    const char *sa = *(const char * const *)a;
    const char *sb = *(const char * const *)b;
    return strcasecmp(sa, sb);
}

/**
 * @brief Visar en popup-lista med befintliga SQLite-databaser (.sq3) i ~/.ueilogg/ samt val att ange nytt namn.
 * @return 0 vid Esc, 1 om befintlig databas valdes, 2 om '< Nytt databasnamn... >' valdes.
 */
static int run_db_picker_dropdown(StationSettings *edit, const QSOList *log_list, const char *status_msg) {
    ensure_app_data_dir();
    const char *app_dir = get_app_data_dir();

    char **items = NULL;
    int count = 0;
    int cap = 0;
    int has_default = 0;

    DIR *dp = opendir(app_dir);
    if (dp != NULL) {
        struct dirent *ent;
        while ((ent = readdir(dp)) != NULL) {
            const char *name = ent->d_name;
            size_t len = strlen(name);
            if (len > 4 && strcasecmp(name + len - 4, ".sq3") == 0) {
                char full[PATH_MAX];
                build_app_data_path(name, full, sizeof(full));
                struct stat st_f;
                if (stat(full, &st_f) == 0 && S_ISREG(st_f.st_mode)) {
                    if (count >= cap) {
                        int ncap = cap ? cap * 2 : 16;
                        char **tmp = realloc(items, (size_t)ncap * sizeof(char *));
                        if (!tmp) break;
                        items = tmp;
                        cap = ncap;
                    }
                    items[count++] = strdup(name);
                    if (strcmp(name, DEFAULT_LOG_NAME) == 0) has_default = 1;
                }
            }
        }
        closedir(dp);
    }

    if (!has_default) {
        if (count >= cap) {
            int ncap = cap ? cap * 2 : 16;
            char **tmp = realloc(items, (size_t)ncap * sizeof(char *));
            if (tmp) {
                items = tmp;
                cap = ncap;
            }
        }
        if (count < cap) {
            items[count++] = strdup(DEFAULT_LOG_NAME);
        }
    }
    if (count > 1) {
        qsort(items, (size_t)count, sizeof(char *), cmp_db_names);
    }

    /* Lägg till val för att skapa ny databas sist */
    const char *new_label = "< Nytt databasnamn... >";
    if (count >= cap) {
        int ncap = cap ? cap * 2 : 16;
        char **tmp = realloc(items, (size_t)ncap * sizeof(char *));
        if (tmp) {
            items = tmp;
            cap = ncap;
        }
    }
    int create_idx = -1;
    if (count < cap) {
        create_idx = count;
        items[count++] = strdup(new_label);
    }

    int sel_idx = 0;
    for (int i = 0; i < count; ++i) {
        if (i != create_idx && strcmp(items[i], edit->db_name) == 0) {
            sel_idx = i;
            break;
        }
    }

    int list_w = 38;
    int pop_x = 24;
    if (pop_x + list_w >= COLS) pop_x = COLS - list_w - 1;
    if (pop_x < 1) pop_x = 1;
    int max_vis = LINES - 6;
    if (max_vis < 4) max_vis = 4;
    int visible_items = (count < max_vis) ? count : max_vis;
    int list_h = visible_items + 2;
    int pop_y = (LINES - list_h) / 2;
    if (pop_y < 2) pop_y = 2;

    int scroll_offset = 0;
    int result = 0;
    curs_set(0);

    while (1) {
        if (sel_idx < scroll_offset) scroll_offset = sel_idx;
        else if (sel_idx >= scroll_offset + visible_items) scroll_offset = sel_idx - visible_items + 1;

        draw_settings_dialog_view(edit, 15, 0, log_list, status_msg, NULL, NULL);
        curs_set(0);
        draw_box_with_title(pop_y, pop_x, list_h, list_w, "Välj databas (~/.ueilogg)", 1);

        for (int i = 0; i < visible_items; ++i) {
            int idx = scroll_offset + i;
            if (idx >= count) break;
            char disp[64];
            utf8_pad_or_clip(disp, sizeof(disp), items[idx], list_w - 8, 0);
            int r = pop_y + 1 + i;
            if (idx == sel_idx) {
                attron(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
                mvprintw(r, pop_x + 1, " > %s < ", disp);
                attroff(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
            } else {
                attron(COLOR_PAIR(CP_FIELD_INACTIVE));
                mvprintw(r, pop_x + 1, "   %s   ", disp);
                attroff(COLOR_PAIR(CP_FIELD_INACTIVE));
            }
        }
        refresh();

        wint_t wch = 0;
        int input_res = get_wch(&wch);
        int ch = (input_res == ERR) ? ERR : (int)wch;
        if (ch == ERR) continue;

        if (ch == 27 || ch == KEY_F(3)) {
            result = 0;
            break;
        } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' ') {
            if (sel_idx == create_idx) {
                result = 2;
            } else {
                copy_str(edit->db_name, sizeof(edit->db_name), items[sel_idx]);
                result = 1;
            }
            break;
        } else if (ch == KEY_UP || ch == 'k' || ch == 'K') {
            sel_idx = (sel_idx > 0) ? sel_idx - 1 : count - 1;
        } else if (ch == KEY_DOWN || ch == 'j' || ch == 'J') {
            sel_idx = (sel_idx + 1 < count) ? sel_idx + 1 : 0;
        } else if (ch == KEY_HOME) {
            sel_idx = 0;
        } else if (ch == KEY_END) {
            sel_idx = count - 1;
        }
    }

    for (int i = 0; i < count; ++i) free(items[i]);
    free(items);
    curs_set(1);
    return result;
}

/**
 * @brief Returnerar markörens slutposition (antal tecken) för angivet textfält i inställningarna.
 */
static int settings_field_cursor_end(const StationSettings *edit, int active) {
    switch (active) {
        case 0: return utf8_display_width(edit->call);
        case 1: return utf8_display_width(edit->name);
        case 2: return utf8_display_width(edit->qth);
        case 3: return utf8_display_width(edit->gridsquare);
        case 4: return utf8_display_width(edit->timezone);
        case 5: return utf8_display_width(edit->qrz_username);
        case 6: return utf8_display_width(edit->qrz_password);
        case 7: return utf8_display_width(edit->qrz_api_key);
        case 15: return utf8_display_width(edit->db_name);
        default: return 0;
    }
}

/**
 * @brief Återställer redigeringsindex (editing_index) efter en import/synk som sorterat om loggen.
 */
static void restore_editing_index_after_import(const QSOList *log_list, QSO *qso,
                                               int *editing_index, int had_edit,
                                               const QSO *orig_edited) {
    if (!had_edit || !editing_index || !log_list || !orig_edited) return;
    int found_new_idx = -1;
    for (size_t k = 0; k < log_list->count; ++k) {
        const QSO *item = &log_list->items[k];
        if (strcasecmp(item->call, orig_edited->call) == 0 &&
            strcmp(item->date, orig_edited->date) == 0 &&
            strcmp(item->time_utc, orig_edited->time_utc) == 0 &&
            strcasecmp(item->band, orig_edited->band) == 0) {
            found_new_idx = (int)k;
            break;
        }
    }
    *editing_index = found_new_idx;
    if (found_new_idx >= 0 && qso) {
        qso->id = log_list->items[found_new_idx].id;
    }
}

/**
 * @brief Kör gemensamt exportflöde för ADIF (is_csv=0) och CSV (is_csv=1).
 */
static void run_export_dialog(const StationSettings *edit, QSO *qso, const int *editing_index,
                              QSOList *log_list, int is_csv,
                              char *status_msg, size_t status_sz) {
    const char *fmt_name = is_csv ? "CSV" : "ADIF";
    if (log_list == NULL || log_list->count == 0) {
        copy_str(status_msg, status_sz, "Loggen är tom – inget att exportera.");
        return;
    }

    int m_cnt = 0, m_new_cnt = 0, new_cnt = 0, handled_cnt = 0;
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
        snprintf(diag_title, sizeof(diag_title), "Exportera %d markerade QSO till %s", m_cnt, fmt_name);
    } else if (new_cnt > 0 && handled_cnt > 0) {
        int choice = 0;
        char box_title[64];
        snprintf(box_title, sizeof(box_title), "Exportera till %s", fmt_name);
        while (1) {
            int box_h = 8, box_w = 48;
            int by = (LINES - box_h) / 2;
            int bx = (COLS - box_w) / 2;
            attron(COLOR_PAIR(CP_MAIN_BG) | A_BOLD);
            for (int r = 0; r < box_h; ++r) {
                mvhline(by + r, bx, ' ', box_w);
            }
            draw_box_with_title(by, bx, box_h, box_w, box_title, 1);
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
            snprintf(status_msg, status_sz, "%s-export avbröts.", fmt_name);
            return;
        }
        if (choice == 1) {
            exp_mode = EXPORT_MODE_ONLY_NEW;
            only_new = 1;
            snprintf(diag_title, sizeof(diag_title), "Exportera %d nya QSO till %s", new_cnt, fmt_name);
        } else {
            exp_mode = EXPORT_MODE_ALL;
            only_new = 0;
            snprintf(diag_title, sizeof(diag_title), "Exportera hela loggen (%zu QSO) till %s", log_list->count, fmt_name);
        }
    } else if (new_cnt > 0) {
        exp_mode = EXPORT_MODE_ALL;
        only_new = 1;
        snprintf(diag_title, sizeof(diag_title), "Exportera alla (%zu nya QSO) till %s", log_list->count, fmt_name);
    } else {
        exp_mode = EXPORT_MODE_ALL;
        only_new = 0;
        snprintf(diag_title, sizeof(diag_title), "Exportera hela loggen (%zu QSO) till %s", log_list->count, fmt_name);
    }

    char def_name[PATH_MAX];
    build_default_export_filename_with_ext(def_name, sizeof(def_name), edit, only_new, is_csv ? "csv" : "adi");

    char chosen_path[PATH_MAX] = {0};
    int chosen = run_file_dialog(diag_title, ".", is_csv ? ".csv" : ".adi;.adif", chosen_path, sizeof(chosen_path), 1, def_name);
    if (!chosen || chosen_path[0] == '\0') {
        snprintf(status_msg, status_sz, "%s-export avbröts.", fmt_name);
        return;
    }

    char final_path[PATH_MAX];
    copy_str(final_path, sizeof(final_path), chosen_path);
    if (is_csv) {
        const char *ext = strrchr(final_path, '.');
        if (!ext || strcasecmp(ext, ".csv") != 0) {
            if (strlen(final_path) + 4 < sizeof(final_path)) {
                strcat(final_path, ".csv");
            }
        }
    }

    int exported = is_csv ? export_qsos_csv(final_path, log_list, edit, exp_mode)
                          : export_qsos_adif(final_path, log_list, edit, exp_mode);
    if (exported < 0) {
        snprintf(status_msg, status_sz, "Kunde inte spara till filen: %s", get_path_basename(final_path));
        return;
    }

    if (!is_csv && qso && editing_index && *editing_index >= 0 && (size_t)*editing_index < log_list->count) {
        qso->handled = log_list->items[*editing_index].handled;
    }
    if (exp_mode == EXPORT_MODE_ONLY_NEW) {
        snprintf(status_msg, status_sz, "Exporterade %d nya QSO till %s!", exported, get_path_basename(final_path));
    } else if (exp_mode == EXPORT_MODE_MARKED) {
        snprintf(status_msg, status_sz, "Exporterade %d markerade QSO till %s!", exported, get_path_basename(final_path));
    } else {
        snprintf(status_msg, status_sz, "Exporterade %d QSO till %s!", exported, get_path_basename(final_path));
    }
}

int switch_station_database(StationSettings *st, QSO *qso, int *editing_index,
                            QSOList *log_list, int *selected_log_pos, int *log_scroll_offset,
                            const char *new_db_name, char *status_msg, size_t status_sz) {
    if (st == NULL) return -1;

    char norm_name[128];
    normalize_db_name(new_db_name, norm_name, sizeof(norm_name));

    char new_path[PATH_MAX];
    resolve_db_path(norm_name, new_path, sizeof(new_path));

    char old_path[PATH_MAX];
    copy_str(old_path, sizeof(old_path), LOG_FILE);

    ensure_app_data_dir();
    db_close();
    if (db_open(new_path) != 0) {
        if (old_path[0] != '\0') {
            db_open(old_path);
        }
        if (status_msg && status_sz > 0) {
            snprintf(status_msg, status_sz, "Kunde inte öppna databasen '%s'.", norm_name);
        }
        return -1;
    }

    copy_str(g_log_filename, sizeof(g_log_filename), new_path);
    copy_str(st->db_name, sizeof(st->db_name), norm_name);

    if (log_list != NULL) {
        log_list->count = 0;
        db_load_all_qsos(log_list);
        update_all_missing_qrbs(st, log_list);
    }
    invalidate_log_stats_cache();
    if (editing_index) *editing_index = -1;
    if (selected_log_pos) *selected_log_pos = 0;
    if (log_scroll_offset) *log_scroll_offset = 0;
    if (qso) qso->id = 0;

    if (status_msg && status_sz > 0) {
        snprintf(status_msg, status_sz, "Bytte databas till %s (%zu QSO).",
                 st->db_name, log_list ? log_list->count : 0);
    }
    return 0;
}

static void save_and_apply_settings(StationSettings *st, StationSettings *edit,
                                    const char *orig_db_name,
                                    QSO *qso, int *editing_index,
                                    QSOList *log_list, int *selected_log_pos,
                                    int *log_scroll_offset,
                                    char *status_msg, size_t status_sz) {
    to_upper_str(edit->call);
    to_upper_str(edit->gridsquare);
    normalize_db_name(edit->db_name, edit->db_name, sizeof(edit->db_name));

    int db_changed = (orig_db_name != NULL && strcmp(edit->db_name, orig_db_name) != 0);
    char target_db[128];
    copy_str(target_db, sizeof(target_db), edit->db_name);

    /* Behåll föregående db_name i *st tills databasbytet har bekräftats */
    *st = *edit;
    if (db_changed && orig_db_name != NULL) {
        copy_str(st->db_name, sizeof(st->db_name), orig_db_name);
    }
    apply_color_theme(st->theme);

    if (db_changed) {
        int rc = switch_station_database(st, qso, editing_index, log_list,
                                         selected_log_pos, log_scroll_offset,
                                         target_db, status_msg, status_sz);
        save_station_settings(CONFIG_FILE, st);
        if (rc == 0 && status_msg && status_sz > 0) {
            snprintf(status_msg, status_sz, "Inställningar sparade. Bytte databas till %s (%zu QSO).",
                     st->db_name, log_list ? log_list->count : 0);
        }
    } else {
        save_station_settings(CONFIG_FILE, st);
        copy_str(status_msg, status_sz, "Inställningar sparade.");
    }
}

/**
 * @brief Hanterar inställningsdialogen (F12) där användaren kan redigera min stations signal, namn, QTH,
 * lokator, tidszon, färgschema, databas, autofyll samt utföra import (F5), export (F6) och loggtömning.
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
    normalize_db_name(edit.db_name, edit.db_name, sizeof(edit.db_name));
    char orig_db_name[128];
    copy_str(orig_db_name, sizeof(orig_db_name), edit.db_name);

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
            save_and_apply_settings(st, &edit, orig_db_name, qso, editing_index,
                                    log_list, selected_log_pos, log_scroll_offset,
                                    status_msg, status_sz);
            return;
        } else if (ch == 27) {
            apply_color_theme(st->theme);
            copy_str(status_msg, status_sz, "Inställningar stängda utan att spara.");
            return;
        } else if (ch == KEY_F(9)) {
            run_help_modal(&edit);
            apply_color_theme(edit.theme);
            continue;
        } else if (ch == KEY_F(3)) {
            active = 15;
            int pick = run_db_picker_dropdown(&edit, log_list, status_msg);
            if (pick == 2) {
                edit.db_name[0] = '\0';
                cursor_pos = 0;
            } else {
                cursor_pos = utf8_display_width(edit.db_name);
            }
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
        } else if (ch == KEY_F(5) || (active == 16 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' '))) {
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
                    restore_editing_index_after_import(log_list, qso, editing_index, had_edit, &orig_edited);
                    if (selected_log_pos && log_list && log_list->count > 0) {
                        *selected_log_pos = (int)log_list->count - 1;
                    }
                }
            } else {
                copy_str(status_msg, status_sz, "ADIF-import avbröts.");
            }
            apply_color_theme(edit.theme);
            continue;
        } else if (ch == KEY_F(6) || (active == 17 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' '))) {
            run_export_dialog(&edit, qso, editing_index, log_list, 0, status_msg, status_sz);
            apply_color_theme(edit.theme);
            continue;
        } else if (active == 18 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' ')) {
            run_export_dialog(&edit, qso, editing_index, log_list, 1, status_msg, status_sz);
            apply_color_theme(edit.theme);
            continue;
        } else if (active == 19 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' ')) {
            sync_unhandled_qsos_to_qrz(log_list, &edit, status_msg, status_sz);
            if (qso && editing_index && *editing_index >= 0 && log_list && (size_t)*editing_index < log_list->count) {
                qso->handled = log_list->items[*editing_index].handled;
            }
            apply_color_theme(edit.theme);
            continue;
        } else if (ch == KEY_F(8) || (active == 20 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' '))) {
            if (edit.qrz_api_key[0] == '\0') {
                copy_str(status_msg, status_sz, "QRZ API-nyckel saknas i inställningar.");
                apply_color_theme(edit.theme);
                continue;
            }

            draw_progress_modal("QRZ.COM Synk", "Hämtar loggbok från QRZ.COM...");

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
                    restore_editing_index_after_import(log_list, qso, editing_index, had_edit, &orig_edited);
                    if (selected_log_pos && log_list && log_list->count > 0) {
                        *selected_log_pos = (int)log_list->count - 1;
                    }
                }
            }
            if (adif_data) free(adif_data);
            apply_color_theme(edit.theme);
            continue;
        } else if (active == 21 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' ')) {
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
        } else if (ch == KEY_F(2) || ch == 19 || (active < 16 && active != 10 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER))) {
            save_and_apply_settings(st, &edit, orig_db_name, qso, editing_index,
                                    log_list, selected_log_pos, log_scroll_offset,
                                    status_msg, status_sz);
            return;
        } else if (ch == '\t' || ch == KEY_DOWN) {
            active = (active + 1) % STATION_FIELD_COUNT;
            cursor_pos = settings_field_cursor_end(&edit, active);
        } else if (ch == KEY_BTAB || ch == KEY_UP) {
            active = (active + STATION_FIELD_COUNT - 1) % STATION_FIELD_COUNT;
            cursor_pos = settings_field_cursor_end(&edit, active);
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
                save_and_apply_settings(st, &edit, orig_db_name, qso, editing_index,
                                        log_list, selected_log_pos, log_scroll_offset,
                                        status_msg, status_sz);
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
        } else if (active < 8 || active == 15) {
            char *buf = (active == 0) ? edit.call :
                        (active == 1) ? edit.name :
                        (active == 2) ? edit.qth :
                        (active == 3) ? edit.gridsquare :
                        (active == 4) ? edit.timezone :
                        (active == 5) ? edit.qrz_username :
                        (active == 6) ? edit.qrz_password :
                        (active == 7) ? edit.qrz_api_key : edit.db_name;
            size_t max_len = (active == 0) ? sizeof(edit.call) :
                             (active == 1) ? sizeof(edit.name) :
                             (active == 2) ? sizeof(edit.qth) :
                             (active == 3) ? sizeof(edit.gridsquare) :
                             (active == 4) ? sizeof(edit.timezone) :
                             (active == 7) ? sizeof(edit.qrz_api_key) :
                             (active == 15) ? sizeof(edit.db_name) : 33;

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
                if (active == 15 && (wch == '/' || wch == '\\')) {
                    continue;
                }
                if (active == 0 || active == 3) {
                    wch = towupper(wch);
                }
                char u8[8];
                size_t u8_len = utf8_encode_wchar((wchar_t)wch, u8, sizeof(u8));
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
