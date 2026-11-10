/**
 * @file settings.c
 * @brief Stationsinställningar, ADIF-import och -export, fildialog samt inställningsmodalen (F12).
 */

#include "settings.h"
#include "dxcc.h"
#include "logg.h"
#include "qrb.h"
#include "tui.h"
#include "util.h"
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
    FILE *fp = fopen(LOG_FILE, "w");
    if (fp == NULL) return -1;
    write_adif_header(fp);
    fclose(fp);
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
        parse_and_normalize_freq(current->freq, norm_freq, sizeof(norm_freq));
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
        save_all_qsos_adif(LOG_FILE, list, st);
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
            write_qso_record_adif(fp, &list->items[i], st);
            count++;
        }
    }

    fclose(fp);

    /* Spara loggen så att handled-flaggan bevaras i qsolog.adi */
    if (count > 0) {
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

/* Struktur och hjälpfunktioner för fildialogen */
typedef struct {
    char name[256];
    int is_dir;
    long size_bytes;
} FileDialogItem;

/**
 * @brief Sorteringsfunktion för fildialogen (kataloger överst, sedan alfabetiskt).
 */
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

/**
 * @brief Matchar ett filnamn mot en semikolonseparerad lista av tillåtna ändelser (t.ex. ".adi;.adif").
 */
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

/**
 * @brief Formaterar filstorlek i läsbart format (B, KB, MB, GB).
 */
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

/**
 * @brief Läser in mappar och matchande filer i en katalog för fildialogen.
 */
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

/**
 * @brief Säkerställer att ett filnamn har .adi som ändelse om ändelse saknas.
 */
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

/**
 * @brief Generisk filväljardialog med ncurses (stöder både öppna/importera och spara/exportera).
 * @param title Dialogrutans rubrik.
 * @param start_dir Startkatalog (t.ex. ".").
 * @param ext_filter Filter för ändelser (t.ex. ".adi;.adif").
 * @param selected_path Buffer för vald fils fullständiga sökväg.
 * @param max_path_len Storlek på selected_path.
 * @param is_save_mode 1 om spara-läge (visar filnamnsfält), 0 om öppna-läge.
 * @param default_filename Förvalt filnamn vid spara.
 * @return 1 om fil valdes/bekräftades, 0 om användaren avbröt med Escape.
 */
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

            int colon_col = 22;
            int fw = COLS - colon_col - 5;
            if (fw > 32) fw = 32;
            if (fw < 12) fw = 12;

            draw_field_aligned(3,  colon_col, "Signal",        edit.call,       fw, active == 0, cursor_pos, &cur_y, &cur_x);
            draw_field_aligned(4,  colon_col, "Namn",          edit.name,       fw, active == 1, cursor_pos, &cur_y, &cur_x);
            draw_field_aligned(5,  colon_col, "QTH",           edit.qth,        fw, active == 2, cursor_pos, &cur_y, &cur_x);
            draw_field_aligned(6,  colon_col, "Lokator",       edit.gridsquare, fw, active == 3, cursor_pos, &cur_y, &cur_x);
            draw_field_aligned(7,  colon_col, "Lokal tidszon", edit.timezone,   fw, active == 4, cursor_pos, &cur_y, &cur_x);

            /* Fält 5: QRZ.COM Användarnamn */
            draw_field_aligned(8,  colon_col, "QRZ.COM Användare", edit.qrz_username, fw, active == 5, cursor_pos, &cur_y, &cur_x);

            /* Fält 6: QRZ.COM Lösenord (visas i klartext vid inmatning, maskeras som ********** när sparat/inaktivt) */
            char pass_display[64] = "";
            if (active == 6) {
                copy_str(pass_display, sizeof(pass_display), edit.qrz_password);
            } else if (edit.qrz_password[0] != '\0') {
                copy_str(pass_display, sizeof(pass_display), "**********");
            }
            draw_field_aligned(9,  colon_col, "QRZ.COM Lösenord", pass_display, fw, active == 6, cursor_pos, &cur_y, &cur_x);

            int r_qrz_auto = 10;
            int r_theme    = 11;
            int r_smcb     = 12;
            int r_rst      = 13;
            int r_flt      = 14;
            int r_import   = 15;
            int r_export   = 16;
            int r_clear    = 17;

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

            /* Fält 12: Importera ADIF från fil (F5) */
            const char *lbl_import = "Importera ADIF (F5)";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_import, colon_col - utf8_display_width(lbl_import), "%s:", lbl_import);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            if (active == 12) {
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

            /* Fält 13: Exportera ADIF till fil (F6) */
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

            if (active == 13) {
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

            /* Fält 14: Töm loggbok och börja på nytt */
            const char *lbl_clear = "Töm loggbok";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_clear, colon_col - utf8_display_width(lbl_clear), "%s:", lbl_clear);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            if (active == 14) {
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
        } else if (ch == KEY_F(5) || (active == 12 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' '))) {
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
        } else if (ch == KEY_F(6) || (active == 13 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' '))) {
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
        } else if (active == 14 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' ')) {
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
        } else if (ch == KEY_F(2) || ch == 19 || (active < 12 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER))) {
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
                utf8_delete_at(buf, cursor_pos);
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
