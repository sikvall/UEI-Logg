/**
 * @file logg.c
 * @brief Implementation av loggningslogik, QSO-listor, ADIF-inläsning/skrivning och QSO-sparande.
 */

#include "logg.h"
#include "callbook.h"
#include "dxcc.h"
#include "qrb.h"
#include "qrz.h"
#include "settings.h"
#include "tui.h"
#include "util.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

static LogStats s_cached_stats;
static size_t s_cached_count = (size_t)-1;
static time_t s_cached_minute = 0;
static const QSOList *s_cached_ptr = NULL;

char g_log_filename[256] = DEFAULT_LOG_FILE;

void invalidate_log_stats_cache(void) {
    s_cached_count = (size_t)-1;
    s_cached_minute = 0;
    s_cached_ptr = NULL;
}

static int cmp_str_ptrs(const void *a, const void *b) {
    const char * const *sa = (const char * const *)a;
    const char * const *sb = (const char * const *)b;
    return strcmp(*sa, *sb);
}

static time_t parse_qso_time_utc(const char *date_str, const char *time_str) {
    if (!date_str || !time_str || date_str[0] == '\0' || time_str[0] == '\0') return 0;
    int year = 0, mon = 0, day = 0;
    if (sscanf(date_str, "%d-%d-%d", &year, &mon, &day) != 3) {
        if (sscanf(date_str, "%4d%2d%2d", &year, &mon, &day) != 3) {
            return 0;
        }
    }
    int hour = 0, min = 0, sec = 0;
    if (strchr(time_str, ':') != NULL) {
        if (sscanf(time_str, "%d:%d:%d", &hour, &min, &sec) < 2) {
            if (sscanf(time_str, "%d:%d", &hour, &min) != 2) return 0;
        }
    } else {
        if (strlen(time_str) >= 4) {
            if (sscanf(time_str, "%2d%2d", &hour, &min) != 2) return 0;
        } else {
            return 0;
        }
    }
    struct tm tm_utc;
    memset(&tm_utc, 0, sizeof(tm_utc));
    tm_utc.tm_year = year - 1900;
    tm_utc.tm_mon  = mon - 1;
    tm_utc.tm_mday = day;
    tm_utc.tm_hour = hour;
    tm_utc.tm_min  = min;
    tm_utc.tm_sec  = sec;
    return timegm(&tm_utc);
}

