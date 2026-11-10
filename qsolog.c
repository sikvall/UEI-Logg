#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L
#include <ctype.h>
#include <curl/curl.h>
#include <locale.h>
#include <ncurses.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <math.h>
#include <pthread.h>
#include <wchar.h>
#include <wctype.h>

#define LOG_FILE "qsolog.adi"
#define CONFIG_FILE "station.conf"
#define DXCC_FILE "dxcc.txt"

#define MIN_COLS 60
#define MIN_LINES 20
#define QSO_FIELD_COUNT 13
#define STATION_FIELD_COUNT 10

/* Tillåtna band i bandlistan samt standardfrekvens (MHz) med 3 decimaler */
static const char * const BAND_OPTIONS[] = {
    "160 m", "80 m", "60 m", "40 m", "30 m", "20 m",
    "17 m",  "15 m", "12 m", "10 m", "6 m",  "2 m",
    "70 cm", "23 cm"
};
static const char * const BAND_DEFAULT_FREQ[] = {
    "1.900",   "3.600",   "5.400",   "7.100",   "10.100",  "14.200",
    "18.100",  "21.200",  "24.900",  "28.800",  "51.000",  "145.000",
    "432.200", "1296.200"
};
#define BAND_OPTION_COUNT ((int)(sizeof(BAND_OPTIONS) / sizeof(BAND_OPTIONS[0])))

/* Färgscheman inspirerade av Borland Turbo Pascal och Midnight Commander (mc) */
static const char * const THEME_NAMES[] = {
    "Turbo Pascal (MC Default)",
    "MC Darkfar (Svart/Blå)",
    "MC Gotar (Svart/Gul)",
    "MC Sand (Ljusgrå/Blå)",
    "MC Matrix (Fosforgrön)",
    "Ljust (Grå/Orange/Blå)"
};
#define THEME_COUNT ((int)(sizeof(THEME_NAMES) / sizeof(THEME_NAMES[0])))

/* Fokusläge (F4 växlar mellan inmatning och loggrader) */
typedef enum {
    FOCUS_INPUT = 0,
    FOCUS_LOG   = 1
} FocusMode;

/* Färgpar för ncurses TUI */
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
    CP_STATUS_WARN
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
    int  marked;         /* 1 om markerad i logglistan */
} QSO;

/* Fältindex i visnings- och Tab/Pil-ordning:
 * 0: Signal        (Rad 2, Kol 1)
 * 1: Namn          (Rad 2, Kol 2)
 * 2: RST Sänt      (Rad 2, Kol 3)
 * 3: RST Mott.     (Rad 2, Kol 4)
 * 4: QTH           (Rad 3, Kol 1)
 * 5: DXCC          (Rad 3, Kol 2)
 * 6: Lokator       (Rad 3, Kol 3)
 * 7: Datum         (Rad 4, Kol 1)
 * 8: UTC           (Rad 4, Kol 2)
 * 9: Frekvens      (Rad 5, Kol 1)
 * 10: Band         (Rad 5, Kol 2)
 * 11: Trafiksätt   (Rad 5, Kol 3)
 * 12: Kommentar    (Rad 6, Kol 1)
 */
enum {
    FLD_CALL = 0,
    FLD_NAME = 1,
    FLD_RSTS = 2,
    FLD_RSTR = 3,
    FLD_QTH  = 4,
    FLD_DXCC = 5,
    FLD_GRID = 6,
    FLD_DATE = 7,
    FLD_TIME = 8,
    FLD_FREQ = 9,
    FLD_BAND = 10,
    FLD_MODE = 11,
    FLD_COMM = 12
};

/* Dynamisk lista av tidigare loggade QSO ("Loggrader") */
typedef struct {
    QSO *items;
    size_t count;
    size_t capacity;
} QSOList;

/* Post i DXCC-tabellen som läses in från dxcc.txt vid start */
typedef struct {
    char key[24];
    char country[64];
} DxccRule;

typedef struct {
    DxccRule *prefixes;
    size_t prefix_count;
    size_t prefix_cap;
    DxccRule *exacts;
    size_t exact_count;
    size_t exact_cap;
} DxccDatabase;

static DxccDatabase g_dxcc_db = {NULL, 0, 0, NULL, 0, 0};

/* Säker strängkopiering med garanterad nollterminering */
static void copy_str(char *dest, size_t dest_size, const char *src) {
    if (dest_size == 0) return;
    size_t len = strlen(src);
    if (len >= dest_size) {
        len = dest_size - 1;
    }
    memcpy(dest, src, len);
    dest[len] = '\0';
}

/* Trimma radbrytningar och mellanslag i slutet av en sträng */
static void trim_newline(char *str) {
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r' || str[len - 1] == ' ')) {
        str[--len] = '\0';
    }
}

/* Beräkna antalet UTF-8-tecken (synliga tecken) i en sträng */
static int utf8_display_width(const char *str) {
    int width = 0;
    for (const unsigned char *p = (const unsigned char *)str; *p != '\0'; ++p) {
        if ((*p & 0xC0) != 0x80) {
            width++;
        }
    }
    return width;
}

/* Översätt teckenposition (0..utf8_display_width) till byte-offset i en UTF-8-sträng */
static size_t utf8_char_to_byte(const char *str, int char_pos) {
    size_t byte_idx = 0;
    int cur_char = 0;
    while (str[byte_idx] != '\0' && cur_char < char_pos) {
        byte_idx++;
        while (str[byte_idx] != '\0' && ((unsigned char)str[byte_idx] & 0xC0) == 0x80) {
            byte_idx++;
        }
        cur_char++;
    }
    return byte_idx;
}

/* Ta bort ett UTF-8-tecken före markörpositionen (Backspace) */
static void utf8_backspace_at(char *str, int *char_pos) {
    int total_chars = utf8_display_width(str);
    if (*char_pos > total_chars) *char_pos = total_chars;
    if (*char_pos <= 0) return;

    size_t end_b = utf8_char_to_byte(str, *char_pos);
    size_t start_b = utf8_char_to_byte(str, *char_pos - 1);
    size_t tail_len = strlen(str + end_b);
    memmove(str + start_b, str + end_b, tail_len + 1);
    (*char_pos)--;
}

/* Ta bort ett UTF-8-tecken vid markörpositionen (Delete) */
static void utf8_delete_at(char *str, int char_pos) {
    int total_chars = utf8_display_width(str);
    if (char_pos < 0 || char_pos >= total_chars) return;

    size_t start_b = utf8_char_to_byte(str, char_pos);
    size_t end_b = utf8_char_to_byte(str, char_pos + 1);
    size_t tail_len = strlen(str + end_b);
    memmove(str + start_b, str + end_b, tail_len + 1);
}

/* Infoga en byte vid markörpositionen i en UTF-8-sträng */
static void utf8_insert_byte_at(char *str, size_t max_size, int *char_pos, char ch) {
    size_t len = strlen(str);
    if (len + 1 >= max_size) return;

    int total_chars = utf8_display_width(str);
    if (*char_pos < 0) *char_pos = 0;
    if (*char_pos > total_chars) *char_pos = total_chars;

    size_t ins_b = utf8_char_to_byte(str, *char_pos);
    /* Om ch är en UTF-8 continuation-byte (10xxxxxx), lägg den direkt efter föregående lead-byte */
    if (((unsigned char)ch & 0xC0) == 0x80) {
        memmove(str + ins_b + 1, str + ins_b, len - ins_b + 1);
        str[ins_b] = ch;
    } else {
        memmove(str + ins_b + 1, str + ins_b, len - ins_b + 1);
        str[ins_b] = ch;
        (*char_pos)++;
    }
}

/* Gör ASCII-bokstäver i en sträng till versaler */
static void to_upper_str(char *str) {
    for (; *str; ++str) {
        if ((unsigned char)*str < 128) {
            *str = (char)toupper((unsigned char)*str);
        }
    }
}

/* Sorteringsfunktion för DXCC-prefix: längsta prefix först, sedan alfabetiskt */
static int cmp_dxcc_prefix_desc(const void *a, const void *b) {
    const DxccRule *ra = (const DxccRule *)a;
    const DxccRule *rb = (const DxccRule *)b;
    size_t la = strlen(ra->key);
    size_t lb = strlen(rb->key);
    if (la != lb) {
        return (lb > la) ? 1 : -1;
    }
    return strcmp(ra->key, rb->key);
}

/* Läs in DXCC-tabellen från textfilen dxcc.txt vid programstart */
static void load_dxcc_file(const char *filename) {
    FILE *fp = fopen(filename, "r");
    if (fp == NULL) return;

    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        trim_newline(line);
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#') continue;

        char *sep = strchr(p, ';');
        if (sep == NULL) continue;
        *sep = '\0';
        const char *key = p;
        const char *country = sep + 1;

        if (key[0] == '=') {
            key++;
            if (key[0] == '\0') continue;
            if (g_dxcc_db.exact_count >= g_dxcc_db.exact_cap) {
                size_t ncap = (g_dxcc_db.exact_cap == 0) ? 256 : g_dxcc_db.exact_cap * 2;
                DxccRule *narr = realloc(g_dxcc_db.exacts, ncap * sizeof(DxccRule));
                if (!narr) break;
                g_dxcc_db.exacts = narr;
                g_dxcc_db.exact_cap = ncap;
            }
            copy_str(g_dxcc_db.exacts[g_dxcc_db.exact_count].key,
                     sizeof(g_dxcc_db.exacts[0].key), key);
            to_upper_str(g_dxcc_db.exacts[g_dxcc_db.exact_count].key);
            copy_str(g_dxcc_db.exacts[g_dxcc_db.exact_count].country,
                     sizeof(g_dxcc_db.exacts[0].country), country);
            g_dxcc_db.exact_count++;
        } else {
            if (key[0] == '\0') continue;
            if (g_dxcc_db.prefix_count >= g_dxcc_db.prefix_cap) {
                size_t ncap = (g_dxcc_db.prefix_cap == 0) ? 512 : g_dxcc_db.prefix_cap * 2;
                DxccRule *narr = realloc(g_dxcc_db.prefixes, ncap * sizeof(DxccRule));
                if (!narr) break;
                g_dxcc_db.prefixes = narr;
                g_dxcc_db.prefix_cap = ncap;
            }
            copy_str(g_dxcc_db.prefixes[g_dxcc_db.prefix_count].key,
                     sizeof(g_dxcc_db.prefixes[0].key), key);
            to_upper_str(g_dxcc_db.prefixes[g_dxcc_db.prefix_count].key);
            copy_str(g_dxcc_db.prefixes[g_dxcc_db.prefix_count].country,
                     sizeof(g_dxcc_db.prefixes[0].country), country);
            g_dxcc_db.prefix_count++;
        }
    }
    fclose(fp);

    if (g_dxcc_db.prefix_count > 1) {
        qsort(g_dxcc_db.prefixes, g_dxcc_db.prefix_count, sizeof(DxccRule), cmp_dxcc_prefix_desc);
    }
}

static void free_dxcc_db(void) {
    free(g_dxcc_db.prefixes);
    free(g_dxcc_db.exacts);
    memset(&g_dxcc_db, 0, sizeof(g_dxcc_db));
}

static int is_modifier_suffix(const char *s) {
    if (s == NULL || s[0] == '\0') return 1;
    if (strcasecmp(s, "P") == 0 || strcasecmp(s, "M") == 0 ||
        strcasecmp(s, "MM") == 0 || strcasecmp(s, "AM") == 0 ||
        strcasecmp(s, "QRP") == 0 || strcasecmp(s, "A") == 0 ||
        strcasecmp(s, "LH") == 0 || strcasecmp(s, "J") == 0) {
        return 1;
    }
    if (isdigit((unsigned char)s[0]) && s[1] == '\0') {
        return 1;
    }
    return 0;
}

static const char *match_exact_dxcc(const char *call) {
    for (size_t i = 0; i < g_dxcc_db.exact_count; ++i) {
        if (strcmp(call, g_dxcc_db.exacts[i].key) == 0) {
            return g_dxcc_db.exacts[i].country;
        }
    }
    return NULL;
}

static const char *match_prefix_dxcc(const char *call) {
    for (size_t i = 0; i < g_dxcc_db.prefix_count; ++i) {
        const char *pfx = g_dxcc_db.prefixes[i].key;
        size_t plen = strlen(pfx);
        if (strncmp(call, pfx, plen) == 0) {
            if (plen == 1 && call[1] != '\0' && isalpha((unsigned char)call[1]) && call[2] == '\0') {
                char c0 = call[0], c1 = call[1];
                if ((c0 == 'K' || c0 == 'N' || c0 == 'W' || c0 == 'A') &&
                    (c1 == 'H' || c1 == 'L' || c1 == 'P' || c1 == 'G')) {
                    return NULL;
                }
            }
            return g_dxcc_db.prefixes[i].country;
        }
    }
    return NULL;
}

/* Identifiera DXCC-land utifrån motstationens anropssignal */
static void lookup_dxcc(const char *call_in, char *out, size_t out_sz) {
    if (out == NULL || out_sz == 0) return;
    out[0] = '\0';
    if (call_in == NULL) return;

    char call[32];
    size_t j = 0;
    for (size_t i = 0; call_in[i] != '\0' && j < sizeof(call) - 1; ++i) {
        if (!isspace((unsigned char)call_in[i])) {
            call[j++] = (char)toupper((unsigned char)call_in[i]);
        }
    }
    call[j] = '\0';

    if (j < 2) return;

    const char *exact = match_exact_dxcc(call);
    if (exact != NULL) {
        copy_str(out, out_sz, exact);
        return;
    }

    char *slash = strchr(call, '/');
    if (slash != NULL) {
        for (size_t i = 0; i < g_dxcc_db.prefix_count; ++i) {
            const char *pfx = g_dxcc_db.prefixes[i].key;
            if (strchr(pfx, '/') != NULL && strncmp(call, pfx, strlen(pfx)) == 0) {
                copy_str(out, out_sz, g_dxcc_db.prefixes[i].country);
                return;
            }
        }

        char part1[32], part2[32];
        size_t len1 = (size_t)(slash - call);
        if (len1 >= sizeof(part1)) len1 = sizeof(part1) - 1;
        memcpy(part1, call, len1);
        part1[len1] = '\0';
        copy_str(part2, sizeof(part2), slash + 1);
        char *second_slash = strchr(part2, '/');
        if (second_slash) *second_slash = '\0';

        if (is_modifier_suffix(part2)) {
            if (strlen(part1) >= 2) {
                const char *c1 = match_exact_dxcc(part1);
                if (!c1) c1 = match_prefix_dxcc(part1);
                if (c1) copy_str(out, out_sz, c1);
            }
            return;
        }

        const char *cand = (strlen(part1) <= strlen(part2) && strlen(part1) >= 1) ? part1 : part2;
        const char *cc = match_prefix_dxcc(cand);
        if (cc != NULL) {
            copy_str(out, out_sz, cc);
            return;
        }
    }

    const char *pfx_country = match_prefix_dxcc(call);
    if (pfx_country != NULL) {
        copy_str(out, out_sz, pfx_country);
    }
}

/* Kontrollera om anropssignalen är en svensk anropssignal (SA-SM, 7S eller 8S)
 * och extrahera bassignalen utan t.ex. /P eller /MM */
