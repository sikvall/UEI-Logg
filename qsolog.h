#ifndef QSOLOG_H
#define QSOLOG_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef LOG_FILE
#define LOG_FILE "qsolog.adi"
#endif
#ifndef CONFIG_FILE
#define CONFIG_FILE "station.conf"
#endif
#ifndef DXCC_FILE
#define DXCC_FILE "dxcc.txt"
#endif

#define MIN_COLS 60
#define MIN_LINES 20
#define QSO_FIELD_COUNT 13
#define STATION_FIELD_COUNT 12

#ifndef KEY_CTRL_HOME
#define KEY_CTRL_HOME 2001
#endif
#ifndef KEY_CTRL_END
#define KEY_CTRL_END 2002
#endif

/* Tillåtna band i bandlistan samt standardfrekvens (MHz) med 3 decimaler */
extern const char * const BAND_OPTIONS[];
extern const char * const BAND_DEFAULT_FREQ[];
#define BAND_OPTION_COUNT 14

/* Färgscheman */
extern const char * const THEME_NAMES[];
#define THEME_COUNT 6

/* Fokusläge (F4 växlar mellan inmatning och loggrader) */
typedef enum {
    FOCUS_INPUT = 0,
    FOCUS_LOG   = 1
} FocusMode;

/* Exportläge för ADIF-export */
enum {
    EXPORT_MODE_ALL      = 0,
    EXPORT_MODE_MARKED   = 1,
    EXPORT_MODE_ONLY_NEW = 2
};

/* Fältindex i visnings- och Tab/Pil-ordning */
enum {
    FLD_CALL = 0,
    FLD_NAME = 1,
    FLD_RSTS = 2,
    FLD_RSTR = 3,
    FLD_QTH  = 4,
    FLD_GRID = 5,
    FLD_DXCC = 6,
    FLD_DATE = 7,
    FLD_TIME = 8,
    FLD_FREQ = 9,
    FLD_BAND = 10,
    FLD_MODE = 11,
    FLD_COMM = 12
};

/* Inställningar för egen station */
typedef struct {
    char call[32];       /* ADIF: STATION_CALLSIGN - Egen anropssignal */
    char name[64];       /* ADIF: MY_NAME          - Eget namn */
    char qth[64];        /* ADIF: MY_CITY          - Eget QTH */
    char gridsquare[16]; /* ADIF: MY_GRIDSQUARE    - Egen lokator */
    char timezone[32];   /* Lokal tidszon (t.ex. UTC+1, CET) */
    int  theme;          /* Valt färgschema (0..THEME_COUNT-1) */
    int  smcb_enabled;   /* Slå upp svenska signaler via SM Callbook (0=Av, 1=På) */
    int  auto_rst_59;    /* Fyll i 59 / 59 automatiskt i RST vid nytt QSO (0=Av, 1=På) */
    int  filter_log;     /* Filtrera loggrader efter anropssignal vid inmatning (0=Av, 1=På) */
    char last_freq[32];  /* Senast använda frekvens (sparas i station.conf) */
    char last_band[16];  /* Senast använda band (sparas i station.conf) */
    char last_mode[16];  /* Senast använda trafikslag (sparas i station.conf) */
} StationSettings;

/* Data för ett QSO */
typedef struct {
    char call[32];       /* ADIF: CALL       - Anropssignal */
    char name[64];       /* ADIF: NAME       - Namn */
    char date[16];       /* ADIF: QSO_DATE   - Datum (YYYY-MM-DD) */
    char time_utc[16];   /* ADIF: TIME_ON    - Tid UTC (HH:MM) */
    char gridsquare[16]; /* ADIF: GRIDSQUARE - Lokator */
    char freq[32];       /* ADIF: FREQ       - Frekvens (MHz) */
    char band[16];       /* ADIF: BAND       - Band (160m..2m) */
    char mode[16];       /* ADIF: MODE       - Trafiksätt (t.ex. SSB, CW) */
    char rst_sent[16];   /* ADIF: RST_SENT   - RST Sänt */
    char rst_rcvd[16];   /* ADIF: RST_RCVD   - RST Mott. */
    char qth[64];        /* ADIF: QTH        - QTH */
    char dxcc[64];       /* ADIF: COUNTRY    - DXCC */
    char comment[256];   /* ADIF: COMMENT    - Kommentar */
    char distance[16];   /* ADIF: DISTANCE   - Avstånd i km */
    int  handled;        /* ADIF: APP_QSOLOG_HANDLED - 0 = ny, 1 = hanterad */
    int  marked;         /* 1 om markerad i logglistan */
} QSO;

/* Dynamisk lista av tidigare loggade QSO ("Loggrader") */
typedef struct {
    QSO *items;
    size_t count;
    size_t capacity;
} QSOList;

/* Delade sträng- och UTF-8-funktioner */
void copy_str(char *dest, size_t dest_size, const char *src);
void trim_newline(char *str);
void trim_str(char *str);
void to_upper_str(char *str);
int utf8_display_width(const char *str);
size_t utf8_char_to_byte(const char *str, int char_pos);
void utf8_backspace_at(char *str, int *char_pos);
void utf8_delete_at(char *str, int char_pos);
void utf8_insert_byte_at(char *str, size_t max_size, int *char_pos, char ch);
int utf8_contains_ci(const char *haystack, const char *needle);
const char *get_path_basename(const char *path);

/* Frekvens & Band */
double parse_and_normalize_freq(const char *freq_str, char *norm_out, size_t out_sz);
const char *guess_band_from_freq(const char *freq_str);
void resolve_band_from_freq(const char *freq_str, char *band_out, size_t out_sz);
int find_band_index(const char *band_str);
void format_band_display(const char *in, char *out, size_t out_sz);

/* Datum & Tid */
void format_date_yyyy_mm_dd(const char *raw, char *out, size_t out_sz);
void format_time_hh_mm(const char *raw, char *out, size_t out_sz);
int parse_tz_offset_seconds(const char *tz_str, long *offset_sec_out);
void get_utc_strings(char *date_buf, size_t date_sz,
                     char *time_buf, size_t time_sz,
                     char *top_buf, size_t top_sz);

/* Inställningar & ADIF I/O */
void load_station_settings(const char *filename, StationSettings *st);
int save_station_settings(const char *filename, const StationSettings *st);
int import_qsos_from_adif(const char *filename, QSOList *list, const StationSettings *st);
int export_qsos_adif(const char *filename, QSOList *list, const StationSettings *st, int export_mode);
int clear_log_book(QSOList *log_list, const StationSettings *st);
void build_default_export_filename(char *out, size_t out_sz, const StationSettings *st, int only_new);

#endif /* QSOLOG_H */