void compute_log_stats(const QSOList *list, LogStats *stats) {
    if (!stats) return;
    memset(stats, 0, sizeof(LogStats));
    if (!list) return;

    time_t now = time(NULL);
    time_t cur_minute = now / 60;

    if (list == s_cached_ptr && list->count == s_cached_count && cur_minute == s_cached_minute) {
        *stats = s_cached_stats;
        return;
    }

    size_t n = list->count;
    stats->total_qsos = (int)n;
    if (n == 0) {
        s_cached_stats = *stats;
        s_cached_count = 0;
        s_cached_minute = cur_minute;
        s_cached_ptr = list;
        return;
    }

    char (*calls)[32] = malloc(n * sizeof(*calls));
    const char **call_ptrs = malloc(n * sizeof(char *));
    size_t call_cnt = 0;

    char (*dxccs)[64] = malloc(n * sizeof(*dxccs));
    const char **dxcc_ptrs = malloc(n * sizeof(char *));
    size_t dxcc_cnt = 0;

    char (*grids)[8] = malloc(n * sizeof(*grids));
    const char **grid_ptrs = malloc(n * sizeof(char *));
    size_t grid_cnt = 0;

    char (*bandlands)[96] = malloc(n * sizeof(*bandlands));
    const char **bandland_ptrs = malloc(n * sizeof(char *));
    size_t bandland_cnt = 0;

    int rate_cnt = 0;

    for (size_t i = 0; i < n; ++i) {
        const QSO *q = &list->items[i];

        /* Station (unik signal) */
        if (calls && call_ptrs && q->call[0] != '\0') {
            copy_str(calls[call_cnt], sizeof(calls[0]), q->call);
            trim_str(calls[call_cnt]);
            to_upper_str(calls[call_cnt]);
            if (calls[call_cnt][0] != '\0') {
                call_ptrs[call_cnt] = calls[call_cnt];
                call_cnt++;
            }
        }

        /* DXCC */
        char ent[64] = "";
        if (q->dxcc[0] != '\0') {
            copy_str(ent, sizeof(ent), q->dxcc);
        } else if (q->call[0] != '\0') {
            lookup_dxcc(q->call, ent, sizeof(ent));
        }
        trim_str(ent);
        to_upper_str(ent);

        if (dxccs && dxcc_ptrs && ent[0] != '\0' && strcmp(ent, "-") != 0) {
            copy_str(dxccs[dxcc_cnt], sizeof(dxccs[0]), ent);
            dxcc_ptrs[dxcc_cnt] = dxccs[dxcc_cnt];
            dxcc_cnt++;
        }

        /* Lokatorruta (4 tecken, t.ex. JO65) */
        if (grids && grid_ptrs && q->gridsquare[0] != '\0') {
            if (is_valid_locator(q->gridsquare) >= 4) {
                const char *p = q->gridsquare;
                while (*p == ' ' || *p == '\t') p++;
                grids[grid_cnt][0] = (char)toupper((unsigned char)p[0]);
                grids[grid_cnt][1] = (char)toupper((unsigned char)p[1]);
                grids[grid_cnt][2] = p[2];
                grids[grid_cnt][3] = p[3];
                grids[grid_cnt][4] = '\0';
                grid_ptrs[grid_cnt] = grids[grid_cnt];
                grid_cnt++;
            }
        }

        /* Bandland (DXCC + Band) */
        if (bandlands && bandland_ptrs && ent[0] != '\0' && strcmp(ent, "-") != 0 && q->band[0] != '\0') {
            char bnd[16];
            copy_str(bnd, sizeof(bnd), q->band);
            trim_str(bnd);
            to_upper_str(bnd);
            if (bnd[0] != '\0') {
                snprintf(bandlands[bandland_cnt], sizeof(bandlands[0]), "%s#%s", ent, bnd);
                bandland_ptrs[bandland_cnt] = bandlands[bandland_cnt];
                bandland_cnt++;
            }
        }

        /* Rate: antalet loggade QSO de senaste 60 minuterna */
        time_t qt = parse_qso_time_utc(q->date, q->time_utc);
        if (qt > 0) {
            double diff = difftime(now, qt);
            if (diff >= -120.0 && diff <= 3600.0) {
                rate_cnt++;
            }
        }
    }

    /* Sortera och räkna unika element */
    if (call_ptrs && call_cnt > 0) {
        qsort(call_ptrs, call_cnt, sizeof(char *), cmp_str_ptrs);
        int u = 0;
        for (size_t i = 0; i < call_cnt; ++i) {
            if (i == 0 || strcmp(call_ptrs[i], call_ptrs[i - 1]) != 0) u++;
        }
        stats->unique_stations = u;
    }

    if (dxcc_ptrs && dxcc_cnt > 0) {
        qsort(dxcc_ptrs, dxcc_cnt, sizeof(char *), cmp_str_ptrs);
        int u = 0;
        for (size_t i = 0; i < dxcc_cnt; ++i) {
            if (i == 0 || strcmp(dxcc_ptrs[i], dxcc_ptrs[i - 1]) != 0) u++;
        }
        stats->unique_dxcc = u;
    }

    if (grid_ptrs && grid_cnt > 0) {
        qsort(grid_ptrs, grid_cnt, sizeof(char *), cmp_str_ptrs);
        int u = 0;
        for (size_t i = 0; i < grid_cnt; ++i) {
            if (i == 0 || strcmp(grid_ptrs[i], grid_ptrs[i - 1]) != 0) u++;
        }
        stats->unique_grids = u;
    }

    if (bandland_ptrs && bandland_cnt > 0) {
        qsort(bandland_ptrs, bandland_cnt, sizeof(char *), cmp_str_ptrs);
        int u = 0;
        for (size_t i = 0; i < bandland_cnt; ++i) {
            if (i == 0 || strcmp(bandland_ptrs[i], bandland_ptrs[i - 1]) != 0) u++;
        }
        stats->unique_bandland = u;
    }

    stats->rate_last_hour = rate_cnt;

    free(calls);
    free(call_ptrs);
    free(dxccs);
    free(dxcc_ptrs);
    free(grids);
    free(grid_ptrs);
    free(bandlands);
    free(bandland_ptrs);

    s_cached_stats = *stats;
    s_cached_count = n;
    s_cached_minute = cur_minute;
    s_cached_ptr = list;
}

/**
 * @brief Skriver en enskild ADIF-tagg till en fil om värdet inte är tomt.
 * Exempel: <CALL:6>SM5UEI
 * @param fp Filpekare.
 * @param tag ADIF-fältets namn (t.ex. "CALL").
 * @param val Fältets textvärde.
 */
void write_adif_field(FILE *fp, const char *tag, const char *val) {
    if (val != NULL && val[0] != '\0') {
        fprintf(fp, "<%s:%zu>%s ", tag, strlen(val), val);
    }
}

/**
 * @brief Skriver standard ADIF 3.1.8-huvud till en ny eller trunkerad loggfil.
 * @param fp Filpekare.
 */
void write_adif_header(FILE *fp) {
    char created_ts[32];
    time_t now = time(NULL);
    struct tm tm_utc;
    gmtime_r(&now, &tm_utc);
    snprintf(created_ts, sizeof(created_ts), "%04d%02d%02d %02d%02d%02d",
             tm_utc.tm_year + 1900, tm_utc.tm_mon + 1, tm_utc.tm_mday,
             tm_utc.tm_hour, tm_utc.tm_min, tm_utc.tm_sec);

    fprintf(fp, "ADIF Log created by qsolog\n");
    fprintf(fp, "<ADIF_VER:5>3.1.8\n");
    fprintf(fp, "<CREATED_TIMESTAMP:%zu>%s\n", strlen(created_ts), created_ts);
    fprintf(fp, "<PROGRAMID:6>qsolog\n");
    fprintf(fp, "<PROGRAMVERSION:5>1.0.0\n");
    fprintf(fp, "<EOH>\n\n");
}