static int extract_swedish_base_call(const char *call_in, char *base_out, size_t out_sz) {
    if (call_in == NULL || call_in[0] == '\0') return 0;

    char clean[32];
    copy_str(clean, sizeof(clean), call_in);
    to_upper_str(clean);

    /* Om portabel (t.ex. SM5UEI/P eller LA/SM5UEI), hitta den del som har siffra */
    char *slash = strchr(clean, '/');
    if (slash != NULL) {
        *slash = '\0';
        const char *part2 = slash + 1;
        int p1_digit = 0, p2_digit = 0;
        for (const char *p = clean; *p; ++p) if (isdigit((unsigned char)*p)) p1_digit = 1;
        for (const char *p = part2; *p; ++p) if (isdigit((unsigned char)*p)) p2_digit = 1;
        if (!p1_digit && p2_digit) {
            copy_str(clean, sizeof(clean), part2);
            char *s2 = strchr(clean, '/');
            if (s2) *s2 = '\0';
        }
    }

    size_t len = strlen(clean);
    if (len < 4 || len > 7) return 0;

    int is_se_prefix = 0;
    if (clean[0] == 'S' && clean[1] >= 'A' && clean[1] <= 'M') {
        is_se_prefix = 1;
    } else if ((clean[0] == '7' || clean[0] == '8') && clean[1] == 'S') {
        is_se_prefix = 1;
    }
    if (!is_se_prefix) return 0;

    if (!isdigit((unsigned char)clean[2])) return 0;
    size_t i = 3;
    while (i < len && isdigit((unsigned char)clean[i])) {
        i++;
    }
    if (i >= len) return 0; /* Måste avslutas med minst en bokstav */
    for (; i < len; ++i) {
        if (!isalpha((unsigned char)clean[i])) return 0;
    }

    if (base_out && out_sz > 0) {
        copy_str(base_out, out_sz, clean);
    }
    return 1;
}

/* Base64-kodning (används för token = btoa(Date.now()) till ssa.se/ssa/smcb/) */
static void base64_encode_str(const char *in, char *out, size_t out_sz) {
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    size_t len = strlen(in);
    size_t j = 0;
    for (size_t i = 0; i < len && j + 4 < out_sz; i += 3) {
        unsigned int v = (unsigned char)in[i] << 16;
        if (i + 1 < len) v |= (unsigned char)in[i + 1] << 8;
        if (i + 2 < len) v |= (unsigned char)in[i + 2];

        out[j++] = b64[(v >> 18) & 0x3F];
        out[j++] = b64[(v >> 12) & 0x3F];
        out[j++] = (i + 1 < len) ? b64[(v >> 6) & 0x3F] : '=';
        out[j++] = (i + 2 < len) ? b64[v & 0x3F] : '=';
    }
    out[j] = '\0';
}

typedef struct {
    char *data;
    size_t len;
} CurlBuf;

static size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
    size_t total = size * nmemb;
    CurlBuf *buf = (CurlBuf *)userdata;
    char *grown = realloc(buf->data, buf->len + total + 1);
    if (grown == NULL) return 0;
    buf->data = grown;
    memcpy(buf->data + buf->len, ptr, total);
    buf->len += total;
    buf->data[buf->len] = '\0';
    return total;
}

/* Utför ett HTTP-uppslag mot SM Callbook för en given anropssignal.
 * - Om signalen hittas direkt i medlemsregistret (<span class="block">), fylls
 *   name_out (endast förnamn) och qth_out i och funktionen returnerar 1.
 * - Om signalen är en extrasignal (t.ex. SM0W -> SM0WKA), fylls alt_call_out i
 *   med huvudsignalen och funktionen returnerar 0 så att ett andra uppslag kan göras. */
static int fetch_sm_callbook_once(const char *query_call,
                                  char *name_out, size_t name_sz,
                                  char *qth_out, size_t qth_sz,
                                  char *alt_call_out, size_t alt_call_sz) {
    if (alt_call_out && alt_call_sz > 0) alt_call_out[0] = '\0';

    char ms_str[32], token_b64[64];
    long long ms_now = (long long)time(NULL) * 1000LL;
    snprintf(ms_str, sizeof(ms_str), "%lld", ms_now);
    base64_encode_str(ms_str, token_b64, sizeof(token_b64));

    char url[256];
    snprintf(url, sizeof(url),
             "https://www.ssa.se/ssa/smcb/?call=%s&fnamn=&token=%s&enamn=&ort=",
             query_call, token_b64);

    CURL *curl = curl_easy_init();
    if (curl == NULL) return 0;

    CurlBuf buf = {NULL, 0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_REFERER, "https://www.ssa.se/ssa/smcb/");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) qsolog/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 4L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || buf.data == NULL) {
        free(buf.data);
        return 0;
    }

    /* Tolka HTML-svaret från SM Callbook:
       OBS: Inuti <strong>...</strong> finns <span style="color: green;">Medlem</span>!
       Därför måste vi söka efter </span> EFTER </strong><br>! */
    int found = 0;
    char *block = strstr(buf.data, "<span class=\"block\">");
    if (block != NULL) {
        char *after_strong = strstr(block, "</strong><br>");
        if (after_strong != NULL) {
            char *end_span = strstr(after_strong, "</span>");
            char *name_start = after_strong + strlen("</strong><br>");
            char *name_end = strstr(name_start, "<br>");
            if (name_end != NULL && end_span != NULL && name_end < end_span) {
                size_t nlen = (size_t)(name_end - name_start);
                char extracted_name[64];
                if (nlen >= sizeof(extracted_name)) nlen = sizeof(extracted_name) - 1;
                memcpy(extracted_name, name_start, nlen);
                extracted_name[nlen] = '\0';
                trim_newline(extracted_name);

                /* Ta bara med förnamnet (första namnet), trunkera efternamn/mellannamn */
                char *np = extracted_name;
                while (*np == ' ' || *np == '\t') np++;
                char *space = np;
                while (*space != '\0' && *space != ' ' && *space != '\t') space++;
                *space = '\0';
                char first_name[64];
                copy_str(first_name, sizeof(first_name), np);

                /* Hitta sista <br> före end_span för postnummer + ort */
                char *last_br = name_end;
                char *scan = name_end + 4;
                while (scan < end_span) {
                    char *next_br = strstr(scan, "<br>");
                    if (next_br != NULL && next_br < end_span) {
                        last_br = next_br;
                        scan = next_br + 4;
                    } else {
                        break;
                    }
                }

                char extracted_qth[64] = "";
                if (last_br < end_span) {
                    const char *city_line = last_br + 4;
                    size_t clen = (size_t)(end_span - city_line);
                    char raw_city[64];
                    if (clen >= sizeof(raw_city)) clen = sizeof(raw_city) - 1;
                    memcpy(raw_city, city_line, clen);
                    raw_city[clen] = '\0';
                    trim_newline(raw_city);

                    /* Hoppa förbi svenskt postnummer (t.ex. "819 62 " eller "162 70 ") */
                    const char *cp = raw_city;
                    while (*cp != '\0' && (isdigit((unsigned char)*cp) || *cp == ' ' || *cp == '-')) {
                        cp++;
                    }
                    copy_str(extracted_qth, sizeof(extracted_qth), cp);
                }

                if (first_name[0] != '\0') {
                    if (name_out && name_sz > 0) copy_str(name_out, name_sz, first_name);
                    if (qth_out && qth_sz > 0 && extracted_qth[0] != '\0') {
                        copy_str(qth_out, qth_sz, extracted_qth);
                    }
                    found = 1;
                }
            }
        }
    }

    /* Om inget direkt medlemsblock hittades, kontrollera om svaret är en Extrasignal
       (t.ex. <td>SM0W</td><td>SM0WKA</td> eller onclick="document.location = '/ssa/smcb/?call=SM0WKA';") */
    if (!found && alt_call_out != NULL && alt_call_sz > 0) {
        char td_pattern[48];
        snprintf(td_pattern, sizeof(td_pattern), "<td>%s</td><td>", query_call);
        char *td_match = strstr(buf.data, td_pattern);
        if (td_match != NULL) {
            const char *cstart = td_match + strlen(td_pattern);
            size_t cj = 0;
            while (cstart[cj] != '\0' && cstart[cj] != '<' &&
                   !isspace((unsigned char)cstart[cj]) && cj < alt_call_sz - 1) {
                alt_call_out[cj] = (char)toupper((unsigned char)cstart[cj]);
                cj++;
            }
            alt_call_out[cj] = '\0';
        } else {
            const char *link_pat = "/ssa/smcb/?call=";
            char *lmatch = strstr(buf.data, link_pat);
            if (lmatch != NULL) {
                const char *cstart = lmatch + strlen(link_pat);
                size_t cj = 0;
                while (cstart[cj] != '\0' && cstart[cj] != '\'' && cstart[cj] != '"' &&
                       cstart[cj] != '&' && cstart[cj] != '<' &&
                       !isspace((unsigned char)cstart[cj]) && cj < alt_call_sz - 1) {
                    alt_call_out[cj] = (char)toupper((unsigned char)cstart[cj]);
                    cj++;
                }
                alt_call_out[cj] = '\0';
            }
        }
    }

    free(buf.data);
    return found;
}

/* Slå upp svensk anropssignal (SA-SM, 7S, 8S) via SM Callbook: https://www.ssa.se/ssa/smcb/
 * Om det första svaret är en extrasignal (t.ex. SM0W -> SM0WKA), görs automatiskt ett
 * andra uppslag på huvudsignalen (SM0WKA) för att få tag i förnamn och QTH. */
static int lookup_sm_callbook(const char *call_in,
                              char *name_out, size_t name_sz,
                              char *qth_out, size_t qth_sz) {
    char base_call[32];
    if (!extract_swedish_base_call(call_in, base_call, sizeof(base_call))) {
        return 0;
    }

    char alt_call[32] = "";
    if (fetch_sm_callbook_once(base_call, name_out, name_sz, qth_out, qth_sz,
                               alt_call, sizeof(alt_call))) {
        return 1;
    }

    /* Om första uppslaget returnerade en annan signal (extrasignal -> huvudsignal),
       gör ett andra uppslag på den signalen! */
    if (alt_call[0] != '\0' && strcasecmp(alt_call, base_call) != 0) {
        if (fetch_sm_callbook_once(alt_call, name_out, name_sz, qth_out, qth_sz,
                                   NULL, 0)) {
            return 1;
        }
    }

    return 0;
}

/* Cache för SM Callbook (signaler -> namn, QTH) */
typedef struct {
    char call[32];
    char name[64];
    char qth[64];
    int found;
} SmcbCacheEntry;

static SmcbCacheEntry g_smcb_cache[128];
static size_t g_smcb_cache_count = 0;
static pthread_mutex_t g_smcb_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Delat tillstånd för asynkront SM Callbook-uppslag i bakgrunden */
typedef struct {
    int active;             /* 1 om en bakgrundstråd körs */
    int req_id;             /* Inkrementellt ID för aktuell förfrågan */
    char req_call[32];      /* Signalen som efterfrågas */

    int has_result;         /* 1 om tråden är klar och resultatet inte hämtats */
    int res_id;             /* req_id för det färdiga resultatet */
    int res_found;
    char res_call[32];
    char res_name[64];
    char res_qth[64];
} SmcbAsyncState;

static SmcbAsyncState g_smcb_async;
static volatile int g_smcb_abort = 0;

static int lookup_smcb_cache(const char *call_in, char *name_out, size_t name_sz,
                             char *qth_out, size_t qth_sz, int *was_cached) {
    if (was_cached) *was_cached = 0;
    pthread_mutex_lock(&g_smcb_mutex);
    for (size_t i = 0; i < g_smcb_cache_count; ++i) {
        if (strcasecmp(g_smcb_cache[i].call, call_in) == 0) {
            if (was_cached) *was_cached = 1;
            int found = g_smcb_cache[i].found;
            if (found) {
                if (name_out && name_sz > 0) copy_str(name_out, name_sz, g_smcb_cache[i].name);
                if (qth_out && qth_sz > 0) copy_str(qth_out, qth_sz, g_smcb_cache[i].qth);
            }
            pthread_mutex_unlock(&g_smcb_mutex);
            return found;
        }
    }
    pthread_mutex_unlock(&g_smcb_mutex);
    return 0;
}

static void store_smcb_cache(const char *call_in, const char *name, const char *qth, int found) {
    pthread_mutex_lock(&g_smcb_mutex);
    for (size_t i = 0; i < g_smcb_cache_count; ++i) {
        if (strcasecmp(g_smcb_cache[i].call, call_in) == 0) {
            g_smcb_cache[i].found = found;
            copy_str(g_smcb_cache[i].name, sizeof(g_smcb_cache[i].name), name ? name : "");
            copy_str(g_smcb_cache[i].qth, sizeof(g_smcb_cache[i].qth), qth ? qth : "");
            pthread_mutex_unlock(&g_smcb_mutex);
            return;
        }
    }
    if (g_smcb_cache_count < sizeof(g_smcb_cache) / sizeof(g_smcb_cache[0])) {
        copy_str(g_smcb_cache[g_smcb_cache_count].call, sizeof(g_smcb_cache[0].call), call_in);
        copy_str(g_smcb_cache[g_smcb_cache_count].name, sizeof(g_smcb_cache[0].name), name ? name : "");
        copy_str(g_smcb_cache[g_smcb_cache_count].qth, sizeof(g_smcb_cache[0].qth), qth ? qth : "");
        g_smcb_cache[g_smcb_cache_count].found = found;
        g_smcb_cache_count++;
    }
    pthread_mutex_unlock(&g_smcb_mutex);
}

typedef struct {
    char call[32];
    int req_id;
} SmcbThreadArg;

static void *smcb_worker_thread(void *arg) {
    SmcbThreadArg *targ = (SmcbThreadArg *)arg;
    char query_call[32];
    copy_str(query_call, sizeof(query_call), targ->call);
    int req_id = targ->req_id;
    free(targ);

    char name[64] = "", qth[64] = "";
    int found = lookup_sm_callbook(query_call, name, sizeof(name), qth, sizeof(qth));

    if (g_smcb_abort) return NULL;

    store_smcb_cache(query_call, name, qth, found);

    pthread_mutex_lock(&g_smcb_mutex);
    if (!g_smcb_abort && req_id == g_smcb_async.req_id) {
        g_smcb_async.has_result = 1;
        g_smcb_async.res_id = req_id;
        g_smcb_async.res_found = found;
        copy_str(g_smcb_async.res_call, sizeof(g_smcb_async.res_call), query_call);
        copy_str(g_smcb_async.res_name, sizeof(g_smcb_async.res_name), name);
        copy_str(g_smcb_async.res_qth, sizeof(g_smcb_async.res_qth), qth);
    }
    g_smcb_async.active = 0;
    pthread_mutex_unlock(&g_smcb_mutex);

    return NULL;
}

static void start_smcb_background_lookup(const char *base_call) {
    if (base_call == NULL || base_call[0] == '\0') return;

    pthread_mutex_lock(&g_smcb_mutex);
    if (g_smcb_async.active && strcasecmp(g_smcb_async.req_call, base_call) == 0) {
        pthread_mutex_unlock(&g_smcb_mutex);
        return;
    }

    g_smcb_async.req_id++;
    copy_str(g_smcb_async.req_call, sizeof(g_smcb_async.req_call), base_call);
    g_smcb_async.active = 1;
    g_smcb_async.has_result = 0;

    SmcbThreadArg *targ = malloc(sizeof(SmcbThreadArg));
    if (!targ) {
        g_smcb_async.active = 0;
        pthread_mutex_unlock(&g_smcb_mutex);
        return;
    }
    copy_str(targ->call, sizeof(targ->call), base_call);
    targ->req_id = g_smcb_async.req_id;

    pthread_t tid;
    if (pthread_create(&tid, NULL, smcb_worker_thread, targ) == 0) {
        pthread_detach(tid);
    } else {
        free(targ);
        g_smcb_async.active = 0;
    }
    pthread_mutex_unlock(&g_smcb_mutex);
}