/**
 * @brief Skriver ett fullständigt QSO-rekord till en ADIF-fil avslutad med <EOR>.
 * @param fp Filpekare.
 * @param qso Det QSO som ska skrivas.
 * @param st Min stations inställningar (för MY_NAME, STATION_CALLSIGN m.m.).
 */
void write_qso_record_adif(FILE *fp, const QSO *qso, const StationSettings *st) {
    char clean_date[16], clean_time[16], clean_band[16];
    normalize_date_adif(qso->date, clean_date, sizeof(clean_date));
    normalize_time_adif(qso->time_utc, clean_time, sizeof(clean_time));
    normalize_band_adif(qso->band, clean_band, sizeof(clean_band));

    write_adif_field(fp, "CALL", qso->call);
    write_adif_field(fp, "NAME", qso->name);
    write_adif_field(fp, "QSO_DATE", clean_date);
    write_adif_field(fp, "TIME_ON", clean_time);
    write_adif_field(fp, "FREQ", qso->freq);
    write_adif_field(fp, "BAND", clean_band);
    write_adif_field(fp, "MODE", qso->mode);
    write_adif_field(fp, "RST_SENT", qso->rst_sent);
    write_adif_field(fp, "RST_RCVD", qso->rst_rcvd);
    write_adif_field(fp, "GRIDSQUARE", qso->gridsquare);
    write_adif_field(fp, "QTH", qso->qth);
    write_adif_field(fp, "COUNTRY", qso->dxcc);
    write_adif_field(fp, "COMMENT", qso->comment);
    write_adif_field(fp, "DISTANCE", qso->distance);
    if (qso->handled) {
        write_adif_field(fp, "APP_QSOLOG_HANDLED", "1");
    }
    if (st != NULL) {
        write_adif_field(fp, "STATION_CALLSIGN", st->call);
        write_adif_field(fp, "OPERATOR", st->call);
        write_adif_field(fp, "MY_NAME", st->name);
        write_adif_field(fp, "MY_CITY", st->qth);
        write_adif_field(fp, "MY_GRIDSQUARE", st->gridsquare);
    }
    fprintf(fp, "<EOR>\n");
}

/**
 * @brief Lägger till ett enskilt QSO i slutet av en ADIF-fil.
 * Skriver filhuvud om filen är tom eller ny.
 * @param filename Sökväg till ADIF-filen.
 * @param qso QSO som ska läggas till.
 * @param st Stationsinställningar.
 * @return 0 vid framgång, -1 vid fel.
 */
int append_qso_adif(const char *filename, const QSO *qso, const StationSettings *st) {
    int need_header = 0;
    FILE *check = fopen(filename, "r");
    if (check == NULL) {
        need_header = 1;
    } else {
        fseek(check, 0, SEEK_END);
        if (ftell(check) == 0) {
            need_header = 1;
        }
        fclose(check);
    }

    FILE *fp = fopen(filename, "a");
    if (fp == NULL) return -1;

    if (need_header) {
        write_adif_header(fp);
    }

    write_qso_record_adif(fp, qso, st);
    fclose(fp);
    return 0;
}

/**
 * @brief Sparar samtliga QSO i en lista till en ADIF-fil (skriver över befintlig fil).
 * @param filename Sökväg till filen.
 * @param list Listan med QSO:n.
 * @param st Stationsinställningar.
 * @return 0 vid framgång, -1 vid fel.
 */
int save_all_qsos_adif(const char *filename, const QSOList *list, const StationSettings *st) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) return -1;

    write_adif_header(fp);

    for (size_t i = 0; i < list->count; ++i) {
        write_qso_record_adif(fp, &list->items[i], st);
    }

    fclose(fp);
    return 0;
}

/**
 * @brief Tilldelar ett inläst ADIF-taggvärde till motsvarande fält i en QSO-struktur.
 * @param qso Målstruktur.
 * @param tag ADIF-tagg (t.ex. "CALL", "FREQ", "MODE", "COMMENT").
 * @param val Fältets textvärde.
 */