/* Kontrollera om en sträng är en giltig och komplett Maidenhead-lokator (4 eller 6 tecken).
 * Returnerar 4 eller 6 vid giltig lokator, annars 0. */
static int is_valid_locator(const char *loc) {
    if (loc == NULL) return 0;
    while (*loc == ' ' || *loc == '\t') loc++;
    size_t len = strlen(loc);
    while (len > 0 && (loc[len - 1] == ' ' || loc[len - 1] == '\t' || loc[len - 1] == '\r' || loc[len - 1] == '\n')) len--;
    if (len != 4 && len != 6) return 0;

    char l0 = (char)toupper((unsigned char)loc[0]);
    char l1 = (char)toupper((unsigned char)loc[1]);
    char l2 = loc[2];
    char l3 = loc[3];

    if (l0 < 'A' || l0 > 'R' || l1 < 'A' || l1 > 'R') return 0;
    if (!isdigit((unsigned char)l2) || !isdigit((unsigned char)l3)) return 0;

    if (len == 6) {
        char l4 = (char)toupper((unsigned char)loc[4]);
        char l5 = (char)toupper((unsigned char)loc[5]);
        if (l4 < 'A' || l4 > 'X' || l5 < 'A' || l5 > 'X') return 0;
    }
    return (int)len;
}

/* Konvertera Maidenhead locator (4 eller 6 tecken) till latitud och longitud (grader).
 * Returnerar 1 om giltig, annars 0. */
static int locator_to_latlon(const char *loc, double *lat_out, double *lon_out) {
    int vlen = is_valid_locator(loc);
    if (!vlen) return 0;

    while (*loc == ' ' || *loc == '\t') loc++;
    char l0 = (char)toupper((unsigned char)loc[0]);
    char l1 = (char)toupper((unsigned char)loc[1]);
    char l2 = loc[2];
    char l3 = loc[3];

    double lon = (l0 - 'A') * 20.0 - 180.0 + (l2 - '0') * 2.0;
    double lat = (l1 - 'A') * 10.0 - 90.0 + (l3 - '0') * 1.0;

    if (vlen == 6) {
        char l4 = (char)toupper((unsigned char)loc[4]);
        char l5 = (char)toupper((unsigned char)loc[5]);
        lon += (l4 - 'A') * (5.0 / 60.0) + (2.5 / 60.0);
        lat += (l5 - 'A') * (2.5 / 60.0) + (1.25 / 60.0);
    } else {
        lon += 1.0;
        lat += 0.5;
    }

    if (lat_out) *lat_out = lat;
    if (lon_out) *lon_out = lon;
    return 1;
}

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Beräkna storcirkelavstånd (haversine) i kilometer mellan två koordinater */
static double calculate_distance_km(double lat1, double lon1, double lat2, double lon2) {
    double rad = M_PI / 180.0;
    double dlat = (lat2 - lat1) * rad;
    double dlon = (lon2 - lon1) * rad;
    double rlat1 = lat1 * rad;
    double rlat2 = lat2 * rad;

    double a = sin(dlat / 2.0) * sin(dlat / 2.0) +
               cos(rlat1) * cos(rlat2) * sin(dlon / 2.0) * sin(dlon / 2.0);
    double c = 2.0 * atan2(sqrt(a), sqrt(1.0 - a));
    return 6371.0 * c;
}

/* Cache för QTH -> koordinater så vi inte gör onödiga HTTP-anrop */
typedef struct {
    char qth[64];
    double lat;
    double lon;
    int valid;
} QthCoordCache;

static QthCoordCache g_qth_cache[64];
static size_t g_qth_cache_count = 0;

static int lookup_qth_cache(const char *qth, double *lat, double *lon) {
    for (size_t i = 0; i < g_qth_cache_count; ++i) {
        if (strcasecmp(g_qth_cache[i].qth, qth) == 0) {
            if (g_qth_cache[i].valid) {
                *lat = g_qth_cache[i].lat;
                *lon = g_qth_cache[i].lon;
                return 1;
            }
            return 0;
        }
    }
    return -1; /* Inte i cache */
}

static void store_qth_cache(const char *qth, double lat, double lon, int valid) {
    if (g_qth_cache_count < sizeof(g_qth_cache) / sizeof(g_qth_cache[0])) {
        copy_str(g_qth_cache[g_qth_cache_count].qth, sizeof(g_qth_cache[0].qth), qth);
        g_qth_cache[g_qth_cache_count].lat = lat;
        g_qth_cache[g_qth_cache_count].lon = lon;
        g_qth_cache[g_qth_cache_count].valid = valid;
        g_qth_cache_count++;
    }
}

/* Slå upp koordinater för en ort (QTH) via OpenStreetMap Nominatim */
static int geocode_qth_osm(const char *qth, double *lat_out, double *lon_out) {
    if (qth == NULL || qth[0] == '\0') return 0;
    while (*qth == ' ' || *qth == '\t') qth++;
    if (qth[0] == '\0') return 0;

    double clat = 0, clon = 0;
    int cstatus = lookup_qth_cache(qth, &clat, &clon);
    if (cstatus == 1) {
        *lat_out = clat;
        *lon_out = clon;
        return 1;
    } else if (cstatus == 0) {
        return 0;
    }

    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    char *escaped = curl_easy_escape(curl, qth, 0);
    if (!escaped) {
        curl_easy_cleanup(curl);
        return 0;
    }

    char url[256];
    snprintf(url, sizeof(url), "https://nominatim.openstreetmap.org/search?q=%s&format=json&limit=1", escaped);
    curl_free(escaped);

    CurlBuf buf = {NULL, 0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "qsolog-uei/1.0 (amateur radio log; contact: sm5uei@avitec.se)");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 2L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    int found = 0;
    if (res == CURLE_OK && buf.data != NULL) {
        char *lat_str = strstr(buf.data, "\"lat\":\"");
        char *lon_str = strstr(buf.data, "\"lon\":\"");
        if (lat_str && lon_str) {
            double lat_val = atof(lat_str + 7);
            double lon_val = atof(lon_str + 7);
            if (lat_val != 0.0 || lon_val != 0.0) {
                *lat_out = lat_val;
                *lon_out = lon_val;
                found = 1;
            }
        }
    }
    free(buf.data);

    store_qth_cache(qth, found ? *lat_out : 0.0, found ? *lon_out : 0.0, found);
    return found;
}

/* Beräkna QRB (avstånd i km) mellan min station och motstationen.
 * 1. Provar lokator mot lokator om motstationen har giltig, komplett lokator (4 eller 6 tecken).
 * 2. Om lokator är tom och QTH finns, slås koordinater upp via geokodning.
 * Om lokator är ofullständig eller koordinater saknas, visas "-". */
static void compute_qrb_str(const StationSettings *my_st, const QSO *target_qso, char *out, size_t out_sz) {
    if (out == NULL || out_sz == 0) return;
    copy_str(out, out_sz, "-");

    if (my_st == NULL || target_qso == NULL) return;

    double my_lat = 0, my_lon = 0;
    int have_my_pos = 0;

    if (locator_to_latlon(my_st->gridsquare, &my_lat, &my_lon)) {
        have_my_pos = 1;
    } else if (my_st->qth[0] != '\0' && geocode_qth_osm(my_st->qth, &my_lat, &my_lon)) {
        have_my_pos = 1;
    }
    if (!have_my_pos) return;

    double tg_lat = 0, tg_lon = 0;
    int have_tg_pos = 0;

    if (locator_to_latlon(target_qso->gridsquare, &tg_lat, &tg_lon)) {
        have_tg_pos = 1;
    } else if (target_qso->gridsquare[0] == '\0' && target_qso->qth[0] != '\0') {
        if (geocode_qth_osm(target_qso->qth, &tg_lat, &tg_lon)) {
            have_tg_pos = 1;
        }
    }
    if (!have_tg_pos) return;

    double dist_km = calculate_distance_km(my_lat, my_lon, tg_lat, tg_lon);
    long rounded_km = (long)(dist_km + 0.5);
    snprintf(out, out_sz, "%ld km", rounded_km);
}

/* Tolka och normalisera frekvensinmatning till MHz med 3 decimaler (punkt som decimaltecken). */
static double parse_and_normalize_freq(const char *in, char *norm_out, size_t out_sz) {
    if (in == NULL) {
        if (norm_out && out_sz > 0) norm_out[0] = '\0';
        return 0.0;
    }

    while (*in == ' ' || *in == '\t') in++;
    if (*in == '\0') {
        if (norm_out && out_sz > 0) norm_out[0] = '\0';
        return 0.0;
    }

    const char *sep = NULL;
    for (const char *p = in; *p != '\0'; ++p) {
        if (*p == '.' || *p == ',') {
            sep = p;
            break;
        }
    }

    double mhz = 0.0;

    if (sep != NULL) {
        char int_digits[16];
        size_t ij = 0;
        for (const char *p = in; p < sep && ij < sizeof(int_digits) - 1; ++p) {
            if (isdigit((unsigned char)*p)) {
                int_digits[ij++] = *p;
            }
        }
        int_digits[ij] = '\0';
        unsigned long int_part = (ij > 0) ? strtoul(int_digits, NULL, 10) : 0;

        char frac_digits[16];
        size_t fj = 0;
        for (const char *p = sep + 1; *p != '\0' && fj < sizeof(frac_digits) - 1; ++p) {
            if (isdigit((unsigned char)*p)) {
                frac_digits[fj++] = *p;
            }
        }
        frac_digits[fj] = '\0';

        double frac_val = 0.0;
        double scale = 0.1;
        for (size_t i = 0; i < fj; ++i) {
            frac_val += (frac_digits[i] - '0') * scale;
            scale *= 0.1;
        }
        mhz = (double)int_part + frac_val;
    } else {
        char khz_digits[24];
        size_t kj = 0;
        for (const char *p = in; *p != '\0' && kj < sizeof(khz_digits) - 1; ++p) {
            if (isdigit((unsigned char)*p)) {
                khz_digits[kj++] = *p;
            }
        }
        khz_digits[kj] = '\0';

        if (kj > 0) {
            unsigned long khz = strtoul(khz_digits, NULL, 10);
            mhz = (double)khz / 1000.0;
        }
    }

    if (norm_out && out_sz > 0) {
        if (mhz > 0.0) {
            snprintf(norm_out, out_sz, "%.3f", mhz);
        } else {
            norm_out[0] = '\0';
        }
    }
    return mhz;
}

/* Föreslå amatörradioband (ur listan 160 m..23 cm) utifrån frekvens */
static const char *guess_band_from_freq(const char *freq_str) {
    if (freq_str == NULL || freq_str[0] == '\0') return "";
    double f = parse_and_normalize_freq(freq_str, NULL, 0);
    if (f >= 1.8 && f <= 2.0)       return "160 m";
    if (f >= 3.5 && f <= 3.8)       return "80 m";
    if (f >= 5.3 && f <= 5.4)       return "60 m";
    if (f >= 7.0 && f <= 7.2)       return "40 m";
    if (f >= 10.1 && f <= 10.15)    return "30 m";
    if (f >= 14.0 && f <= 14.35)    return "20 m";
    if (f >= 18.068 && f <= 18.168) return "17 m";
    if (f >= 21.0 && f <= 21.45)    return "15 m";
    if (f >= 24.89 && f <= 24.99)   return "12 m";
    if (f >= 28.0 && f <= 29.7)     return "10 m";
    if (f >= 50.0 && f <= 52.0)     return "6 m";
    if (f >= 144.0 && f <= 146.0)   return "2 m";
    if (f >= 430.0 && f <= 440.0)   return "70 cm";
    if (f >= 1240.0 && f <= 1300.0) return "23 cm";
    return "";
}

/* Bestäm band utifrån frekvens:
 * Om frekvensen tillhör något av amatörradiobanden används det bandet.
 * Annars beräknas metervärdet enligt 300 / f avrundat till närmaste heltal (t.ex. "75 m", "11 m"). */
static void resolve_band_from_freq(const char *freq_str, char *band_out, size_t out_sz) {
    if (band_out == NULL || out_sz == 0) return;
    band_out[0] = '\0';
    if (freq_str == NULL || freq_str[0] == '\0') return;

    const char *known = guess_band_from_freq(freq_str);
    if (known[0] != '\0') {
        copy_str(band_out, out_sz, known);
        return;
    }

    double f = parse_and_normalize_freq(freq_str, NULL, 0);
    if (f > 0.0) {
        long meters = (long)(300.0 / f + 0.5);
        if (meters < 1) meters = 1;
        if (meters > 999999) meters = 999999;
        snprintf(band_out, out_sz, "%ld m", meters);
    }
}

static int find_band_index(const char *band_str) {
    if (band_str == NULL || band_str[0] == '\0') return -1;
    char norm[16];
    size_t j = 0;
    for (size_t i = 0; band_str[i] != '\0' && j < sizeof(norm) - 1; ++i) {
        if (band_str[i] != ' ' && band_str[i] != '\t') {
            norm[j++] = band_str[i];
        }
    }
    norm[j] = '\0';

    size_t len = strlen(norm);
    if (len > 0 && isdigit((unsigned char)norm[len - 1]) && len + 1 < sizeof(norm)) {
        norm[len] = 'm';
        norm[len + 1] = '\0';
    }

    for (int i = 0; i < BAND_OPTION_COUNT; ++i) {
        char opt_clean[16];
        size_t oj = 0;
        for (size_t k = 0; BAND_OPTIONS[i][k] != '\0' && oj < sizeof(opt_clean) - 1; ++k) {
            if (BAND_OPTIONS[i][k] != ' ' && BAND_OPTIONS[i][k] != '\t') {
                opt_clean[oj++] = BAND_OPTIONS[i][k];
            }
        }
        opt_clean[oj] = '\0';
        if (strcasecmp(norm, opt_clean) == 0) {
            return i;
        }
    }
    return -1;
}

/* Formatera bandnamn för användargränssnittet med mellanslag (t.ex. "80 m", "70 cm") */
static void format_band_display(const char *in, char *out, size_t out_sz) {
    if (in == NULL || out == NULL || out_sz == 0) return;
    int idx = find_band_index(in);
    if (idx >= 0) {
        copy_str(out, out_sz, BAND_OPTIONS[idx]);
        return;
    }
    char digits[8], unit[8];
    size_t di = 0, ui = 0;
    for (size_t i = 0; in[i] != '\0'; ++i) {
        if (isdigit((unsigned char)in[i])) {
            if (di < sizeof(digits) - 1) digits[di++] = in[i];
        } else if (isalpha((unsigned char)in[i])) {
            if (ui < sizeof(unit) - 1) unit[ui++] = in[i];
        }
    }
    digits[di] = '\0';
    unit[ui] = '\0';
    if (di > 0 && ui > 0) {
        snprintf(out, out_sz, "%.6s %-.4s", digits, unit);
    } else if (di > 0) {
        snprintf(out, out_sz, "%.6s m", digits);
    } else {
        copy_str(out, out_sz, in);
    }
}

/* Normalisera bandnamn enligt ADIF 3.1.8-specifikationen utan mellanslag (t.ex. "80m", "70cm") */
static void normalize_band_adif(const char *in, char *out, size_t out_sz) {
    if (in == NULL || out == NULL || out_sz == 0) return;
    size_t j = 0;
    for (size_t i = 0; in[i] != '\0' && j < out_sz - 1; ++i) {
        if (in[i] != ' ' && in[i] != '\t') {
            out[j++] = (char)tolower((unsigned char)in[i]);
        }
    }
    out[j] = '\0';
}

static void normalize_date_adif(const char *in, char *out, size_t out_sz) {
    char clean[16];
    size_t j = 0;
    for (size_t i = 0; in[i] != '\0' && j < sizeof(clean) - 1; ++i) {
        if (isdigit((unsigned char)in[i])) {
            clean[j++] = in[i];
        }
    }
    clean[j] = '\0';
    if (j == 8) {
        copy_str(out, out_sz, clean);
    } else {
        copy_str(out, out_sz, in);
    }
}

static void normalize_time_adif(const char *in, char *out, size_t out_sz) {
    char clean[16];
    size_t j = 0;
    for (size_t i = 0; in[i] != '\0' && j < sizeof(clean) - 1; ++i) {
        if (isdigit((unsigned char)in[i])) {
            clean[j++] = in[i];
        }
    }
    clean[j] = '\0';
    if (j >= 4) {
        clean[4] = '\0';
        copy_str(out, out_sz, clean);
    } else {
        copy_str(out, out_sz, in);
    }
}

static void format_date_yyyy_mm_dd(const char *raw, char *out, size_t out_sz) {
    char digits[16];
    normalize_date_adif(raw, digits, sizeof(digits));
    if (strlen(digits) == 8) {
        snprintf(out, out_sz, "%.4s-%.2s-%.2s", digits, digits + 4, digits + 6);
    } else {
        copy_str(out, out_sz, raw);
    }
}

static void format_time_hh_mm(const char *raw, char *out, size_t out_sz) {
    char digits[16];
    normalize_time_adif(raw, digits, sizeof(digits));
    if (strlen(digits) >= 4) {
        snprintf(out, out_sz, "%.2s:%.2s", digits, digits + 2);
    } else {
        copy_str(out, out_sz, raw);
    }
}

/* Läs in stationsinställningar från station.conf (SMCB är default 0 = avstängd) */
static void load_station_settings(const char *filename, StationSettings *st) {
    memset(st, 0, sizeof(*st));
    st->smcb_enabled = 0;
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

static int save_station_settings(const char *filename, const StationSettings *st) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) return -1;
    fprintf(fp, "CALL=%s\n", st->call);
    fprintf(fp, "NAME=%s\n", st->name);
    fprintf(fp, "QTH=%s\n", st->qth);
    fprintf(fp, "GRIDSQUARE=%s\n", st->gridsquare);
    fprintf(fp, "TIMEZONE=%s\n", st->timezone);
    fprintf(fp, "THEME=%d\n", st->theme);
    fprintf(fp, "SMCB=%d\n", st->smcb_enabled ? 1 : 0);
    fprintf(fp, "AUTO_RST=%d\n", st->auto_rst_59 ? 1 : 0);
    fprintf(fp, "FILTER_LOG=%d\n", st->filter_log ? 1 : 0);
    fprintf(fp, "FREQ=%s\n", st->last_freq);
    fprintf(fp, "BAND=%s\n", st->last_band);
    fprintf(fp, "MODE=%s\n", st->last_mode);
    fclose(fp);
    return 0;
}

static void write_adif_field(FILE *fp, const char *tag, const char *val) {
    if (val != NULL && val[0] != '\0') {
        fprintf(fp, "<%s:%zu>%s ", tag, strlen(val), val);
    }
}

static void write_adif_header(FILE *fp) {
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

static void write_qso_record_adif(FILE *fp, const QSO *qso, const StationSettings *st) {
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
    if (st != NULL) {
        write_adif_field(fp, "STATION_CALLSIGN", st->call);
        write_adif_field(fp, "OPERATOR", st->call);
        write_adif_field(fp, "MY_NAME", st->name);
        write_adif_field(fp, "MY_CITY", st->qth);
        write_adif_field(fp, "MY_GRIDSQUARE", st->gridsquare);
    }
    fprintf(fp, "<EOR>\n");
}

static int append_qso_adif(const char *filename, const QSO *qso, const StationSettings *st) {
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

static int save_all_qsos_adif(const char *filename, const QSOList *list, const StationSettings *st) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) return -1;

    write_adif_header(fp);

    for (size_t i = 0; i < list->count; ++i) {
        write_qso_record_adif(fp, &list->items[i], st);
    }

    fclose(fp);
    return 0;
}

static void assign_adif_field(QSO *qso, const char *tag, const char *val) {
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
    }
}

static void qso_list_push(QSOList *list, const QSO *qso) {
    if (list->count >= list->capacity) {
        size_t new_cap = (list->capacity == 0) ? 32 : list->capacity * 2;
        QSO *new_items = realloc(list->items, new_cap * sizeof(QSO));
        if (new_items == NULL) return;
        list->items = new_items;
        list->capacity = new_cap;
    }
    list->items[list->count++] = *qso;
}

static void qso_list_delete_at(QSOList *list, size_t index) {
    if (index >= list->count) return;
    for (size_t i = index; i + 1 < list->count; ++i) {
        list->items[i] = list->items[i + 1];
    }
    list->count--;
}

static int qso_list_delete_marked(QSOList *list) {
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
    return deleted;
}

static void load_qsos_from_adif(const char *filename, QSOList *list) {
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
}

static char *get_qso_field_ptr(QSO *qso, int idx, size_t *max_len) {
    switch (idx) {
        case FLD_CALL: *max_len = sizeof(qso->call);       return qso->call;
        case FLD_NAME: *max_len = sizeof(qso->name);       return qso->name;
        case FLD_DATE: *max_len = sizeof(qso->date);       return qso->date;
        case FLD_TIME: *max_len = sizeof(qso->time_utc);   return qso->time_utc;
        case FLD_GRID: *max_len = sizeof(qso->gridsquare); return qso->gridsquare;
        case FLD_FREQ: *max_len = sizeof(qso->freq);       return qso->freq;
        case FLD_BAND: *max_len = sizeof(qso->band);       return qso->band;
        case FLD_MODE: *max_len = sizeof(qso->mode);       return qso->mode;
        case FLD_RSTS: *max_len = sizeof(qso->rst_sent);   return qso->rst_sent;
        case FLD_RSTR: *max_len = sizeof(qso->rst_rcvd);   return qso->rst_rcvd;
        case FLD_QTH:  *max_len = sizeof(qso->qth);        return qso->qth;
        case FLD_DXCC: *max_len = sizeof(qso->dxcc);       return qso->dxcc;
        case FLD_COMM: *max_len = sizeof(qso->comment);    return qso->comment;
        default:       *max_len = 0;                       return NULL;
    }
}

static void sync_cursor_to_field_end(QSO *qso, int active_field, int *cursor_pos) {
    size_t max_len = 0;
    char *ptr = get_qso_field_ptr(qso, active_field, &max_len);
    *cursor_pos = ptr ? utf8_display_width(ptr) : 0;
}

/* Returnera vilken kolumn (0..3) ett inmatningsfält tillhör */
static int field_default_col(int fld) {
    switch (fld) {
        case FLD_CALL:
        case FLD_QTH:
        case FLD_DATE:
        case FLD_FREQ:
        case FLD_COMM:
            return 0;
        case FLD_NAME:
        case FLD_DXCC:
        case FLD_TIME:
        case FLD_BAND:
            return 1;
        case FLD_RSTS:
        case FLD_GRID:
        case FLD_MODE:
            return 2;
        case FLD_RSTR:
            return 3;
        default:
            return 0;
    }
}

/* Returnera vilken rad (0..4) ett inmatningsfält ligger på */
static int field_row_index(int fld) {
    switch (fld) {
        case FLD_CALL:
        case FLD_NAME:
        case FLD_RSTS:
        case FLD_RSTR:
            return 0;
        case FLD_QTH:
        case FLD_DXCC:
        case FLD_GRID:
            return 1;
        case FLD_DATE:
        case FLD_TIME:
            return 2;
        case FLD_FREQ:
        case FLD_BAND:
        case FLD_MODE:
            return 3;
        case FLD_COMM:
        default:
            return 4;
    }
}

/* Flytta vertikalt (dir = +1 för Pil ned, dir = -1 för Pil upp) till närmaste fält
 * på raden under eller över det fält man står i. */
static int step_field_vertical(int current_field, int dir, int pref_col) {
    int r = field_row_index(current_field);
    int target_row = (r + dir + 5) % 5;

    switch (target_row) {
        case 0:
            if (pref_col == 0) return FLD_CALL;
            if (pref_col == 1) return FLD_NAME;
            if (pref_col == 2) return FLD_RSTS;
            return FLD_RSTR;
        case 1:
            if (pref_col == 0) return FLD_QTH;
            if (pref_col == 1) return FLD_DXCC;
            return FLD_GRID;
        case 2:
            if (pref_col == 0) return FLD_DATE;
            return FLD_TIME;
        case 3:
            if (pref_col == 0) return FLD_FREQ;
            if (pref_col == 1) return FLD_BAND;
            return FLD_MODE;
        case 4:
        default:
            return FLD_COMM;
    }
}

/* Tolka tidszon-offset från inställningar (t.ex. "+0100", "+02:00", "+1", "-0500", "UTC+2").
 * Returnerar 1 om giltig offset hittades och sätter *offset_sec_out, annars 0 (använd locale). */
static int parse_tz_offset_seconds(const char *tz_str, long *offset_sec_out) {
    if (tz_str == NULL) return 0;
    const char *p = tz_str;
    while (*p == ' ' || *p == '\t') p++;
    if (strncasecmp(p, "UTC", 3) == 0 || strncasecmp(p, "GMT", 3) == 0) {
        p += 3;
        while (*p == ' ' || *p == '\t') p++;
    }
    if (*p == '\0') return 0;

    int sign = 1;
    if (*p == '+') {
        sign = 1;
        p++;
    } else if (*p == '-') {
        sign = -1;
        p++;
    }

    if (!isdigit((unsigned char)*p)) return 0;

    int hours = 0, mins = 0;
    const char *colon = strchr(p, ':');
    if (colon != NULL) {
        hours = atoi(p);
        mins  = atoi(colon + 1);
    } else {
        char digits[8];
        size_t dj = 0;
        while (isdigit((unsigned char)p[dj]) && dj < sizeof(digits) - 1) {
            digits[dj] = p[dj];
            dj++;
        }
        digits[dj] = '\0';
        if (dj == 0) return 0;
        if (dj <= 2) {
            hours = atoi(digits);
            mins  = 0;
        } else {
            int val = atoi(digits);
            hours = val / 100;
            mins  = val % 100;
        }
    }

    if (hours > 14 || mins < 0 || mins >= 60) return 0;
    *offset_sec_out = (long)sign * ((long)hours * 3600L + (long)mins * 60L);
    return 1;
}

static void get_utc_strings(char *date_buf, size_t date_sz,
                            char *time_buf, size_t time_sz,
                            char *top_buf, size_t top_sz) {
    time_t now = time(NULL);
    struct tm *utc = gmtime(&now);
    if (utc != NULL) {
        if (date_buf) strftime(date_buf, date_sz, "%Y-%m-%d", utc);
        if (time_buf) strftime(time_buf, time_sz, "%H:%M", utc);
        if (top_buf)  strftime(top_buf, top_sz, "%Y-%m-%d %H:%M UTC", utc);
    }
}

/* Formatera både UTC och lokal tid för statusraden längst upp.
 * Tar hänsyn till offset i inställningarna om den finns, annars används systemets locale. */
static void format_top_bar_clocks(const StationSettings *st, char *out, size_t out_sz) {
    time_t now = time(NULL);
    struct tm utc_tm;
    struct tm loc_tm;

    struct tm *u = gmtime(&now);
    if (u == NULL) {
        if (out_sz > 0) out[0] = '\0';
        return;
    }
    utc_tm = *u;

    long offset_sec = 0;
    if (st != NULL && parse_tz_offset_seconds(st->timezone, &offset_sec)) {
        time_t adj = now + offset_sec;
        struct tm *l = gmtime(&adj);
        if (l != NULL) {
            loc_tm = *l;
        } else {
            loc_tm = utc_tm;
        }
    } else {
        struct tm *l = localtime(&now);
        if (l != NULL) {
            loc_tm = *l;
        } else {
            loc_tm = utc_tm;
        }
    }

    char utc_part[32], loc_part[16];
    strftime(utc_part, sizeof(utc_part), "%Y-%m-%d %H:%M UTC", &utc_tm);
    strftime(loc_part, sizeof(loc_part), "%H:%M", &loc_tm);

    if (COLS >= 74) {
        snprintf(out, out_sz, "%s  |  Lokal: %s", utc_part, loc_part);
    } else {
        snprintf(out, out_sz, "%s (%s)", utc_part, loc_part);
    }
}

/* Rensa inmatningsformuläret för nytt QSO men behåll Frekvens, Band och Trafiksätt */
static void reset_qso_form(QSO *qso, const StationSettings *st, int *time_frozen, int *active_field,
                           int *field_cursor_pos, int *editing_index, char *last_smcb_call,
                           char *current_qrb) {
    char saved_freq[32], saved_band[16], saved_mode[16];
    copy_str(saved_freq, sizeof(saved_freq), qso->freq);
    copy_str(saved_band, sizeof(saved_band), qso->band);
    copy_str(saved_mode, sizeof(saved_mode), qso->mode);

    memset(qso, 0, sizeof(*qso));
    *time_frozen = 0;
    *active_field = FLD_CALL;
    *field_cursor_pos = 0;
    *editing_index = -1;
    if (last_smcb_call) last_smcb_call[0] = '\0';
    if (current_qrb) copy_str(current_qrb, 32, "-");

    copy_str(qso->band, sizeof(qso->band), saved_band[0] ? saved_band : "20 m");
    copy_str(qso->freq, sizeof(qso->freq), saved_freq[0] ? saved_freq : "14.200");
    copy_str(qso->mode, sizeof(qso->mode), saved_mode);

    if (st != NULL && st->auto_rst_59) {
        copy_str(qso->rst_sent, sizeof(qso->rst_sent), "59");
        copy_str(qso->rst_rcvd, sizeof(qso->rst_rcvd), "59");
    }

    get_utc_strings(qso->date, sizeof(qso->date),
                    qso->time_utc, sizeof(qso->time_utc),
                    NULL, 0);
}