void assign_adif_field(QSO *qso, const char *tag, const char *val) {
    if (strcasecmp(tag, "CALL") == 0) {
        copy_str(qso->call, sizeof(qso->call), val);
    } else if (strcasecmp(tag, "NAME") == 0) {
        copy_str(qso->name, sizeof(qso->name), val);
    } else if (strcasecmp(tag, "QSO_DATE") == 0) {
        format_date_yyyy_mm_dd(val, qso->date, sizeof(qso->date));
    } else if (strcasecmp(tag, "TIME_ON") == 0) {
        format_time_hh_mm(val, qso->time_utc, sizeof(qso->time_utc));
    } else if (strcasecmp(tag, "TIME_OFF") == 0) {
        if (qso->time_utc[0] == '\0') format_time_hh_mm(val, qso->time_utc, sizeof(qso->time_utc));
    } else if (strcasecmp(tag, "FREQ") == 0) {
        parse_and_normalize_freq(val, qso->freq, sizeof(qso->freq));
    } else if (strcasecmp(tag, "BAND") == 0) {
        format_band_display(val, qso->band, sizeof(qso->band));
    } else if (strcasecmp(tag, "MODE") == 0) {
        copy_str(qso->mode, sizeof(qso->mode), val);
    } else if (strcasecmp(tag, "SUBMODE") == 0) {
        if (val[0] != '\0' && (qso->mode[0] == '\0' || strcasecmp(qso->mode, "MFSK") == 0 || strcasecmp(qso->mode, "DATA") == 0)) {
            copy_str(qso->mode, sizeof(qso->mode), val);
        }
    } else if (strcasecmp(tag, "RST_SENT") == 0) {
        copy_str(qso->rst_sent, sizeof(qso->rst_sent), val);
    } else if (strcasecmp(tag, "RST_RCVD") == 0) {
        copy_str(qso->rst_rcvd, sizeof(qso->rst_rcvd), val);
    } else if (strcasecmp(tag, "GRIDSQUARE") == 0) {
        copy_str(qso->gridsquare, sizeof(qso->gridsquare), val);
    } else if (strcasecmp(tag, "QTH") == 0) {
        copy_str(qso->qth, sizeof(qso->qth), val);
    } else if (strcasecmp(tag, "COUNTRY") == 0) {
        copy_str(qso->dxcc, sizeof(qso->dxcc), val);
    } else if (strcasecmp(tag, "COMMENT") == 0) {
        copy_str(qso->comment, sizeof(qso->comment), val);
    } else if (strcasecmp(tag, "QSLMSG") == 0 || strcasecmp(tag, "NOTES") == 0) {
        if (qso->comment[0] == '\0') copy_str(qso->comment, sizeof(qso->comment), val);
    } else if (strcasecmp(tag, "DISTANCE") == 0) {
        copy_str(qso->distance, sizeof(qso->distance), val);
    } else if (strcasecmp(tag, "APP_QSOLOG_HANDLED") == 0) {
        qso->handled = (atoi(val) != 0 || strcasecmp(val, "Y") == 0 || strcasecmp(val, "YES") == 0 || strcasecmp(val, "TRUE") == 0);
    }
}

/**
 * @brief Skjuter in ett nytt QSO i slutet av en dynamisk QSO-lista.
 * @param list Listan.
 * @param qso QSO:t som ska läggas till.
 */
void qso_list_push(QSOList *list, const QSO *qso) {
    if (list->count >= list->capacity) {
        size_t new_cap = (list->capacity == 0) ? 32 : list->capacity * 2;
        QSO *new_items = realloc(list->items, new_cap * sizeof(QSO));
        if (new_items == NULL) return;
        list->items = new_items;
        list->capacity = new_cap;
    }
    list->items[list->count++] = *qso;
    invalidate_log_stats_cache();
}

/**
 * @brief Raderar ett QSO vid ett givet index i QSO-listan.
 * @param list Listan.
 * @param index Index på raden som ska raderas.
 */
void qso_list_delete_at(QSOList *list, size_t index) {
    if (index >= list->count) return;
    for (size_t i = index; i + 1 < list->count; ++i) {
        list->items[i] = list->items[i + 1];
    }
    list->count--;
    invalidate_log_stats_cache();
}

/**
 * @brief Raderar alla markerade QSO:n (marked == 1) i en lista.
 * @param list Listan.
 * @return Antal rader som raderades.
 */
int qso_list_delete_marked(QSOList *list) {
    size_t write_idx = 0;
    int deleted = 0;
    for (size_t read_idx = 0; read_idx < list->count; ++read_idx) {
        if (list->items[read_idx].marked) {
            deleted++;
        } else {
            if (write_idx != read_idx) {
                list->items[write_idx] = list->items[read_idx];
            }
            write_idx++;
        }
    }
    list->count = write_idx;
    invalidate_log_stats_cache();
    return deleted;
}

/**
 * @brief Läser in samtliga QSO från en ADIF-fil till en dynamisk lista.
 * @param filename Sökvägen till ADIF-filen (t.ex. qsolog.adi).
 * @param list Mottagande lista.
 */
void load_qsos_from_adif(const char *filename, QSOList *list) {
    list->count = 0;
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) return;

    fseek(fp, 0, SEEK_END);
    long fsize = ftell(fp);
    fseek(fp, 0, SEEK_SET);
    if (fsize <= 0) {
        fclose(fp);
        return;
    }

    char *content = malloc((size_t)fsize + 1);
    if (content == NULL) {
        fclose(fp);
        return;
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
            if (current.dxcc[0] == '\0' && current.call[0] != '\0') {
                lookup_dxcc(current.call, current.dxcc, sizeof(current.dxcc));
            }
            qso_list_push(list, &current);
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

    free(content);
    invalidate_log_stats_cache();
}

/**
 * @brief Kontrollerar om ett QSO redan existerar i listan baserat på signal, datum, tid och band/frekvens.
 * @param list Listan.
 * @param q Det nya QSO:t.
 * @return 1 om dubblett, annars 0.
 */
int qso_is_duplicate(const QSOList *list, const QSO *q) {
    if (q->call[0] == '\0') return 0;
    for (size_t i = 0; i < list->count; ++i) {
        const QSO *ex = &list->items[i];
        if (strcasecmp(ex->call, q->call) == 0 &&
            strcmp(ex->date, q->date) == 0 &&
            strcmp(ex->time_utc, q->time_utc) == 0) {
            if (ex->band[0] == '\0' || q->band[0] == '\0' || strcasecmp(ex->band, q->band) == 0 ||
                (ex->freq[0] != '\0' && q->freq[0] != '\0' && strcmp(ex->freq, q->freq) == 0)) {
                return 1;
            }
        }
    }
    return 0;
}