/* Aktivera valt färgschema (0..THEME_COUNT-1) – utan separat bakgrundsfärg i inmatningsfälten */
static void apply_color_theme(int theme_idx) {
    if (!has_colors()) return;
    start_color();

    if (theme_idx < 0 || theme_idx >= THEME_COUNT) {
        theme_idx = 0;
    }

    switch (theme_idx) {
        case 0: /* 1. Turbo Pascal / MC Default (Klassisk blå bakgrund) */
            init_pair(CP_MAIN_BG,        COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_TOPBAR,         COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_TOPBAR_ACCENT,  COLOR_RED,    COLOR_WHITE);
            init_pair(CP_BORDER,         COLOR_CYAN,   COLOR_BLUE);
            init_pair(CP_BORDER_ACTIVE,  COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_TITLE,          COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_LABEL,          COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_FIELD_INACTIVE, COLOR_CYAN,   COLOR_BLUE);
            init_pair(CP_FIELD_ACTIVE,   COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_FROZEN_BADGE,   COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_LIVE_BADGE,     COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_EDIT_BADGE,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_TABLE_HEADER,   COLOR_CYAN,   COLOR_BLUE);
            init_pair(CP_LOG_ROW,        COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_LOG_SELECTED,   COLOR_BLACK,  COLOR_CYAN);
            init_pair(CP_LOG_MARKED,     COLOR_YELLOW, COLOR_MAGENTA);
            init_pair(CP_FKEY_NUM,       COLOR_RED,    COLOR_WHITE);
            init_pair(CP_FKEY_LABEL,     COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_STATUS_OK,      COLOR_GREEN,  COLOR_BLUE);
            init_pair(CP_STATUS_WARN,    COLOR_WHITE,  COLOR_RED);
            break;

        case 1: /* 2. MC Darkfar (Svart bakgrund, blå/cyan lister, gul/vit text) */
            init_pair(CP_MAIN_BG,        COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_TOPBAR,         COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_TOPBAR_ACCENT,  COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_BORDER,         COLOR_BLUE,   COLOR_BLACK);
            init_pair(CP_BORDER_ACTIVE,  COLOR_CYAN,   COLOR_BLACK);
            init_pair(CP_TITLE,          COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_LABEL,          COLOR_CYAN,   COLOR_BLACK);
            init_pair(CP_FIELD_INACTIVE, COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_FIELD_ACTIVE,   COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_FROZEN_BADGE,   COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_LIVE_BADGE,     COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_EDIT_BADGE,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_TABLE_HEADER,   COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_LOG_ROW,        COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_LOG_SELECTED,   COLOR_BLACK,  COLOR_CYAN);
            init_pair(CP_LOG_MARKED,     COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_FKEY_NUM,       COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_FKEY_LABEL,     COLOR_BLACK,  COLOR_CYAN);
            init_pair(CP_STATUS_OK,      COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_STATUS_WARN,    COLOR_WHITE,  COLOR_RED);
            break;

        case 2: /* 3. MC Gotar (Svart bakgrund, bärnstensgul/grön stil) */
            init_pair(CP_MAIN_BG,        COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_TOPBAR,         COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_TOPBAR_ACCENT,  COLOR_RED,    COLOR_YELLOW);
            init_pair(CP_BORDER,         COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_BORDER_ACTIVE,  COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_TITLE,          COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_LABEL,          COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_FIELD_INACTIVE, COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_FIELD_ACTIVE,   COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_FROZEN_BADGE,   COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_LIVE_BADGE,     COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_EDIT_BADGE,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_TABLE_HEADER,   COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_LOG_ROW,        COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_LOG_SELECTED,   COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_LOG_MARKED,     COLOR_WHITE,  COLOR_GREEN);
            init_pair(CP_FKEY_NUM,       COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_FKEY_LABEL,     COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_STATUS_OK,      COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_STATUS_WARN,    COLOR_WHITE,  COLOR_RED);
            break;

        case 3: /* 4. MC Sand / Gray (Ljusgrå/vit bakgrund med blå och svarta accenter) */
            init_pair(CP_MAIN_BG,        COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_TOPBAR,         COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_TOPBAR_ACCENT,  COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_BORDER,         COLOR_BLUE,   COLOR_WHITE);
            init_pair(CP_BORDER_ACTIVE,  COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_TITLE,          COLOR_BLUE,   COLOR_WHITE);
            init_pair(CP_LABEL,          COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_FIELD_INACTIVE, COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_FIELD_ACTIVE,   COLOR_BLUE,   COLOR_WHITE);
            init_pair(CP_FROZEN_BADGE,   COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_LIVE_BADGE,     COLOR_WHITE,  COLOR_GREEN);
            init_pair(CP_EDIT_BADGE,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_TABLE_HEADER,   COLOR_BLUE,   COLOR_WHITE);
            init_pair(CP_LOG_ROW,        COLOR_BLACK,  COLOR_WHITE);
            init_pair(CP_LOG_SELECTED,   COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_LOG_MARKED,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_FKEY_NUM,       COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_FKEY_LABEL,     COLOR_WHITE,  COLOR_CYAN);
            init_pair(CP_STATUS_OK,      COLOR_BLUE,   COLOR_WHITE);
            init_pair(CP_STATUS_WARN,    COLOR_WHITE,  COLOR_RED);
            break;

        case 4: /* 5. MC Matrix (Klassisk fosforgrön terminalskärm på svart bakgrund) */
            init_pair(CP_MAIN_BG,        COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_TOPBAR,         COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_TOPBAR_ACCENT,  COLOR_YELLOW, COLOR_BLACK);
            init_pair(CP_BORDER,         COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_BORDER_ACTIVE,  COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_TITLE,          COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_LABEL,          COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_FIELD_INACTIVE, COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_FIELD_ACTIVE,   COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_FROZEN_BADGE,   COLOR_BLACK,  COLOR_YELLOW);
            init_pair(CP_LIVE_BADGE,     COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_EDIT_BADGE,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_TABLE_HEADER,   COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_LOG_ROW,        COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_LOG_SELECTED,   COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_LOG_MARKED,     COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_FKEY_NUM,       COLOR_GREEN,  COLOR_BLACK);
            init_pair(CP_FKEY_LABEL,     COLOR_BLACK,  COLOR_GREEN);
            init_pair(CP_STATUS_OK,      COLOR_WHITE,  COLOR_BLACK);
            init_pair(CP_STATUS_WARN,    COLOR_WHITE,  COLOR_RED);
            break;

        case 5: { /* 6. Ljust färgschema (Ljusgrå bakgrund, svart text, orange ramar, blå highlights) */
            short bg_gray   = (COLORS >= 256) ? 252 : COLOR_WHITE;
            short fg_orange = (COLORS >= 256) ? 208 : COLOR_RED;
            init_pair(CP_MAIN_BG,        COLOR_BLACK,  bg_gray);
            init_pair(CP_TOPBAR,         COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_TOPBAR_ACCENT,  COLOR_YELLOW, COLOR_BLUE);
            init_pair(CP_BORDER,         fg_orange,    bg_gray);
            init_pair(CP_BORDER_ACTIVE,  fg_orange,    bg_gray);
            init_pair(CP_TITLE,          fg_orange,    bg_gray);
            init_pair(CP_LABEL,          COLOR_BLACK,  bg_gray);
            init_pair(CP_FIELD_INACTIVE, COLOR_BLACK,  bg_gray);
            init_pair(CP_FIELD_ACTIVE,   COLOR_BLUE,   bg_gray);
            init_pair(CP_FROZEN_BADGE,   COLOR_WHITE,  fg_orange);
            init_pair(CP_LIVE_BADGE,     COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_EDIT_BADGE,     COLOR_WHITE,  COLOR_MAGENTA);
            init_pair(CP_TABLE_HEADER,   COLOR_BLUE,   bg_gray);
            init_pair(CP_LOG_ROW,        COLOR_BLACK,  bg_gray);
            init_pair(CP_LOG_SELECTED,   COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_LOG_MARKED,     COLOR_WHITE,  fg_orange);
            init_pair(CP_FKEY_NUM,       COLOR_WHITE,  fg_orange);
            init_pair(CP_FKEY_LABEL,     COLOR_WHITE,  COLOR_BLUE);
            init_pair(CP_STATUS_OK,      COLOR_BLUE,   bg_gray);
            init_pair(CP_STATUS_WARN,    COLOR_WHITE,  COLOR_RED);
            break;
        }
    }

    bkgd(COLOR_PAIR(CP_MAIN_BG));
}

/* Rita en ram (box) med titel och markering om sektionen har fokus */
static void draw_box_with_title(int y, int x, int h, int w, const char *title, int has_focus) {
    if (h < 2 || w < 4) return;
    int color = has_focus ? CP_BORDER_ACTIVE : CP_BORDER;
    int attr  = has_focus ? A_BOLD : 0;
    attron(COLOR_PAIR(color) | attr);
    mvaddch(y, x, ACS_ULCORNER);
    mvhline(y, x + 1, ACS_HLINE, w - 2);
    mvaddch(y, x + w - 1, ACS_URCORNER);

    for (int r = y + 1; r < y + h - 1; ++r) {
        mvaddch(r, x, ACS_VLINE);
        mvaddch(r, x + w - 1, ACS_VLINE);
    }

    mvaddch(y + h - 1, x, ACS_LLCORNER);
    mvhline(y + h - 1, x + 1, ACS_HLINE, w - 2);
    mvaddch(y + h - 1, x + w - 1, ACS_LRCORNER);
    attroff(COLOR_PAIR(color) | attr);

    if (title != NULL && title[0] != '\0') {
        attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvprintw(y, x + 2, " %s ", title);
        attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
    }
}

/* Rita den översta statusraden (inklusive UTC och lokal tid) */
static void draw_top_bar(const StationSettings *st) {
    char clock_str[80] = "";
    format_top_bar_clocks(st, clock_str, sizeof(clock_str));

    const char *my_call = st->call[0] ? st->call : "(ange i F12)";
    const char *my_loc  = st->gridsquare[0] ? st->gridsquare : "----";

    attron(COLOR_PAIR(CP_TOPBAR));
    mvhline(0, 0, ' ', COLS);
    mvprintw(0, 1, "Station: ");
    attroff(COLOR_PAIR(CP_TOPBAR));

    attron(COLOR_PAIR(CP_TOPBAR_ACCENT) | A_BOLD);
    printw("%s", my_call);
    attroff(COLOR_PAIR(CP_TOPBAR_ACCENT) | A_BOLD);

    attron(COLOR_PAIR(CP_TOPBAR));
    if (COLS >= 88 && st->qth[0] != '\0') {
        printw(" (%s)", st->qth);
    }
    printw("  |  Lokator: ");
    attroff(COLOR_PAIR(CP_TOPBAR));

    attron(COLOR_PAIR(CP_TOPBAR_ACCENT) | A_BOLD);
    printw("%s", my_loc);
    attroff(COLOR_PAIR(CP_TOPBAR_ACCENT) | A_BOLD);

    int clock_len = (int)strlen(clock_str);
    int clock_col = COLS - clock_len - 1;
    if (clock_col > 1) {
        attron(COLOR_PAIR(CP_TOPBAR));
        mvprintw(0, clock_col, "%s", clock_str);
        attroff(COLOR_PAIR(CP_TOPBAR));
    }
}

/* Rita ett inmatningsfält där etikettens ':' hamnar exakt på kolumn colon_col
 * och markören placeras på cursor_pos inom fältet. */
static void draw_field_aligned(int row, int colon_col, const char *label, const char *val,
                               int width, int is_active, int cursor_pos,
                               int *cur_y, int *cur_x) {
    if (width < 2) width = 2;

    int label_w = utf8_display_width(label);
    int label_col = colon_col - label_w;
    if (label_col < 1) label_col = 1;

    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(row, label_col, "%s:", label);
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

    int fcol = colon_col + 2;

    if (is_active) {
        attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        for (int i = 0; i < width; ++i) {
            mvaddch(row, fcol + i, ' ');
        }
    } else {
        attron(COLOR_PAIR(CP_FIELD_INACTIVE));
        for (int i = 0; i < width; ++i) {
            mvaddch(row, fcol + i, ' ');
        }
    }

    int total_chars = utf8_display_width(val);
    int start_char = 0;
    int cpos = is_active ? cursor_pos : 0;
    if (cpos < 0) cpos = 0;
    if (cpos > total_chars) cpos = total_chars;

    if (is_active && cpos >= width) {
        start_char = cpos - width + 1;
    }

    size_t start_byte = utf8_char_to_byte(val, start_char);
    size_t end_byte   = utf8_char_to_byte(val, start_char + width);
    int byte_len = (int)(end_byte - start_byte);
    mvprintw(row, fcol, "%.*s", byte_len, val + start_byte);

    if (is_active) {
        attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        int screen_offset = cpos - start_char;
        if (screen_offset < 0) screen_offset = 0;
        if (screen_offset >= width) screen_offset = width - 1;
        *cur_y = row;
        *cur_x = fcol + screen_offset;
    } else {
        attroff(COLOR_PAIR(CP_FIELD_INACTIVE));
    }
}

/* Rita Band-väljarfältet (< 20m >) linjerat med ':' på colon_col */
static void draw_band_selector_aligned(int row, int colon_col, const char *val,
                                       int is_active, int band_selecting,
                                       int *cur_y, int *cur_x) {
    const char *label = "Band";
    int label_w = utf8_display_width(label);
    int label_col = colon_col - label_w;
    if (label_col < 1) label_col = 1;

    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
    mvprintw(row, label_col, "%s:", label);
    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

    int fcol = colon_col + 2;
    const char *shown = (val && val[0]) ? val : "20 m";

    if (is_active) {
        int attr = A_BOLD | (band_selecting ? A_REVERSE : 0);
        attron(COLOR_PAIR(CP_FIELD_ACTIVE) | attr);
        mvprintw(row, fcol, "<%-5.5s>", shown);
        attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | attr);
        *cur_y = row;
        *cur_x = fcol + 1;
    } else {
        attron(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
        mvprintw(row, fcol, "<%-5.5s>", shown);
        attroff(COLOR_PAIR(CP_FIELD_INACTIVE) | A_BOLD);
    }
}

/* Rita raden för bandval när Band är aktivt */
static void draw_band_popup_list(int row, const char *current_band, int band_selecting) {
    int x = 2;
    if (!band_selecting) {
        attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvprintw(row, x, " Band: Tryck Enter för att välja band ");
        attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
        return;
    }

    int cur_idx = find_band_index(current_band);
    attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
    if (COLS >= 95) {
        mvprintw(row, x, "Band (Pilar väljer, Enter=Klar): ");
        x += 33;
    } else {
        mvprintw(row, x, "Band: ");
        x += 6;
    }
    attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);

    static const char * const SHORT_NAMES[] = {
        "160", "80", "60", "40", "30", "20", "17", "15", "12", "10", "6", "2", "70cm", "23cm"
    };

    int start_i = 0;
    if (cur_idx >= 7 && COLS < 80) {
        start_i = cur_idx - 5;
        if (start_i + 8 > BAND_OPTION_COUNT) start_i = BAND_OPTION_COUNT - 8;
        if (start_i < 0) start_i = 0;
    }

    for (int i = start_i; i < BAND_OPTION_COUNT && x + 5 < COLS - 1; ++i) {
        if (i == cur_idx) {
            attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD | A_REVERSE);
            mvprintw(row, x, "[%s]", SHORT_NAMES[i]);
            attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD | A_REVERSE);
            x += (int)strlen(SHORT_NAMES[i]) + 2;
        } else {
            attron(COLOR_PAIR(CP_FIELD_INACTIVE));
            mvprintw(row, x, " %s ", SHORT_NAMES[i]);
            attroff(COLOR_PAIR(CP_FIELD_INACTIVE));
            x += (int)strlen(SHORT_NAMES[i]) + 2;
        }
    }
}