/**
 * @brief Jämförelsefunktion för qsort som sorterar QSO kronologiskt efter datum och tid UTC.
 * @param a_ptr Pekare till första QSO.
 * @param b_ptr Pekare till andra QSO.
 * @return Negativt om a < b, positivt om a > b, 0 om lika.
 */
int cmp_qso_chronological(const void *a_ptr, const void *b_ptr) {
    const QSO *a = (const QSO *)a_ptr;
    const QSO *b = (const QSO *)b_ptr;
    int c = strcmp(a->date, b->date);
    if (c != 0) return c;
    return strcmp(a->time_utc, b->time_utc);
}

/**
 * @brief Sök efter en signal i befintliga loggrader (nyaste raderna kontrolleras först).
 * Om signalen finns och har namn, QTH eller lokator kopieras dessa.
 * @param log_list Logglistan.
 * @param call_in Signalen.
 * @param name_out Buffer för namn.
 * @param name_sz Storlek på namnbuffer.
 * @param qth_out Buffer för QTH.
 * @param qth_sz Storlek på QTH-buffer.
 * @param grid_out Buffer för lokator.
 * @param grid_sz Storlek på lokatorbuffer.
 * @return 1 om minst ett fält hittades, annars 0.
 */
int find_call_in_log(const QSOList *log_list, const char *call_in,
                     char *name_out, size_t name_sz,
                     char *qth_out, size_t qth_sz,
                     char *grid_out, size_t grid_sz) {
    if (log_list == NULL || call_in == NULL || call_in[0] == '\0') return 0;
    for (int i = (int)log_list->count - 1; i >= 0; --i) {
        const QSO *q = &log_list->items[i];
        if (strcasecmp(q->call, call_in) == 0) {
            if (q->name[0] != '\0' || q->qth[0] != '\0' || q->gridsquare[0] != '\0') {
                if (name_out && name_sz > 0) copy_str(name_out, name_sz, q->name);
                if (qth_out && qth_sz > 0) copy_str(qth_out, qth_sz, q->qth);
                if (grid_out && grid_sz > 0) copy_str(grid_out, grid_sz, q->gridsquare);
                return 1;
            }
        }
    }
    return 0;
}

/**
 * @brief Utför automatisk uppslagning av namn och QTH för en anropssignal:
 * 1. Kontrollerar först tidigare loggrader.
 * 2. Om inte i loggen, kontrolleras SMCB-cachen.
 * 3. Om SMCB är aktiverat, startas asynkron bakgrundsslagning mot SM Callbook.
 * @param qso Aktuellt QSO.
 * @param log_list Logglistan.
 * @param st Stationsinställningar.
 * @param last_smcb_call Senast efterfrågade signal (undviker dubbla anrop).
 * @param last_smcb_sz Storlek på last_smcb_call.
 * @param status_msg Buffer för statusmeddelande.
 * @param status_sz Storlek på statusbuffert.
 */
void maybe_lookup_call_details(QSO *qso, const QSOList *log_list,
                               const StationSettings *st,
                               char *last_smcb_call, size_t last_smcb_sz,
                               char *status_msg, size_t status_sz) {
    if (qso->call[0] == '\0') return;

    /* 1. Sök i tidigare loggade rader */
    char log_name[64] = "", log_qth[64] = "", log_grid[16] = "";
    if (find_call_in_log(log_list, qso->call, log_name, sizeof(log_name),
                         log_qth, sizeof(log_qth), log_grid, sizeof(log_grid))) {
        if (strcmp(log_name, "-SK-") == 0) {
            copy_str(qso->name, sizeof(qso->name), "-SK-");
            qso->qth[0] = '\0';
            copy_str(qso->comment, sizeof(qso->comment), "SILENT KEY");
            copy_str(status_msg, status_sz, "Från logg: SILENT KEY (-SK-)");
            return;
        }
        if (log_name[0] != '\0' && qso->name[0] == '\0') copy_str(qso->name, sizeof(qso->name), log_name);
        if (log_qth[0] != '\0' && qso->qth[0] == '\0')  copy_str(qso->qth, sizeof(qso->qth), log_qth);
        if (log_grid[0] != '\0' && qso->gridsquare[0] == '\0') copy_str(qso->gridsquare, sizeof(qso->gridsquare), log_grid);
        snprintf(status_msg, status_sz, "Från logg: %s%s%s",
                 log_name, (log_name[0] && log_qth[0]) ? ", " : "", log_qth);
        return;
    }

    char base_call[32];
    int is_swedish = extract_swedish_base_call(qso->call, base_call, sizeof(base_call));

    /* 2. Svensk signal: prova SM Callbook först */
    if (is_swedish && st->smcb_enabled) {
        if (strcasecmp(base_call, last_smcb_call) == 0) return;
        copy_str(last_smcb_call, last_smcb_sz, base_call);

        int was_cached = 0;
        char cached_name[64] = "", cached_qth[64] = "";
        if (lookup_smcb_cache(base_call, cached_name, sizeof(cached_name), cached_qth, sizeof(cached_qth), &was_cached)) {
            if (strcmp(cached_name, "-SK-") == 0) {
                copy_str(qso->name, sizeof(qso->name), "-SK-");
                qso->qth[0] = '\0';
                copy_str(qso->comment, sizeof(qso->comment), "SILENT KEY");
                copy_str(status_msg, status_sz, "SMCB: SILENT KEY (-SK-)");
                return;
            }
            if (cached_name[0] != '\0' && qso->name[0] == '\0') copy_str(qso->name, sizeof(qso->name), cached_name);
            if (cached_qth[0] != '\0' && qso->qth[0] == '\0')  copy_str(qso->qth, sizeof(qso->qth), cached_qth);
            snprintf(status_msg, status_sz, "SMCB: %s, %s", cached_name, cached_qth);
            return;
        }
        if (was_cached) {
            int is_blocked = is_smcb_blocked();
            /* SMCB har ingen träff: prova QRZ.COM om det är aktiverat */
            if (st->qrz_enabled && st->qrz_username[0] != '\0') {
                int qrz_cached = 0;
                char q_name[64] = "", q_qth[64] = "", q_grid[16] = "";
                if (lookup_qrz_cache(qso->call, q_name, sizeof(q_name), q_qth, sizeof(q_qth), q_grid, sizeof(q_grid), &qrz_cached)) {
                    if (strcmp(q_name, "-SK-") == 0) {
                        copy_str(qso->name, sizeof(qso->name), "-SK-");
                        qso->qth[0] = '\0';
                        qso->gridsquare[0] = '\0';
                        copy_str(qso->comment, sizeof(qso->comment), "SILENT KEY");
                        copy_str(status_msg, status_sz, "QRZ: SILENT KEY (-SK-)");
                        return;
                    }
                    if (q_name[0] != '\0' && qso->name[0] == '\0') copy_str(qso->name, sizeof(qso->name), q_name);
                    if (q_qth[0] != '\0' && qso->qth[0] == '\0')  copy_str(qso->qth, sizeof(qso->qth), q_qth);
                    if (q_grid[0] != '\0' && qso->gridsquare[0] == '\0') copy_str(qso->gridsquare, sizeof(qso->gridsquare), q_grid);
                    snprintf(status_msg, status_sz, "QRZ: %s, %s", q_name, q_qth);
                    return;
                }
                if (!qrz_cached) {
                    start_qrz_background_lookup(st->qrz_username, st->qrz_password, qso->call);
                    if (is_blocked) {
                        snprintf(status_msg, status_sz, "SMCB blockerat -> QRZ: Slår upp %s...", qso->call);
                    } else {
                        snprintf(status_msg, status_sz, "SMCB: Ingen träff -> QRZ: Slår upp %s...", qso->call);
                    }
                    return;
                }
            }
            if (is_blocked) {
                snprintf(status_msg, status_sz, "SMCB: Blockerat av SSA webbläsarkontroll (använd QRZ.COM)");
            } else {
                snprintf(status_msg, status_sz, "SMCB: Ingen träff för %s", base_call);
            }
            return;
        }

        start_smcb_background_lookup(base_call);
        snprintf(status_msg, status_sz, "SMCB: Slår upp %s...", base_call);
        return;
    }

    /* 3. Icke-svensk signal (eller svensk om SMCB avstängt): slå upp i QRZ.COM om aktiverat */
    if (st->qrz_enabled && st->qrz_username[0] != '\0') {
        if (strcasecmp(qso->call, last_smcb_call) == 0) return;
        copy_str(last_smcb_call, last_smcb_sz, qso->call);

        int qrz_cached = 0;
        char q_name[64] = "", q_qth[64] = "", q_grid[16] = "";
        if (lookup_qrz_cache(qso->call, q_name, sizeof(q_name), q_qth, sizeof(q_qth), q_grid, sizeof(q_grid), &qrz_cached)) {
            if (strcmp(q_name, "-SK-") == 0) {
                copy_str(qso->name, sizeof(qso->name), "-SK-");
                qso->qth[0] = '\0';
                qso->gridsquare[0] = '\0';
                copy_str(qso->comment, sizeof(qso->comment), "SILENT KEY");
                copy_str(status_msg, status_sz, "QRZ: SILENT KEY (-SK-)");
                return;
            }
            if (q_name[0] != '\0' && qso->name[0] == '\0') copy_str(qso->name, sizeof(qso->name), q_name);
            if (q_qth[0] != '\0' && qso->qth[0] == '\0')  copy_str(qso->qth, sizeof(qso->qth), q_qth);
            if (q_grid[0] != '\0' && qso->gridsquare[0] == '\0') copy_str(qso->gridsquare, sizeof(qso->gridsquare), q_grid);
            if (q_grid[0] != '\0') {
                snprintf(status_msg, status_sz, "QRZ: %s, %s (%s)", q_name, q_qth, q_grid);
            } else {
                snprintf(status_msg, status_sz, "QRZ: %s, %s", q_name, q_qth);
            }
            return;
        }
        if (qrz_cached) {
            snprintf(status_msg, status_sz, "QRZ.COM: Ingen träff för %s", qso->call);
            return;
        }

        start_qrz_background_lookup(st->qrz_username, st->qrz_password, qso->call);
        snprintf(status_msg, status_sz, "QRZ: Slår upp %s...", qso->call);
    }
}