/* Rita funktionstangentmenyn allra längst ned */
static void draw_bottom_fkey_bar(int mode_type, FocusMode focus_mode, const char *search_filter) {
    int row = LINES - 1;
    attron(COLOR_PAIR(CP_FKEY_LABEL));
    mvhline(row, 0, ' ', COLS);
    attroff(COLOR_PAIR(CP_FKEY_LABEL));

    struct {
        const char *key;
        const char *label;
    } btns_main[] = {
        {"F1/Esc", "Nytt"},
        {"Enter",  focus_mode == FOCUS_LOG ? "Redigera" : "Logga"},
        {"F3",     (search_filter && search_filter[0]) ? "Sök*" : "Sök"},
        {"F4",     focus_mode == FOCUS_LOG ? "Inmatn." : "Loggrad"},
        {"Ins",    focus_mode == FOCUS_LOG ? "Märk" : NULL},
        {"F8",     focus_mode == FOCUS_LOG ? "Radera" : NULL},
        {"F9",     "Hjälp"},
        {"F10",    "Avsluta"},
        {"F12",    "Inställn."}
    };

    struct {
        const char *key;
        const char *label;
    } btns_modal[] = {
        {"Enter/F2",  "Spara"},
        {"Tab/Pilar", "Byt fält"},
        {"Esc",       "Tillbaka"}
    };

    int col = 1;
    if (mode_type == 0) {
        int count = (int)(sizeof(btns_main) / sizeof(btns_main[0]));
        for (int i = 0; i < count; ++i) {
            if (btns_main[i].label == NULL) continue;
            int btn_len = (int)strlen(btns_main[i].key) + (int)strlen(btns_main[i].label) + 2;
            if (col + btn_len >= COLS) break;

            attron(COLOR_PAIR(CP_FKEY_NUM) | A_BOLD);
            mvprintw(row, col, "%s", btns_main[i].key);
            col += (int)strlen(btns_main[i].key);
            attroff(COLOR_PAIR(CP_FKEY_NUM) | A_BOLD);

            attron(COLOR_PAIR(CP_FKEY_LABEL));
            mvprintw(row, col, " %-s ", btns_main[i].label);
            col += (int)strlen(btns_main[i].label) + 2;
            attroff(COLOR_PAIR(CP_FKEY_LABEL));
        }
    } else {
        int count = (int)(sizeof(btns_modal) / sizeof(btns_modal[0]));
        for (int i = 0; i < count && col < COLS - 4; ++i) {
            attron(COLOR_PAIR(CP_FKEY_NUM) | A_BOLD);
            mvprintw(row, col, "%s", btns_modal[i].key);
            col += (int)strlen(btns_modal[i].key);
            attroff(COLOR_PAIR(CP_FKEY_NUM) | A_BOLD);

            attron(COLOR_PAIR(CP_FKEY_LABEL));
            mvprintw(row, col, " %-s  ", btns_modal[i].label);
            col += (int)strlen(btns_modal[i].label) + 3;
            attroff(COLOR_PAIR(CP_FKEY_LABEL));
        }
    }
}

/* Skiftlägesokänslig delsökning som hanterar både ASCII och UTF-8 (t.ex. å, ä, ö) */
static int utf8_contains_ci(const char *haystack, const char *needle) {
    if (needle == NULL || needle[0] == '\0') return 1;
    if (haystack == NULL || haystack[0] == '\0') return 0;

    size_t h_len = strlen(haystack);
    size_t n_len = strlen(needle);

    wchar_t stack_h[128];
    wchar_t stack_n[64];
    wchar_t *w_haystack = (h_len < 128) ? stack_h : malloc((h_len + 1) * sizeof(wchar_t));
    wchar_t *w_needle   = (n_len < 64)  ? stack_n : malloc((n_len + 1) * sizeof(wchar_t));

    int found = 0;
    if (w_haystack != NULL && w_needle != NULL) {
        size_t h_res = mbstowcs(w_haystack, haystack, h_len + 1);
        size_t n_res = mbstowcs(w_needle, needle, n_len + 1);
        if (h_res != (size_t)-1 && n_res != (size_t)-1) {
            for (size_t i = 0; i < h_res; ++i) {
                w_haystack[i] = towupper(w_haystack[i]);
            }
            for (size_t i = 0; i < n_res; ++i) {
                w_needle[i] = towupper(w_needle[i]);
            }
            found = (wcsstr(w_haystack, w_needle) != NULL);
            if (w_haystack != stack_h) free(w_haystack);
            if (w_needle != stack_n) free(w_needle);
            return found;
        }
    }
    if (w_haystack && w_haystack != stack_h) free(w_haystack);
    if (w_needle && w_needle != stack_n) free(w_needle);

    return (strcasestr(haystack, needle) != NULL);
}

static int build_filtered_indices(const QSOList *log_list, const char *search_filter,
                                  int *out_indices, int max_out) {
    int count = 0;
    for (int i = (int)log_list->count - 1; i >= 0 && count < max_out; --i) {
        const QSO *item = &log_list->items[i];
        if (search_filter != NULL && search_filter[0] != '\0') {
            int match = utf8_contains_ci(item->call, search_filter) ||
                        utf8_contains_ci(item->name, search_filter) ||
                        utf8_contains_ci(item->qth, search_filter) ||
                        utf8_contains_ci(item->gridsquare, search_filter);
            if (!match) continue;
        }
        out_indices[count++] = i;
    }
    return count;
}