/**
 * @brief Sparar senast valda frekvens, band och trafiksätt till station.conf så att de minns till nästa start.
 * @param qso Aktuellt QSO med radioinställningar.
 * @param st Stationsinställningar.
 */
void persist_qso_radio_settings(const QSO *qso, StationSettings *st) {
    if (qso == NULL || st == NULL) return;
    if (qso->freq[0] != '\0') copy_str(st->last_freq, sizeof(st->last_freq), qso->freq);
    if (qso->band[0] != '\0') copy_str(st->last_band, sizeof(st->last_band), qso->band);
    if (qso->mode[0] != '\0') copy_str(st->last_mode, sizeof(st->last_mode), qso->mode);
    save_station_settings(CONFIG_FILE, st);
}

/**
 * @brief Uppdaterar automatiska förslag och härledda värden när användaren lämnar ett inmatningsfält.
 * @param qso Aktuellt QSO.
 * @param leaving_field Fältet som användaren lämnade.
 * @param log_list Logglistan.
 * @param st Stationsinställningar.
 * @param last_smcb_call Buffer för senast uppslagna signal.
 * @param last_smcb_sz Storlek på last_smcb_call.
 * @param status_msg Statusmeddelandebuffer.
 * @param status_sz Storlek på statusbuffert.
 * @param current_qrb Buffer för QRB-avståndet.
 * @param qrb_sz Storlek på QRB-buffert.
 */
void apply_field_defaults_on_leave(QSO *qso, int leaving_field,
                                   const QSOList *log_list,
                                   StationSettings *st,
                                   char *last_smcb_call, size_t last_smcb_sz,
                                   char *status_msg, size_t status_sz,
                                   char *current_qrb, size_t qrb_sz) {
    if (leaving_field == FLD_CALL) {
        maybe_lookup_call_details(qso, log_list, st, last_smcb_call, last_smcb_sz, status_msg, status_sz);
        if (current_qrb && qrb_sz > 0) {
            compute_qrb_str(st, qso, current_qrb, qrb_sz);
        }
    }
    if (leaving_field == FLD_QTH || leaving_field == FLD_GRID) {
        if (current_qrb && qrb_sz > 0) {
            compute_qrb_str(st, qso, current_qrb, qrb_sz);
        }
    }
    if (leaving_field == FLD_FREQ && qso->freq[0] != '\0') {
        char norm_freq[32];
        parse_and_normalize_freq(qso->freq, norm_freq, sizeof(norm_freq));
        if (norm_freq[0] != '\0') {
            copy_str(qso->freq, sizeof(qso->freq), norm_freq);
        }
        char calc_band[16];
        resolve_band_from_freq(qso->freq, calc_band, sizeof(calc_band));
        if (calc_band[0] != '\0') {
            copy_str(qso->band, sizeof(qso->band), calc_band);
        }
        persist_qso_radio_settings(qso, st);
    }
    if (leaving_field == FLD_MODE && qso->mode[0] != '\0') {
        to_upper_str(qso->mode);
        const char *def_rst = (strcmp(qso->mode, "CW") == 0) ? "599" : "59";
        if (qso->rst_sent[0] == '\0') copy_str(qso->rst_sent, sizeof(qso->rst_sent), def_rst);
        if (qso->rst_rcvd[0] == '\0') copy_str(qso->rst_rcvd, sizeof(qso->rst_rcvd), def_rst);
        persist_qso_radio_settings(qso, st);
    }
}

/**
 * @brief Sätter band och standardfrekvens givet index i BAND_OPTIONS (0..BAND_OPTION_COUNT-1).
 * @param qso Aktuellt QSO.
 * @param st Stationsinställningar.
 * @param idx Index i BAND_OPTIONS.
 */
void set_band_choice(QSO *qso, StationSettings *st, int idx) {
    if (idx >= 0 && idx < BAND_OPTION_COUNT) {
        copy_str(qso->band, sizeof(qso->band), BAND_OPTIONS[idx]);
        copy_str(qso->freq, sizeof(qso->freq), BAND_DEFAULT_FREQ[idx]);
        persist_qso_radio_settings(qso, st);
    }
}

/**
 * @brief Stegar bandvalet i listan (+1 eller -1) och sätter motsvarande standardfrekvens i MHz.
 * @param qso Aktuellt QSO.
 * @param st Stationsinställningar.
 * @param delta +1 för nästa band, -1 för föregående band.
 */
void step_band_choice(QSO *qso, StationSettings *st, int delta) {
    int idx = find_band_index(qso->band);
    if (idx < 0) {
        double f = parse_and_normalize_freq(qso->freq, NULL, 0);
        idx = 5;
        if (f > 0.0) {
            int best = (delta > 0) ? 0 : (BAND_OPTION_COUNT - 1);
            for (int i = 0; i < BAND_OPTION_COUNT; ++i) {
                double bf = atof(BAND_DEFAULT_FREQ[i]);
                if (delta > 0 && bf > f) {
                    best = i;
                    break;
                }
                if (delta < 0 && bf < f) {
                    best = i;
                }
            }
            idx = best;
        }
    } else {
        idx = (idx + delta + BAND_OPTION_COUNT) % BAND_OPTION_COUNT;
    }
    set_band_choice(qso, st, idx);
}