/* Rita hela huvudskärmen med fälten logiskt linjerade på ':' */
static void draw_main_screen(const StationSettings *st, const QSO *qso,
                             FocusMode focus_mode, int active_field, int band_selecting,
                             int field_cursor_pos, int time_frozen, int editing_index,
                             const QSOList *log_list, int selected_log_pos, int *log_scroll_offset,
                             const char *search_filter, const char *status_msg,
                             const char *qrb_str,
                             int *cur_y, int *cur_x) {
    erase();

    if (COLS < MIN_COLS || LINES < MIN_LINES) {
        attron(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);
        mvprintw(LINES / 2 - 1, 2, "Fönstret är för litet (%dx%d).", COLS, LINES);
        mvprintw(LINES / 2,     2, "Minsta storlek är %dx%d. Förstora terminalen.", MIN_COLS, MIN_LINES);
        attroff(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);
        refresh();
        return;
    }

    int in_input = (focus_mode == FOCUS_INPUT);

    /* 1. Längst upp: Statusrad */
    draw_top_bar(st);

    /* 2. Under detta: Inmatningsfälten (Rader 1..7) */
    draw_box_with_title(1, 0, 7, COLS,
                        editing_index >= 0 ? "Redigera QSO" : "Ny förbindelse",
                        in_input);

    if (editing_index >= 0) {
        attron(COLOR_PAIR(CP_EDIT_BADGE) | A_BOLD);
        mvprintw(1, 18, " REDIGERAR #%d ", editing_index + 1);
        attroff(COLOR_PAIR(CP_EDIT_BADGE) | A_BOLD);
    } else if (time_frozen) {
        attron(COLOR_PAIR(CP_FROZEN_BADGE) | A_BOLD);
        mvprintw(1, 20, " TID FRYST ");
        attroff(COLOR_PAIR(CP_FROZEN_BADGE) | A_BOLD);
    } else {
        attron(COLOR_PAIR(CP_LIVE_BADGE) | A_BOLD);
        mvprintw(1, 20, " UTC LIVE ");
        attroff(COLOR_PAIR(CP_LIVE_BADGE) | A_BOLD);
    }

    if (status_msg != NULL && status_msg[0] != '\0' && COLS >= 60) {
        int max_msg = COLS - 36;
        if (max_msg > 0) {
            attron(COLOR_PAIR(CP_STATUS_OK) | A_BOLD);
            mvprintw(1, 34, "%.*s", max_msg, status_msg);
            attroff(COLOR_PAIR(CP_STATUS_OK) | A_BOLD);
        }
    }

    /* Kolumnlinjering på ':' i inmatningsrutan:
     * - Kolumn 1 (colon1): Signal:, QTH:, Datum:, Frekv.:, Not:
     * - Kolumn 2 (colon2): Namn:, Lokator:, UTC:, Band:
     * - Kolumn 3 (colon3): RST T:, DXCC:, Mode:
     * - Kolumn 4 (colon4 på rad 2): RST R: */
    int colon1 = 11;
    int colon2 = (COLS >= 76) ? 36 : 34;
    int colon3 = (COLS >= 76) ? 57 : 50;
    int colon4 = (COLS >= 76) ? 73 : (COLS - 6);

    const char *lbl_mode = "Mode";

    int w_call = (colon2 - 10) - (colon1 + 2);
    if (w_call > 12) w_call = 12;
    if (w_call < 7)  w_call = 7;

    int w_name = (colon3 - 9) - (colon2 + 2);
    if (w_name < 6) w_name = 6;

    int w_col3_end = (COLS - 2) - (colon3 + 2);
    if (w_col3_end < 6) w_col3_end = 6;

    /* Rad 2: Signal: [...] | Namn: [...] | RST T: [...] | RST R: [...] */
    draw_field_aligned(2, colon1, "Signal",   qso->call, w_call,
                       in_input && active_field == FLD_CALL, field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(2, colon2, "Namn",     qso->name, w_name,
                       in_input && active_field == FLD_NAME, field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(2, colon3, "RST T",    qso->rst_sent, 4,
                       in_input && active_field == FLD_RSTS, field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(2, colon4, "RST R",    qso->rst_rcvd, 4,
                       in_input && active_field == FLD_RSTR, field_cursor_pos, cur_y, cur_x);

    /* Rad 3: QTH: [...] | Lokator: [...] | DXCC: [...] | QRB: [...] */
    int w_qth = (colon2 - 10) - (colon1 + 2);
    if (w_qth < 6) w_qth = 6;
    int w_grid = (colon3 - 9) - (colon2 + 2);
    if (w_grid < 6) w_grid = 6;
    int w_dxcc = (colon4 - 6) - (colon3 + 2);
    if (w_dxcc < 6) w_dxcc = 6;

    draw_field_aligned(3, colon1, "QTH",      qso->qth,        w_qth,
                       in_input && active_field == FLD_QTH, field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(3, colon2, "Lokator",  qso->gridsquare, w_grid,
                       in_input && active_field == FLD_GRID, field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(3, colon3, "DXCC",     qso->dxcc,       w_dxcc,
                       in_input && active_field == FLD_DXCC, field_cursor_pos, cur_y, cur_x);

    /* QRB under RST R på rad 3 */
    int dummy_y, dummy_x;
    draw_field_aligned(3, colon4, "QRB",      (qrb_str && qrb_str[0]) ? qrb_str : "-", 8,
                       0, 0, &dummy_y, &dummy_x);

    /* Rad 4: Datum: [...] | UTC: [...] */
    draw_field_aligned(4, colon1, "Datum",     qso->date,       11,
                       in_input && active_field == FLD_DATE, field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(4, colon2, "UTC",       qso->time_utc,   6,
                       in_input && active_field == FLD_TIME, field_cursor_pos, cur_y, cur_x);

    /* Rad 5: Frekv.: [...] | Band: <20m  > | Mode: [...] */
    draw_field_aligned(5,         colon1, "Frekv.",   qso->freq, 9,
                       in_input && active_field == FLD_FREQ, field_cursor_pos, cur_y, cur_x);
    draw_band_selector_aligned(5, colon2,             qso->band,
                       in_input && active_field == FLD_BAND, band_selecting, cur_y, cur_x);
    draw_field_aligned(5,         colon3, lbl_mode,   qso->mode, w_col3_end > 8 ? 8 : w_col3_end,
                       in_input && active_field == FLD_MODE, field_cursor_pos, cur_y, cur_x);

    /* Rad 6: Not: [...] */
    int w_comment = (COLS - 2) - (colon1 + 2);
    draw_field_aligned(6, colon1, "Not",      qso->comment, w_comment,
                       in_input && active_field == FLD_COMM, field_cursor_pos, cur_y, cur_x);

    if (in_input && active_field == FLD_BAND) {
        draw_band_popup_list(7, qso->band, band_selecting);
    }

    /* 3. Under inmatningsrutan: Redan loggade QSO ("Loggrader") */
    int log_top = 8;
    int log_height = (LINES - 1) - log_top;
    int marked_count = 0;
    for (size_t i = 0; i < log_list->count; ++i) {
        if (log_list->items[i].marked) marked_count++;
    }

    char log_title[112];
    if (focus_mode == FOCUS_LOG) {
        if (marked_count > 0) {
            snprintf(log_title, sizeof(log_title),
                     "Loggrader – Ins=Märk, F8=Radera (%d markerade, %zu QSO)",
                     marked_count, log_list->count);
        } else {
            snprintf(log_title, sizeof(log_title),
                     "Loggrader – Välj med Pilar, Ins=Märk, F8=Radera, Enter=Redigera (%zu QSO)",
                     log_list->count);
        }
    } else if (search_filter != NULL && search_filter[0] != '\0') {
        if (marked_count > 0) {
            snprintf(log_title, sizeof(log_title), "Loggrader (Filter: %s | %d markerade | %zu QSO)",
                     search_filter, marked_count, log_list->count);
        } else {
            snprintf(log_title, sizeof(log_title), "Loggrader (Filter: %s | %zu QSO)",
                     search_filter, log_list->count);
        }
    } else {
        if (marked_count > 0) {
            snprintf(log_title, sizeof(log_title), "Loggrader (%d markerade, %zu QSO)",
                     marked_count, log_list->count);
        } else {
            snprintf(log_title, sizeof(log_title), "Loggrader (%zu QSO)", log_list->count);
        }
    }
    draw_box_with_title(log_top, 0, log_height, COLS, log_title, focus_mode == FOCUS_LOG);

    int hdr_row = log_top + 1;
    int first_data_row = log_top + 2;
    int avail_log_rows = (log_top + log_height - 1) - first_data_row;
    if (avail_log_rows < 0) avail_log_rows = 0;

    /* Band och Frekv i loggraderna: Band är högerställt (%5s) och Frekv (%8s) med 3 decimaler */
    attron(COLOR_PAIR(CP_TABLE_HEADER) | A_BOLD);
    if (COLS >= 92) {
        mvprintw(hdr_row, 2, "%-10s %-5s %-11s %5s %8s %-5s %-7s %-8s %-12s %-s",
                 "Datum", "UTC", "Signal", "Band", "Frekv", "Mode", "RST S/M", "Lokator", "Namn", "QTH");
    } else if (COLS >= 74) {
        mvprintw(hdr_row, 2, "%-10s %-5s %-10s %5s %8s %-5s %-7s %-6s %-s",
                 "Datum", "UTC", "Signal", "Band", "Frekv", "Mode", "RST", "Lok.", "Namn/QTH");
    } else {
        mvprintw(hdr_row, 2, "%-10s %-5s %-9s %5s %8s %-4s %-7s %-s",
                 "Datum", "UTC", "Signal", "Band", "Frekv", "Mode", "RST", "Namn");
    }
    attroff(COLOR_PAIR(CP_TABLE_HEADER) | A_BOLD);

    int max_filtered = (int)log_list->count > 0 ? (int)log_list->count : 1;
    int *filtered_idx = malloc((size_t)max_filtered * sizeof(int));
    int filtered_count = 0;
    if (filtered_idx != NULL) {
        filtered_count = build_filtered_indices(log_list, search_filter, filtered_idx, max_filtered);
    }

    if (avail_log_rows > 0 && filtered_count > 0) {
        if (selected_log_pos < *log_scroll_offset) {
            *log_scroll_offset = selected_log_pos;
        } else if (selected_log_pos >= *log_scroll_offset + avail_log_rows) {
            *log_scroll_offset = selected_log_pos - avail_log_rows + 1;
        }
        if (*log_scroll_offset < 0) *log_scroll_offset = 0;
    } else {
        *log_scroll_offset = 0;
    }

    int shown = 0;
    for (int pos = *log_scroll_offset; pos < filtered_count && shown < avail_log_rows; ++pos) {
        int qso_idx = filtered_idx[pos];
        const QSO *item = &log_list->items[qso_idx];
        int r = first_data_row + shown;
        int is_selected_row = (focus_mode == FOCUS_LOG && pos == selected_log_pos);

        char d_disp[16], t_disp[16], f_disp[16], b_disp[16], rst_disp[36];
        format_date_yyyy_mm_dd(item->date, d_disp, sizeof(d_disp));
        format_time_hh_mm(item->time_utc, t_disp, sizeof(t_disp));
        format_band_display(item->band, b_disp, sizeof(b_disp));
        parse_and_normalize_freq(item->freq, f_disp, sizeof(f_disp));
        snprintf(rst_disp, sizeof(rst_disp), "%s/%s",
                 item->rst_sent[0] ? item->rst_sent : "-",
                 item->rst_rcvd[0] ? item->rst_rcvd : "-");

        if (is_selected_row) {
            attron(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
            mvhline(r, 1, ' ', COLS - 2);
            if (item->marked) {
                mvaddch(r, 1, '*');
            } else {
                mvaddch(r, 1, '>');
            }
            *cur_y = r;
            *cur_x = 2;
        } else if (item->marked) {
            attron(COLOR_PAIR(CP_LOG_MARKED) | A_BOLD);
            mvhline(r, 1, ' ', COLS - 2);
            mvaddch(r, 1, '*');
        } else {
            attron(COLOR_PAIR(CP_LOG_ROW) | A_BOLD);
            mvaddch(r, 1, ' ');
        }

        if (COLS >= 92) {
            int rem_qth = (COLS - 2) - 82;
            if (rem_qth < 4) rem_qth = 4;
            mvprintw(r, 2, "%-10.10s %-5.5s %-11.11s %5.5s %8.8s %-5.5s %-7.7s %-8.8s %-12.12s %-.*s",
                     d_disp[0] ? d_disp : "-",
                     t_disp[0] ? t_disp : "-",
                     item->call[0] ? item->call : "-",
                     b_disp[0] ? b_disp : "-",
                     f_disp[0] ? f_disp : "-",
                     item->mode[0] ? item->mode : "-",
                     rst_disp,
                     item->gridsquare[0] ? item->gridsquare : "-",
                     item->name[0] ? item->name : "-",
                     rem_qth,
                     item->qth[0] ? item->qth : "-");
        } else if (COLS >= 74) {
            int rem = (COLS - 2) - 64;
            if (rem < 4) rem = 4;
            const char *nq = item->name[0] ? item->name : (item->qth[0] ? item->qth : "-");
            mvprintw(r, 2, "%-10.10s %-5.5s %-10.10s %5.5s %8.8s %-5.5s %-7.7s %-6.6s %-.*s",
                     d_disp[0] ? d_disp : "-",
                     t_disp[0] ? t_disp : "-",
                     item->call[0] ? item->call : "-",
                     b_disp[0] ? b_disp : "-",
                     f_disp[0] ? f_disp : "-",
                     item->mode[0] ? item->mode : "-",
                     rst_disp,
                     item->gridsquare[0] ? item->gridsquare : "-",
                     rem, nq);
        } else {
            int rem = (COLS - 2) - 54;
            if (rem < 3) rem = 3;
            mvprintw(r, 2, "%-10.10s %-5.5s %-9.9s %5.5s %8.8s %-4.4s %-7.7s %-.*s",
                     d_disp[0] ? d_disp : "-",
                     t_disp[0] ? t_disp : "-",
                     item->call[0] ? item->call : "-",
                     b_disp[0] ? b_disp : "-",
                     f_disp[0] ? f_disp : "-",
                     item->mode[0] ? item->mode : "-",
                     rst_disp,
                     rem,
                     item->name[0] ? item->name : "-");
        }

        if (is_selected_row) {
            attroff(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
        } else {
            attroff(COLOR_PAIR(CP_LOG_ROW) | A_BOLD);
        }
        shown++;
    }

    free(filtered_idx);

    if (shown == 0 && avail_log_rows > 0) {
        mvprintw(first_data_row, 2, "Inga förbindelser att visa ännu. Skriv in anropssignal ovan och tryck Enter.");
    }

    /* 4. Allra längst ned: Funktionstangentrad */
    draw_bottom_fkey_bar(0, focus_mode, search_filter);

    move(*cur_y, *cur_x);
    refresh();
}

/* F9: Hjälpskärm */
static void run_help_modal(const StationSettings *st) {
    while (1) {
        erase();
        draw_top_bar(st);
        draw_box_with_title(1, 0, LINES - 2, COLS, "Hjälp – QSO-logg (F9)", 1);

        attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvprintw(3, 3, "Tangentbordskommandon:");
        attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);

        attron(COLOR_PAIR(CP_LABEL));
        mvprintw(4,  4, "Tab / Shift-Tab   : Hoppa till nästa / föregående inmatningsfält");
        mvprintw(5,  4, "Vänster / Höger   : Hoppa till föregående / nästa fält");
        mvprintw(6,  4, "Pil upp / Pil ned : Gå till närmaste fält över eller under aktivt fält");
        mvprintw(7,  4, "Enter på Band     : Aktivera bandval (byt med pilar, Enter bekräftar)");
        mvprintw(8,  4, "F1 / Esc          : Rensa inmatningsrutan (UTC-klockan tickar igen)");
        mvprintw(9,  4, "F2 / Enter        : Logga QSO direkt med ifyllda uppgifter");
        mvprintw(10, 4, "F3 / Ctrl-F       : Sök/filtrera (Signal, Namn, QTH, Lokator)");
        mvprintw(11, 4, "F4 / Ctrl-O       : Växla mellan Inmatningsrutan och Loggraderna");
        mvprintw(12, 4, "Loggrader         : Välj med Pilar, Enter = Redigera");
        mvprintw(13, 4, "                    Ins = Markera rad (och gå ned), F8 = Radera markerade/aktuell");
        mvprintw(14, 4, "F9                : Visa denna hjälpsida");
        mvprintw(15, 4, "F10 / Ctrl-Q      : Avsluta programmet");
        mvprintw(16, 4, "F12               : Inställningar (Station, Färgschema, SM Callbook, Auto RST)");

        if (LINES >= 22) {
            attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
            mvprintw(17, 3, "Smarta funktioner:");
            attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
            attron(COLOR_PAIR(CP_LABEL));
            mvprintw(18, 4, "- Frekvens utanför amatörband ger beräknat metervärde (300/f avrundat).");
            mvprintw(19, 4, "- DXCC identifieras automatiskt från dxcc.txt när du skriver anropssignal.");
            mvprintw(20, 4, "- Svenska signaler (SA-SM, 7S, 8S) kan slås upp i SM Callbook (slås på i F12).");
        }
        attroff(COLOR_PAIR(CP_LABEL));

        int row = LINES - 1;
        attron(COLOR_PAIR(CP_FKEY_LABEL));
        mvhline(row, 0, ' ', COLS);
        mvprintw(row, 2, "Tryck Esc, Enter eller F9 för att stänga hjälpen.");
        attroff(COLOR_PAIR(CP_FKEY_LABEL));

        refresh();

        int ch = getch();
        if (ch == 27 || ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == KEY_F(9)) {
            break;
        }
    }
}

/* Rensa hela loggboken i minnet och på disk (qsolog.adi skrivs över med tom ADIF 3.1.8-header) */
static int clear_log_book(QSOList *log_list, const StationSettings *st) {
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

/* F12: Inställningssida med ':'-linjerade fält, färgscheman, SM Callbook, Auto RST och Töm loggbok */
static void run_settings_modal(StationSettings *st, QSO *qso, int editing_index,
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

            int r_theme = 8;
            int r_smcb  = 9;
            int r_rst   = 10;
            int r_flt   = 11;
            int r_clear = 12;

            /* Fält 5: Färgschema */
            const char *lbl_theme = "Färgschema";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_theme, colon_col - utf8_display_width(lbl_theme), "%s:", lbl_theme);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            int fcol = colon_col + 2;
            if (active == 5) {
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

            /* Fält 6: SM Callbook toggle (Default Avstängd) */
            const char *lbl_smcb = "SM Callbook (ssa.se)";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_smcb, colon_col - utf8_display_width(lbl_smcb), "%s:", lbl_smcb);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            const char *smcb_txt = edit.smcb_enabled ? "På (Slå upp SA-SM, 7S, 8S)" : "Av";
            if (active == 6) {
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

            /* Fält 7: Automatisk RST 59/59 vid nytt QSO */
            const char *lbl_rst = "Auto RST 59/59";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_rst, colon_col - utf8_display_width(lbl_rst), "%s:", lbl_rst);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            const char *rst_txt = edit.auto_rst_59 ? "På (Fyll i 59 / 59 vid nytt QSO)" : "Av";
            if (active == 7) {
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

            /* Fält 8: Filtrera loggrader efter anropssignal */
            const char *lbl_flt = "Filtrera loggrader";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_flt, colon_col - utf8_display_width(lbl_flt), "%s:", lbl_flt);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            const char *flt_txt = edit.filter_log ? "På" : "Av";
            if (active == 8) {
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

            /* Fält 9: Töm loggbok och börja på nytt */
            const char *lbl_clear = "Töm loggbok";
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(r_clear, colon_col - utf8_display_width(lbl_clear), "%s:", lbl_clear);
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            if (active == 9) {
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

            if (LINES >= 17) {
                mvprintw(LINES - 2, 3, "Tab/Pilar byter fält. Vänster/Höger ändrar val. Enter = Spara/Utför. Esc = Avbryt.");
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
        } else if (active == 9 && (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == ' ')) {
            /* Bekräftelseruta för att tömma loggboken */
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
        } else if (ch == KEY_F(2) || ch == 19 || ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            to_upper_str(edit.call);
            to_upper_str(edit.gridsquare);
            *st = edit;
            apply_color_theme(st->theme);
            save_station_settings(CONFIG_FILE, st);
            if (qso != NULL && editing_index < 0 && st->auto_rst_59) {
                if (qso->rst_sent[0] == '\0') copy_str(qso->rst_sent, sizeof(qso->rst_sent), "59");
                if (qso->rst_rcvd[0] == '\0') copy_str(qso->rst_rcvd, sizeof(qso->rst_rcvd), "59");
            }
            copy_str(status_msg, status_sz, "Stationsinställningar sparade!");
            return;
        } else if (ch == '\t' || ch == KEY_DOWN) {
            active = (active + 1) % STATION_FIELD_COUNT;
            char *p = (active == 0) ? edit.call : (active == 1) ? edit.name :
                      (active == 2) ? edit.qth  : (active == 3) ? edit.gridsquare :
                      (active == 4) ? edit.timezone : NULL;
            cursor_pos = p ? utf8_display_width(p) : 0;
        } else if (ch == KEY_BTAB || ch == KEY_UP) {
            active = (active + STATION_FIELD_COUNT - 1) % STATION_FIELD_COUNT;
            char *p = (active == 0) ? edit.call : (active == 1) ? edit.name :
                      (active == 2) ? edit.qth  : (active == 3) ? edit.gridsquare :
                      (active == 4) ? edit.timezone : NULL;
            cursor_pos = p ? utf8_display_width(p) : 0;
        } else if (active == 5 && ch == KEY_LEFT) {
            edit.theme = (edit.theme + THEME_COUNT - 1) % THEME_COUNT;
            apply_color_theme(edit.theme);
        } else if (active == 5 && (ch == KEY_RIGHT || ch == ' ')) {
            edit.theme = (edit.theme + 1) % THEME_COUNT;
            apply_color_theme(edit.theme);
        } else if (active == 6 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')) {
            edit.smcb_enabled = !edit.smcb_enabled;
        } else if (active == 7 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')) {
            edit.auto_rst_59 = !edit.auto_rst_59;
        } else if (active == 8 && (ch == KEY_LEFT || ch == KEY_RIGHT || ch == ' ')) {
            edit.filter_log = !edit.filter_log;
        } else if (active < 5 && ch == KEY_LEFT) {
            if (cursor_pos > 0) cursor_pos--;
        } else if (active < 5 && ch == KEY_RIGHT) {
            char *p = (active == 0) ? edit.call : (active == 1) ? edit.name :
                      (active == 2) ? edit.qth  : (active == 3) ? edit.gridsquare : edit.timezone;
            int max_c = utf8_display_width(p);
            if (cursor_pos < max_c) cursor_pos++;
        } else if (active < 5 && (ch == KEY_BACKSPACE || ch == 127 || ch == '\b')) {
            char *p = (active == 0) ? edit.call : (active == 1) ? edit.name :
                      (active == 2) ? edit.qth  : (active == 3) ? edit.gridsquare : edit.timezone;
            utf8_backspace_at(p, &cursor_pos);
        } else if (active < 5 && ch == KEY_DC) {
            char *p = (active == 0) ? edit.call : (active == 1) ? edit.name :
                      (active == 2) ? edit.qth  : (active == 3) ? edit.gridsquare : edit.timezone;
            utf8_delete_at(p, cursor_pos);
        } else if (active < 5 && ch >= 32 && ch != 127 && ch < KEY_MIN) {
            char *ptr = NULL;
            size_t cap = 0;
            switch (active) {
                case 0: ptr = edit.call;       cap = sizeof(edit.call); break;
                case 1: ptr = edit.name;       cap = sizeof(edit.name); break;
                case 2: ptr = edit.qth;        cap = sizeof(edit.qth); break;
                case 3: ptr = edit.gridsquare; cap = sizeof(edit.gridsquare); break;
                case 4: ptr = edit.timezone;   cap = sizeof(edit.timezone); break;
            }
            if (ptr) {
                char c = (char)ch;
                if ((active == 0 || active == 3) && (unsigned char)c < 128) {
                    c = (char)toupper((unsigned char)c);
                }
                utf8_insert_byte_at(ptr, cap, &cursor_pos, c);
            }
        }
    }
}

/* F3: Sök/filtrera i Loggrader (Signal, Namn, QTH, Lokator) */
static void run_search_bar(const StationSettings *st, char *search_filter, size_t filter_sz) {
    int cpos = utf8_display_width(search_filter);
    while (1) {
        int cur_y = LINES - 1, cur_x = 35;
        draw_top_bar(st);
        mvhline(LINES - 1, 0, ' ', COLS);
        const char *prompt_lbl = (COLS >= 80) ? "Sök (Signal/Namn/QTH/Lokator)" : "Sök";
        int colon_x = (COLS >= 80) ? 33 : 6;
        draw_field_aligned(LINES - 1, colon_x, prompt_lbl,
                           search_filter, 18, 1, cpos, &cur_y, &cur_x);
        if (COLS >= 75) {
            attron(COLOR_PAIR(CP_FKEY_LABEL));
            mvprintw(LINES - 1, colon_x + 22, "[Enter=Klar, Tom=Alla]");
            attroff(COLOR_PAIR(CP_FKEY_LABEL));
        }
        move(cur_y, cur_x);
        refresh();

        int ch = getch();
        if (ch == ERR) continue;
        if (ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == 27 || ch == KEY_F(3)) {
            break;
        } else if (ch == KEY_LEFT) {
            if (cpos > 0) cpos--;
        } else if (ch == KEY_RIGHT) {
            int max_c = utf8_display_width(search_filter);
            if (cpos < max_c) cpos++;
        } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
            utf8_backspace_at(search_filter, &cpos);
        } else if (ch == KEY_DC) {
            utf8_delete_at(search_filter, cpos);
        } else if (ch >= 32 && ch != 127 && ch < KEY_MIN) {
            utf8_insert_byte_at(search_filter, filter_sz, &cpos, (char)ch);
        }
    }
}

/* Sök efter en signal i loggraderna (nyaste först).
 * Om signalen finns och har namn, QTH eller lokator, kopieras dessa och 1 returneras.
 * Annars returneras 0. */
static int find_call_in_log(const QSOList *log_list, const char *call_in,
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

/* Utför uppslagning av Namn och QTH:
 * 1. Kontrollerar först om signalen redan finns i befintliga loggrader med namn/QTH/lokator.
 * 2. Om inte, kontrolleras SMCB-cachen i minnet.
 * 3. Om signalen saknas och SMCB är påslaget, startas en asynkron bakgrundssökning
 *    så att redigeringen inte fryser/stoppar. */
static void maybe_lookup_call_details(QSO *qso, const QSOList *log_list,
                                      const StationSettings *st,
                                      char *last_smcb_call, size_t last_smcb_sz,
                                      char *status_msg, size_t status_sz) {
    if (qso->call[0] == '\0') return;

    /* 1. Sök i tidigare loggade rader (direkt i minnet) */
    char log_name[64] = "", log_qth[64] = "", log_grid[16] = "";
    if (find_call_in_log(log_list, qso->call, log_name, sizeof(log_name),
                         log_qth, sizeof(log_qth), log_grid, sizeof(log_grid))) {
        if (log_name[0] != '\0' && qso->name[0] == '\0') copy_str(qso->name, sizeof(qso->name), log_name);
        if (log_qth[0] != '\0' && qso->qth[0] == '\0')  copy_str(qso->qth, sizeof(qso->qth), log_qth);
        if (log_grid[0] != '\0' && qso->gridsquare[0] == '\0') copy_str(qso->gridsquare, sizeof(qso->gridsquare), log_grid);
        snprintf(status_msg, status_sz, "Från logg: %s%s%s",
                 log_name, (log_name[0] && log_qth[0]) ? ", " : "", log_qth);
        return;
    }

    /* 2. Slå upp i SM Callbook om aktiverat och svensk signal */
    if (!st->smcb_enabled) return;

    char base_call[32];
    if (!extract_swedish_base_call(qso->call, base_call, sizeof(base_call))) return;
    if (strcasecmp(base_call, last_smcb_call) == 0) return;

    copy_str(last_smcb_call, last_smcb_sz, base_call);

    /* Kontrollera om signalen redan finns i SMCB-cache */
    int was_cached = 0;
    char cached_name[64] = "", cached_qth[64] = "";
    if (lookup_smcb_cache(base_call, cached_name, sizeof(cached_name), cached_qth, sizeof(cached_qth), &was_cached)) {
        if (cached_name[0] != '\0' && qso->name[0] == '\0') copy_str(qso->name, sizeof(qso->name), cached_name);
        if (cached_qth[0] != '\0' && qso->qth[0] == '\0')  copy_str(qso->qth, sizeof(qso->qth), cached_qth);
        snprintf(status_msg, status_sz, "SMCB: %s, %s", cached_name, cached_qth);
        return;
    }
    if (was_cached) {
        snprintf(status_msg, status_sz, "SMCB: Ingen träff för %s", base_call);
        return;
    }

    /* Starta asynkron sökning i bakgrundstråd */
    start_smcb_background_lookup(base_call);
    snprintf(status_msg, status_sz, "SMCB: Slår upp %s...", base_call);
}

/* Kontrollera om bakgrundssökningen i SM Callbook har blivit klar och applicera resultatet */
static void poll_smcb_background_result(QSO *qso, const StationSettings *st,
                                        int active_field, int *cursor_pos,
                                        char *status_msg, size_t status_sz,
                                        char *current_qrb, size_t qrb_sz) {
    int has_result = 0;
    int res_found = 0;
    char res_call[32] = "", res_name[64] = "", res_qth[64] = "";

    pthread_mutex_lock(&g_smcb_mutex);
    if (g_smcb_async.has_result) {
        has_result = 1;
        res_found = g_smcb_async.res_found;
        copy_str(res_call, sizeof(res_call), g_smcb_async.res_call);
        copy_str(res_name, sizeof(res_name), g_smcb_async.res_name);
        copy_str(res_qth, sizeof(res_qth), g_smcb_async.res_qth);
        g_smcb_async.has_result = 0;
    }
    pthread_mutex_unlock(&g_smcb_mutex);

    if (!has_result) return;

    /* Kontrollera att det aktuella QSO:t fortfarande har samma signal */
    char base_call[32];
    if (qso->call[0] == '\0' || !extract_swedish_base_call(qso->call, base_call, sizeof(base_call))) {
        return;
    }
    if (strcasecmp(base_call, res_call) != 0) {
        return; /* Signalen har ändrats under tiden, ignorera resultatet */
    }

    if (res_found) {
        int updated = 0;
        if (qso->name[0] == '\0' && res_name[0] != '\0') {
            copy_str(qso->name, sizeof(qso->name), res_name);
            if (active_field == FLD_NAME && cursor_pos) {
                *cursor_pos = utf8_display_width(qso->name);
            }
            updated = 1;
        }
        if (qso->qth[0] == '\0' && res_qth[0] != '\0') {
            copy_str(qso->qth, sizeof(qso->qth), res_qth);
            if (active_field == FLD_QTH && cursor_pos) {
                *cursor_pos = utf8_display_width(qso->qth);
            }
            updated = 1;
            /* Uppdatera QRB om motstationens lokator är tom */
            if (current_qrb && qrb_sz > 0) {
                compute_qrb_str(st, qso, current_qrb, qrb_sz);
            }
        }
        if (updated || status_msg[0] == '\0' || strstr(status_msg, "SMCB: Slår upp")) {
            snprintf(status_msg, status_sz, "SMCB: %s, %s", res_name, res_qth);
        }
    } else {
        if (status_msg[0] == '\0' || strstr(status_msg, "SMCB: Slår upp")) {
            snprintf(status_msg, status_sz, "SMCB: Ingen träff för %s", res_call);
        }
    }
}

/* Synkronisera och spara Frekvens, Band och Trafikslag till station.conf */
static void persist_qso_radio_settings(const QSO *qso, StationSettings *st) {
    if (qso == NULL || st == NULL) return;
    if (qso->freq[0] != '\0') copy_str(st->last_freq, sizeof(st->last_freq), qso->freq);
    if (qso->band[0] != '\0') copy_str(st->last_band, sizeof(st->last_band), qso->band);
    if (qso->mode[0] != '\0') copy_str(st->last_mode, sizeof(st->last_mode), qso->mode);
    save_station_settings(CONFIG_FILE, st);
}

/* Uppdatera automatiska förslag när man lämnar ett fält */
static void apply_field_defaults_on_leave(QSO *qso, int leaving_field,
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

/* Byt band i listan (160m..2m) med steg (+1 eller -1) och sätt standardfrekvens */
static void step_band_choice(QSO *qso, StationSettings *st, int delta) {
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
    copy_str(qso->band, sizeof(qso->band), BAND_OPTIONS[idx]);
    copy_str(qso->freq, sizeof(qso->freq), BAND_DEFAULT_FREQ[idx]);
    persist_qso_radio_settings(qso, st);
}

/* Logga/spara aktuellt QSO (eller uppdatera redigerat QSO) och återgå till Anropssignal */
static void try_save_current_qso(QSO *qso, int *time_frozen, int *active_field,
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
    long d_val = 0;
    if (current_qrb != NULL && current_qrb[0] != '\0' && strcmp(current_qrb, "-") != 0) {
        d_val = atol(current_qrb);
    }
    if (d_val <= 0) {
        char temp_qrb[32];
        compute_qrb_str(st, qso, temp_qrb, sizeof(temp_qrb));
        if (temp_qrb[0] != '\0' && strcmp(temp_qrb, "-") != 0) {
            d_val = atol(temp_qrb);
        }
    }
    if (d_val > 0) {
        snprintf(qso->distance, sizeof(qso->distance), "%ld", d_val);
    }

    persist_qso_radio_settings(qso, st);

    if (*editing_index >= 0 && (size_t)(*editing_index) < log_list->count) {
        int idx = *editing_index;
        log_list->items[idx] = *qso;
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

int main(void) {
    setlocale(LC_ALL, "");
    curl_global_init(CURL_GLOBAL_DEFAULT);

    /* Läs in DXCC-tabellen från dxcc.txt vid programstart */
    load_dxcc_file(DXCC_FILE);

    StationSettings station;
    load_station_settings(CONFIG_FILE, &station);

    QSOList log_list = {NULL, 0, 0};
    load_qsos_from_adif(LOG_FILE, &log_list);

    initscr();
    set_escdelay(25);
    cbreak();
    noecho();
    keypad(stdscr, TRUE);
    timeout(250);
    apply_color_theme(station.theme);

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

    while (1) {
        if (!time_frozen && editing_index < 0) {
            get_utc_strings(current_qso.date, sizeof(current_qso.date),
                            current_qso.time_utc, sizeof(current_qso.time_utc),
                            NULL, 0);
        }

        poll_smcb_background_result(&current_qso, &station,
                                    active_field, &field_cursor_pos,
                                    status_msg, sizeof(status_msg),
                                    current_qrb, sizeof(current_qrb));

        int cur_y = 2, cur_x = 16;
        const char *active_filter = (search_filter[0] != '\0') ? search_filter :
                                    (station.filter_log && current_qso.call[0] != '\0') ? current_qso.call : NULL;
        draw_main_screen(&station, &current_qso, focus_mode, active_field, band_selecting,
                         field_cursor_pos, time_frozen, editing_index,
                         &log_list, selected_log_pos, &log_scroll_offset,
                         active_filter, status_msg, current_qrb, &cur_y, &cur_x);

        int ch = getch();
        if (ch == ERR) {
            /* Om användaren står i Signal och har skrivit in en anropssignal:
               1. Om signalen redan finns i loggraderna med namn/QTH, hämta därifrån direkt.
               2. Annars, om svensk signal och SM Callbook är påslaget, slå upp online efter en kort paus (~0.5 s). */
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

        /* Globala funktionstangenter och ESC */
        if (ch == KEY_F(1) || ch == 27) {
            /* F1 eller ESC: Rensa inmatningsfältet och låt UTC-klockan ticka igen */
            focus_mode = FOCUS_INPUT;
            band_selecting = 0;
            reset_qso_form(&current_qso, &station, &time_frozen, &active_field, &field_cursor_pos, &editing_index, last_smcb_call, current_qrb);
            preferred_col = 0;
            copy_str(status_msg, sizeof(status_msg), "Formulär rensat – UTC-klockan tickar igen.");
            continue;
        } else if (ch == KEY_F(3) || ch == 6) { /* F3 eller Ctrl+F: Sök */
            band_selecting = 0;
            run_search_bar(&station, search_filter, sizeof(search_filter));
            selected_log_pos = 0;
            log_scroll_offset = 0;
            continue;
        } else if (ch == KEY_F(4) || ch == 15) { /* F4 (eller Ctrl-O): Växla mellan inmatning och loggrader */
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
                    copy_str(status_msg, sizeof(status_msg), "Loggrader: Välj med pilar, Ins=Märk, F8=Radera, Enter=Redigera.");
                } else {
                    copy_str(status_msg, sizeof(status_msg), "Inga loggrader att välja ännu.");
                }
            } else {
                focus_mode = FOCUS_INPUT;
                sync_cursor_to_field_end(&current_qso, active_field, &field_cursor_pos);
                copy_str(status_msg, sizeof(status_msg), "Åter i inmatningsfälten.");
            }
            continue;
        } else if (ch == KEY_F(9)) { /* F9: Hjälp */
            run_help_modal(&station);
            continue;
        } else if (ch == KEY_F(10) || ch == 17) { /* F10 eller Ctrl+Q: Avsluta */
            break;
        } else if (ch == KEY_F(12)) { /* F12: Ändra inställningar */
            band_selecting = 0;
            run_settings_modal(&station, &current_qso, editing_index, &log_list,
                               &selected_log_pos, &log_scroll_offset,
                               status_msg, sizeof(status_msg));
            compute_qrb_str(&station, &current_qso, current_qrb, sizeof(current_qrb));
            continue;
        }

        /* Hantera tangenttryckningar när fokus är i Loggraderna (FOCUS_LOG) */
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
            } else if (ch == KEY_F(8)) {
                /* F8: Radera alla markerade rader, eller aktuell rad om ingen är markerad */
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

        /* Hantera tangenttryckningar när fokus är i Inmatningsfälten (FOCUS_INPUT) */
        if (active_field == FLD_BAND && (ch == '\n' || ch == '\r' || ch == KEY_ENTER)) {
            /* På Band-fältet: Enter öppnar/bekräftar bandval */
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
            /* Enter (i övriga fält) eller F2 loggar QSO direkt med det som är ifyllt */
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
            /* Tab och Högerpil hoppar till nästa inmatningsfält */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg),
                                          current_qrb, sizeof(current_qrb));
            active_field = (active_field + 1) % QSO_FIELD_COUNT;
            preferred_col = field_default_col(active_field);
            sync_cursor_to_field_end(&current_qso, active_field, &field_cursor_pos);
        } else if (ch == KEY_BTAB || ch == KEY_LEFT) {
            /* Shift-Tab och Vänsterpil hoppar till föregående inmatningsfält */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg),
                                          current_qrb, sizeof(current_qrb));
            active_field = (active_field + QSO_FIELD_COUNT - 1) % QSO_FIELD_COUNT;
            preferred_col = field_default_col(active_field);
            sync_cursor_to_field_end(&current_qso, active_field, &field_cursor_pos);
        } else if (ch == KEY_DOWN) {
            /* Pil ned går till närmaste fält rakt under det man står i */
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
            /* Pil upp går till närmaste fält rakt över det man står i */
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
            /* När användaren börjar skriva: kopiera och frys UTC-datum & tid till QSO! */
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

    apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                  last_smcb_call, sizeof(last_smcb_call),
                                  status_msg, sizeof(status_msg),
                                  current_qrb, sizeof(current_qrb));
    persist_qso_radio_settings(&current_qso, &station);

    g_smcb_abort = 1;
    endwin();
    free(log_list.items);
    free_dxcc_db();
    curl_global_cleanup();
    return 0;
}