/**
 * @brief Loggar eller uppdaterar aktuellt QSO, sparar till ADIF-filen och återställer formuläret.
 * @param qso Aktuellt QSO.
 * @param time_frozen Flagga för om tiden är fryst.
 * @param active_field Aktivt fält.
 * @param field_cursor_pos Markörposition.
 * @param editing_index Index för redigerat QSO (-1 vid nytt).
 * @param st Stationsinställningar.
 * @param log_list Logglistan.
 * @param last_smcb_call Senast uppslagna anropssignal.
 * @param status_msg Buffer för statusmeddelande.
 * @param status_sz Storlek på statusbuffert.
 * @param current_qrb Buffer för QRB-avstånd.
 */
void try_save_current_qso(QSO *qso, int *time_frozen, int *active_field,
                          int *field_cursor_pos, int *editing_index,
                          StationSettings *st, QSOList *log_list,
                          char *last_smcb_call,
                          char *status_msg, size_t status_sz,
                          char *current_qrb) {
    if (qso->call[0] == '\0') {
        copy_str(status_msg, status_sz, "Ange anropssignal först!");
        *active_field = FLD_CALL;
        *field_cursor_pos = 0;
        return;
    }

    to_upper_str(qso->call);
    to_upper_str(qso->mode);
    to_upper_str(qso->gridsquare);

    if (qso->dxcc[0] == '\0') {
        lookup_dxcc(qso->call, qso->dxcc, sizeof(qso->dxcc));
    }

    format_date_yyyy_mm_dd(qso->date, qso->date, sizeof(qso->date));
    format_time_hh_mm(qso->time_utc, qso->time_utc, sizeof(qso->time_utc));

    if (qso->freq[0] != '\0') {
        char norm_freq[32];
        parse_and_normalize_freq(qso->freq, norm_freq, sizeof(norm_freq));
        if (norm_freq[0] != '\0') {
            copy_str(qso->freq, sizeof(qso->freq), norm_freq);
        }
        char calc_band[16];
        resolve_band_from_freq(qso->freq, calc_band, sizeof(calc_band));
        if (calc_band[0] != '\0') {
            copy_str(qso->band, sizeof(qso->band), calc_band);
        }
    }
    if (qso->band[0] == '\0') {
        copy_str(qso->band, sizeof(qso->band), "20 m");
    } else if (find_band_index(qso->band) >= 0) {
        copy_str(qso->band, sizeof(qso->band), BAND_OPTIONS[find_band_index(qso->band)]);
    }

    if (qso->mode[0] == '\0') {
        copy_str(qso->mode, sizeof(qso->mode), "SSB");
    }
    if (qso->rst_sent[0] == '\0') {
        copy_str(qso->rst_sent, sizeof(qso->rst_sent), strcmp(qso->mode, "CW") == 0 ? "599" : "59");
    }
    if (qso->rst_rcvd[0] == '\0') {
        copy_str(qso->rst_rcvd, sizeof(qso->rst_rcvd), strcmp(qso->mode, "CW") == 0 ? "599" : "59");
    }

    /* Avstånd (ADIF: DISTANCE) i hela kilometer */
    int d_val = 0;
    if (current_qrb != NULL && current_qrb[0] != '\0' && strcmp(current_qrb, "-") != 0) {
        d_val = (int)atol(current_qrb);
    }
    if (d_val <= 0) {
        char temp_qrb[32];
        compute_qrb_str(st, qso, temp_qrb, sizeof(temp_qrb));
        if (temp_qrb[0] != '\0' && strcmp(temp_qrb, "-") != 0) {
            d_val = (int)atol(temp_qrb);
        }
    }
    if (d_val > 0) {
        snprintf(qso->distance, sizeof(qso->distance), "%d", d_val);
    }

    persist_qso_radio_settings(qso, st);

    if (*editing_index >= 0 && (size_t)(*editing_index) < log_list->count) {
        int idx = *editing_index;
        log_list->items[idx] = *qso;
        invalidate_log_stats_cache();
        if (save_all_qsos_adif(LOG_FILE, log_list, st) == 0) {
            snprintf(status_msg, status_sz, "Uppdaterade QSO #%d med %s", idx + 1, qso->call);
            reset_qso_form(qso, st, time_frozen, active_field, field_cursor_pos, editing_index, last_smcb_call, current_qrb);
        } else {
            copy_str(status_msg, status_sz, "Fel: Kunde inte spara ändring till ADIF!");
        }
    } else {
        if (append_qso_adif(LOG_FILE, qso, st) == 0) {
            qso_list_push(log_list, qso);
            snprintf(status_msg, status_sz, "Loggade %s (%s %s)",
                     qso->call, qso->band, qso->mode);
            reset_qso_form(qso, st, time_frozen, active_field, field_cursor_pos, editing_index, last_smcb_call, current_qrb);
        } else {
            copy_str(status_msg, status_sz, "Fel: Kunde inte spara till ADIF!");
        }
    }
}
