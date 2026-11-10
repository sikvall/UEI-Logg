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

#define LOG_FILE "qsolog.adi"
#define CONFIG_FILE "station.conf"
#define DXCC_FILE "dxcc.txt"

#define MIN_COLS 60
#define MIN_LINES 20
#define QSO_FIELD_COUNT 13
#define STATION_FIELD_COUNT 10

/* Tillåtna band i bandlistan samt standardfrekvens (MHz) med 3 decimaler */
static const char * const BAND_OPTIONS[] = {
    "160m", "80m", "60m", "40m", "30m", "20m",
    "17m",  "15m", "12m", "10m", "6m",  "2m"
};
static const char * const BAND_DEFAULT_FREQ[] = {
    "1.900",  "3.600",  "5.400",  "7.100",  "10.100", "14.200",
    "18.100", "21.200", "24.900", "28.800", "51.000", "145.000"
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

/* Fokusläge (Ctrl-O växlar mellan inmatning och loggrader) */
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
 * 8: Tid (UTC)     (Rad 4, Kol 2)
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

/* Föreslå amatörradioband (ur listan 160m..2m) utifrån frekvens */
static const char *guess_band_from_freq(const char *freq_str) {
    if (freq_str == NULL || freq_str[0] == '\0') return "";
    double f = parse_and_normalize_freq(freq_str, NULL, 0);
    if (f >= 1.8 && f <= 2.0)       return "160m";
    if (f >= 3.5 && f <= 3.8)       return "80m";
    if (f >= 5.3 && f <= 5.4)       return "60m";
    if (f >= 7.0 && f <= 7.2)       return "40m";
    if (f >= 10.1 && f <= 10.15)    return "30m";
    if (f >= 14.0 && f <= 14.35)    return "20m";
    if (f >= 18.068 && f <= 18.168) return "17m";
    if (f >= 21.0 && f <= 21.45)    return "15m";
    if (f >= 24.89 && f <= 24.99)   return "12m";
    if (f >= 28.0 && f <= 29.7)     return "10m";
    if (f >= 50.0 && f <= 52.0)     return "6m";
    if (f >= 144.0 && f <= 146.0)   return "2m";
    return "";
}

/* Bestäm band utifrån frekvens:
 * Om frekvensen tillhör något av amatörradiobanden (160m..2m) används det bandet.
 * Annars beräknas metervärdet enligt 300 / f avrundat till närmaste heltal (t.ex. "75m", "11m"). */
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
        snprintf(band_out, out_sz, "%ldm", meters);
    }
}

static int find_band_index(const char *band_str) {
    if (band_str == NULL || band_str[0] == '\0') return -1;
    char norm[16];
    copy_str(norm, sizeof(norm), band_str);
    size_t len = strlen(norm);
    if (len > 0 && isdigit((unsigned char)norm[len - 1]) && len + 1 < sizeof(norm)) {
        norm[len] = 'm';
        norm[len + 1] = '\0';
    }
    for (int i = 0; i < BAND_OPTION_COUNT; ++i) {
        if (strcasecmp(norm, BAND_OPTIONS[i]) == 0) {
            return i;
        }
    }
    return -1;
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
    copy_str(st->last_band, sizeof(st->last_band), "20m");
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
            int bidx = find_band_index(val);
            if (bidx >= 0) {
                copy_str(st->last_band, sizeof(st->last_band), BAND_OPTIONS[bidx]);
            } else {
                copy_str(st->last_band, sizeof(st->last_band), val);
            }
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

static void write_qso_record_adif(FILE *fp, const QSO *qso, const StationSettings *st) {
    char clean_date[16], clean_time[16];
    normalize_date_adif(qso->date, clean_date, sizeof(clean_date));
    normalize_time_adif(qso->time_utc, clean_time, sizeof(clean_time));

    write_adif_field(fp, "CALL", qso->call);
    write_adif_field(fp, "NAME", qso->name);
    write_adif_field(fp, "QSO_DATE", clean_date);
    write_adif_field(fp, "TIME_ON", clean_time);
    write_adif_field(fp, "FREQ", qso->freq);
    write_adif_field(fp, "BAND", qso->band);
    write_adif_field(fp, "MODE", qso->mode);
    write_adif_field(fp, "RST_SENT", qso->rst_sent);
    write_adif_field(fp, "RST_RCVD", qso->rst_rcvd);
    write_adif_field(fp, "GRIDSQUARE", qso->gridsquare);
    write_adif_field(fp, "QTH", qso->qth);
    write_adif_field(fp, "COUNTRY", qso->dxcc);
    write_adif_field(fp, "COMMENT", qso->comment);
    if (st != NULL) {
        write_adif_field(fp, "STATION_CALLSIGN", st->call);
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
        fprintf(fp, "ADIF Log skapad av qsolog\n");
        fprintf(fp, "<ADIF_VER:5>3.1.4\n");
        fprintf(fp, "<PROGRAMID:6>qsolog\n");
        fprintf(fp, "<EOH>\n\n");
    }

    write_qso_record_adif(fp, qso, st);
    fclose(fp);
    return 0;
}

static int save_all_qsos_adif(const char *filename, const QSOList *list, const StationSettings *st) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) return -1;

    fprintf(fp, "ADIF Log skapad av qsolog\n");
    fprintf(fp, "<ADIF_VER:5>3.1.4\n");
    fprintf(fp, "<PROGRAMID:6>qsolog\n");
    fprintf(fp, "<EOH>\n\n");

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
    } else if (strcasecmp(tag, "FREQ") == 0) {
        parse_and_normalize_freq(val, qso->freq, sizeof(qso->freq));
    } else if (strcasecmp(tag, "BAND") == 0) {
        copy_str(qso->band, sizeof(qso->band), val);
    } else if (strcasecmp(tag, "MODE") == 0) {
        copy_str(qso->mode, sizeof(qso->mode), val);
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
                           int *field_cursor_pos, int *editing_index, char *last_smcb_call) {
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

    copy_str(qso->band, sizeof(qso->band), saved_band[0] ? saved_band : "20m");
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
    const char *shown = (val && val[0]) ? val : "20m";

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

/* Rita raden för bandval (160, 80, 60, 40, 30, 20, 17, 15, 12, 10, 6, 2) när Band är aktivt */
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
    mvprintw(row, x, "Band (Pilar väljer, Enter=Klar): ");
    attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);
    x += 33;

    static const char * const SHORT_NAMES[] = {
        "160", "80", "60", "40", "30", "20", "17", "15", "12", "10", "6", "2"
    };

    for (int i = 0; i < BAND_OPTION_COUNT && x + 5 < COLS - 1; ++i) {
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
        {"Ctrl-O", focus_mode == FOCUS_LOG ? "Inmatn." : "Loggrad"},
        {"F3",     (search_filter && search_filter[0]) ? "Sök*" : "Sök"},
        {"F11",    "Hjälp"},
        {"F12",    "Inställn."},
        {"F10",    "Avsluta"}
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
        for (int i = 0; i < count && col < COLS - 4; ++i) {
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

static int build_filtered_indices(const QSOList *log_list, const char *search_filter,
                                  int *out_indices, int max_out) {
    int count = 0;
    for (int i = (int)log_list->count - 1; i >= 0 && count < max_out; --i) {
        const QSO *item = &log_list->items[i];
        if (search_filter != NULL && search_filter[0] != '\0') {
            char call_u[32], filt_u[32];
            copy_str(call_u, sizeof(call_u), item->call);
            copy_str(filt_u, sizeof(filt_u), search_filter);
            to_upper_str(call_u);
            to_upper_str(filt_u);
            if (strstr(call_u, filt_u) == NULL) continue;
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
     * - Kolumn 2 (colon2): Namn:, DXCC:, Tid (UTC):, Band:
     * - Kolumn 3 (colon3): RST T:, Lokator:, Mode:
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

    /* Rad 3: QTH: [...] | DXCC: [...] | Lokator: [...] */
    int w_qth = (colon2 - 10) - (colon1 + 2);
    if (w_qth < 6) w_qth = 6;
    int w_dxcc = (colon3 - 9) - (colon2 + 2);
    if (w_dxcc < 6) w_dxcc = 6;

    draw_field_aligned(3, colon1, "QTH",      qso->qth,  w_qth,
                       in_input && active_field == FLD_QTH, field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(3, colon2, "DXCC",     qso->dxcc, w_dxcc,
                       in_input && active_field == FLD_DXCC, field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(3, colon3, "Lokator",  qso->gridsquare, w_col3_end > 8 ? 8 : w_col3_end,
                       in_input && active_field == FLD_GRID, field_cursor_pos, cur_y, cur_x);

    /* Rad 4: Datum: [...] | Tid (UTC): [...] */
    draw_field_aligned(4, colon1, "Datum",     qso->date,       11,
                       in_input && active_field == FLD_DATE, field_cursor_pos, cur_y, cur_x);
    draw_field_aligned(4, colon2, "Tid (UTC)", qso->time_utc,   6,
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
    char log_title[112];
    if (focus_mode == FOCUS_LOG) {
        snprintf(log_title, sizeof(log_title),
                 "Loggrader – Välj med Pilar, Enter = Redigera, Ctrl-O = Tillbaka (%zu QSO)",
                 log_list->count);
    } else if (search_filter != NULL && search_filter[0] != '\0') {
        snprintf(log_title, sizeof(log_title), "Loggrader (Filter: %s | Totalt: %zu QSO)",
                 search_filter, log_list->count);
    } else {
        snprintf(log_title, sizeof(log_title), "Loggrader (%zu QSO)", log_list->count);
    }
    draw_box_with_title(log_top, 0, log_height, COLS, log_title, focus_mode == FOCUS_LOG);

    int hdr_row = log_top + 1;
    int first_data_row = log_top + 2;
    int avail_log_rows = (log_top + log_height - 1) - first_data_row;
    if (avail_log_rows < 0) avail_log_rows = 0;

    /* Frekv-kolumnen är högerställd (%8s) och formaterad med 3 decimaler */
    attron(COLOR_PAIR(CP_TABLE_HEADER) | A_BOLD);
    if (COLS >= 92) {
        mvprintw(hdr_row, 2, "%-10s %-5s %-11s %8s %-5s %-5s %-7s %-8s %-12s %-s",
                 "Datum", "UTC", "Signal", "Frekv", "Band", "Mode", "RST S/M", "Lokator", "Namn", "QTH");
    } else if (COLS >= 74) {
        mvprintw(hdr_row, 2, "%-10s %-5s %-10s %8s %-5s %-5s %-7s %-6s %-s",
                 "Datum", "UTC", "Signal", "Frekv", "Band", "Mode", "RST", "Lok.", "Namn/QTH");
    } else {
        mvprintw(hdr_row, 2, "%-10s %-5s %-9s %8s %-4s %-4s %-7s %-s",
                 "Datum", "UTC", "Signal", "Frekv", "Band", "Mode", "RST", "Namn");
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

        char d_disp[16], t_disp[16], f_disp[16], rst_disp[36];
        format_date_yyyy_mm_dd(item->date, d_disp, sizeof(d_disp));
        format_time_hh_mm(item->time_utc, t_disp, sizeof(t_disp));
        parse_and_normalize_freq(item->freq, f_disp, sizeof(f_disp));
        snprintf(rst_disp, sizeof(rst_disp), "%s/%s",
                 item->rst_sent[0] ? item->rst_sent : "-",
                 item->rst_rcvd[0] ? item->rst_rcvd : "-");

        if (is_selected_row) {
            attron(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
            mvhline(r, 1, ' ', COLS - 2);
            *cur_y = r;
            *cur_x = 2;
        } else {
            attron(COLOR_PAIR(CP_LOG_ROW) | A_BOLD);
        }

        if (COLS >= 92) {
            int rem_qth = (COLS - 2) - 82;
            if (rem_qth < 4) rem_qth = 4;
            mvprintw(r, 2, "%-10.10s %-5.5s %-11.11s %8.8s %-5.5s %-5.5s %-7.7s %-8.8s %-12.12s %-.*s",
                     d_disp[0] ? d_disp : "-",
                     t_disp[0] ? t_disp : "-",
                     item->call[0] ? item->call : "-",
                     f_disp[0] ? f_disp : "-",
                     item->band[0] ? item->band : "-",
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
            mvprintw(r, 2, "%-10.10s %-5.5s %-10.10s %8.8s %-5.5s %-5.5s %-7.7s %-6.6s %-.*s",
                     d_disp[0] ? d_disp : "-",
                     t_disp[0] ? t_disp : "-",
                     item->call[0] ? item->call : "-",
                     f_disp[0] ? f_disp : "-",
                     item->band[0] ? item->band : "-",
                     item->mode[0] ? item->mode : "-",
                     rst_disp,
                     item->gridsquare[0] ? item->gridsquare : "-",
                     rem, nq);
        } else {
            int rem = (COLS - 2) - 54;
            if (rem < 3) rem = 3;
            mvprintw(r, 2, "%-10.10s %-5.5s %-9.9s %8.8s %-4.4s %-4.4s %-7.7s %-.*s",
                     d_disp[0] ? d_disp : "-",
                     t_disp[0] ? t_disp : "-",
                     item->call[0] ? item->call : "-",
                     f_disp[0] ? f_disp : "-",
                     item->band[0] ? item->band : "-",
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

/* F11: Hjälpskärm */
static void run_help_modal(const StationSettings *st) {
    while (1) {
        erase();
        draw_top_bar(st);
        draw_box_with_title(1, 0, LINES - 2, COLS, "Hjälp – QSO-logg (F11)", 1);

        attron(COLOR_PAIR(CP_TITLE) | A_BOLD);
        mvprintw(3, 3, "Tangentbordskommandon:");
        attroff(COLOR_PAIR(CP_TITLE) | A_BOLD);

        attron(COLOR_PAIR(CP_LABEL));
        mvprintw(4,  4, "Tab / Shift-Tab   : Hoppa till nästa / föregående inmatningsfält");
        mvprintw(5,  4, "Vänster / Höger   : Hoppa till föregående / nästa fält");
        mvprintw(6,  4, "Pil upp / Pil ned : Gå till närmaste fält över eller under aktivt fält");
        mvprintw(7,  4, "Enter på Band     : Aktivera bandval (byt med pilar, Enter bekräftar)");
        mvprintw(8,  4, "Enter / F2        : Logga QSO direkt med ifyllda uppgifter");
        mvprintw(9,  4, "Esc / F1          : Rensa inmatningsrutan (UTC-klockan tickar igen)");
        mvprintw(10, 4, "Ctrl-O            : Växla mellan Inmatningsrutan och Loggraderna");
        mvprintw(11, 4, "                    (Välj loggrad med Pil upp/ned, Enter = Redigera)");
        mvprintw(12, 4, "F3 / Ctrl-F       : Sök/filtrera på anropssignal bland loggraderna");
        mvprintw(13, 4, "F11               : Visa denna hjälpsida");
        mvprintw(14, 4, "F12               : Inställningar (Station, Färgschema, SM Callbook, Auto RST)");
        mvprintw(15, 4, "F10 / Ctrl-Q      : Avsluta programmet");

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
        mvprintw(row, 2, "Tryck Esc, Enter eller F11 för att stänga hjälpen.");
        attroff(COLOR_PAIR(CP_FKEY_LABEL));

        refresh();

        int ch = getch();
        if (ch == 27 || ch == '\n' || ch == '\r' || ch == KEY_ENTER || ch == KEY_F(11)) {
            break;
        }
    }
}

/* Rensa hela loggboken i minnet och på disk (qsolog.adi skrivs över med tom ADIF-header) */
static int clear_log_book(QSOList *log_list, const StationSettings *st) {
    if (log_list != NULL) {
        log_list->count = 0;
    }
    FILE *fp = fopen(LOG_FILE, "w");
    if (fp == NULL) return -1;
    fprintf(fp, "ADIF Log skapad av qsolog\n");
    fprintf(fp, "<ADIF_VER:5>3.1.4\n");
    fprintf(fp, "<PROGRAMID:6>qsolog\n");
    if (st != NULL && st->call[0] != '\0') {
        fprintf(fp, "<STATION_CALLSIGN:%zu>%s\n", strlen(st->call), st->call);
    }
    fprintf(fp, "<EOH>\n\n");
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

/* F3: Sök/filtrera på anropssignal i Loggrader */
static void run_search_bar(const StationSettings *st, char *search_filter, size_t filter_sz) {
    int cpos = utf8_display_width(search_filter);
    while (1) {
        int cur_y = LINES - 1, cur_x = 42;
        draw_top_bar(st);
        mvhline(LINES - 1, 0, ' ', COLS);
        draw_field_aligned(LINES - 1, 40, "Sök signal (Enter=Klar, Tom=Visa alla)",
                           search_filter, 14, 1, cpos, &cur_y, &cur_x);
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
            char c = (char)toupper((unsigned char)ch);
            utf8_insert_byte_at(search_filter, filter_sz, &cpos, c);
        }
    }
}

/* Sök efter en signal i loggraderna (nyaste först).
 * Om signalen finns och har namn och/eller QTH, kopieras dessa och 1 returneras.
 * Annars returneras 0. */
static int find_call_in_log(const QSOList *log_list, const char *call_in,
                            char *name_out, size_t name_sz,
                            char *qth_out, size_t qth_sz) {
    if (log_list == NULL || call_in == NULL || call_in[0] == '\0') return 0;
    for (int i = (int)log_list->count - 1; i >= 0; --i) {
        const QSO *q = &log_list->items[i];
        if (strcasecmp(q->call, call_in) == 0) {
            if ((q->name[0] != '\0' || q->qth[0] != '\0')) {
                if (name_out && name_sz > 0) copy_str(name_out, name_sz, q->name);
                if (qth_out && qth_sz > 0) copy_str(qth_out, qth_sz, q->qth);
                return 1;
            }
        }
    }
    return 0;
}

/* Utför uppslagning av Namn och QTH:
 * 1. Kontrollerar först om signalen redan finns i befintliga loggrader med namn/QTH.
 * 2. Om inte, och om SMCB är påslaget och signalen är svensk, görs SM Callbook-uppslag online. */
static void maybe_lookup_call_details(QSO *qso, const QSOList *log_list,
                                      const StationSettings *st,
                                      char *last_smcb_call, size_t last_smcb_sz,
                                      char *status_msg, size_t status_sz) {
    if (qso->call[0] == '\0') return;

    /* 1. Sök i tidigare loggade rader */
    char log_name[64] = "", log_qth[64] = "";
    if (find_call_in_log(log_list, qso->call, log_name, sizeof(log_name), log_qth, sizeof(log_qth))) {
        if (log_name[0] != '\0') copy_str(qso->name, sizeof(qso->name), log_name);
        if (log_qth[0] != '\0')  copy_str(qso->qth, sizeof(qso->qth), log_qth);
        snprintf(status_msg, status_sz, "Från logg: %s%s%s",
                 log_name, (log_name[0] && log_qth[0]) ? ", " : "", log_qth);
        return;
    }

    /* 2. Slå upp online i SM Callbook om aktiverat och svensk signal */
    if (!st->smcb_enabled) return;

    char base_call[32];
    if (!extract_swedish_base_call(qso->call, base_call, sizeof(base_call))) return;
    if (strcasecmp(base_call, last_smcb_call) == 0) return;

    copy_str(last_smcb_call, last_smcb_sz, base_call);
    char found_name[64] = "", found_qth[64] = "";
    if (lookup_sm_callbook(base_call,
                           found_name, sizeof(found_name),
                           found_qth, sizeof(found_qth))) {
        if (found_name[0] != '\0') copy_str(qso->name, sizeof(qso->name), found_name);
        if (found_qth[0] != '\0')  copy_str(qso->qth, sizeof(qso->qth), found_qth);
        snprintf(status_msg, status_sz, "SMCB: %s, %s", found_name, found_qth);
    } else {
        snprintf(status_msg, status_sz, "SMCB: Ingen träff för %s", base_call);
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
                                          char *status_msg, size_t status_sz) {
    if (leaving_field == FLD_CALL) {
        maybe_lookup_call_details(qso, log_list, st, last_smcb_call, last_smcb_sz, status_msg, status_sz);
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
                                 char *status_msg, size_t status_sz) {
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
        copy_str(qso->band, sizeof(qso->band), "20m");
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

    persist_qso_radio_settings(qso, st);

    if (*editing_index >= 0 && (size_t)(*editing_index) < log_list->count) {
        int idx = *editing_index;
        log_list->items[idx] = *qso;
        if (save_all_qsos_adif(LOG_FILE, log_list, st) == 0) {
            snprintf(status_msg, status_sz, "Uppdaterade QSO #%d med %s", idx + 1, qso->call);
            reset_qso_form(qso, st, time_frozen, active_field, field_cursor_pos, editing_index, last_smcb_call);
        } else {
            copy_str(status_msg, status_sz, "Fel: Kunde inte spara ändring till ADIF!");
        }
    } else {
        if (append_qso_adif(LOG_FILE, qso, st) == 0) {
            qso_list_push(log_list, qso);
            snprintf(status_msg, status_sz, "Loggade %s (%s %s)",
                     qso->call, qso->band, qso->mode);
            reset_qso_form(qso, st, time_frozen, active_field, field_cursor_pos, editing_index, last_smcb_call);
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
    FocusMode focus_mode = FOCUS_INPUT;
    int selected_log_pos = 0;
    int log_scroll_offset = 0;
    char search_filter[32] = "";
    char last_smcb_call[32] = "";
    char status_msg[128] = "";

    reset_qso_form(&current_qso, &station, &time_frozen, &active_field, &field_cursor_pos, &editing_index, last_smcb_call);

    while (1) {
        if (!time_frozen && editing_index < 0) {
            get_utc_strings(current_qso.date, sizeof(current_qso.date),
                            current_qso.time_utc, sizeof(current_qso.time_utc),
                            NULL, 0);
        }

        int cur_y = 2, cur_x = 16;
        const char *active_filter = (search_filter[0] != '\0') ? search_filter :
                                    (station.filter_log && current_qso.call[0] != '\0') ? current_qso.call : NULL;
        draw_main_screen(&station, &current_qso, focus_mode, active_field, band_selecting,
                         field_cursor_pos, time_frozen, editing_index,
                         &log_list, selected_log_pos, &log_scroll_offset,
                         active_filter, status_msg, &cur_y, &cur_x);

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
                }
            }
            continue;
        }
        idle_ticks_on_call = 0;

        /* Globala funktionstangenter, ESC och Ctrl-O */
        if (ch == KEY_F(10) || ch == 17) { /* F10 eller Ctrl+Q: Avsluta */
            break;
        } else if (ch == KEY_F(11)) { /* F11: Hjälp */
            run_help_modal(&station);
            continue;
        } else if (ch == 15) { /* Ctrl-O: Växla mellan inmatning och loggrader */
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
                    copy_str(status_msg, sizeof(status_msg), "Loggrader: Välj med pilar, Enter = Redigera.");
                } else {
                    copy_str(status_msg, sizeof(status_msg), "Inga loggrader att välja ännu.");
                }
            } else {
                focus_mode = FOCUS_INPUT;
                sync_cursor_to_field_end(&current_qso, active_field, &field_cursor_pos);
                copy_str(status_msg, sizeof(status_msg), "Åter i inmatningsfälten.");
            }
            continue;
        } else if (ch == KEY_F(1) || ch == 27) {
            /* F1 eller ESC: Rensa inmatningsfältet och låt UTC-klockan ticka igen */
            focus_mode = FOCUS_INPUT;
            band_selecting = 0;
            reset_qso_form(&current_qso, &station, &time_frozen, &active_field, &field_cursor_pos, &editing_index, last_smcb_call);
            preferred_col = 0;
            copy_str(status_msg, sizeof(status_msg), "Formulär rensat – UTC-klockan tickar igen.");
            continue;
        } else if (ch == KEY_F(12)) { /* F12: Ändra inställningar */
            band_selecting = 0;
            run_settings_modal(&station, &current_qso, editing_index, &log_list,
                               &selected_log_pos, &log_scroll_offset,
                               status_msg, sizeof(status_msg));
            continue;
        } else if (ch == KEY_F(3) || ch == 6) { /* F3 eller Ctrl+F: Sök */
            band_selecting = 0;
            run_search_bar(&station, search_filter, sizeof(search_filter));
            selected_log_pos = 0;
            log_scroll_offset = 0;
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
            } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
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
                                          status_msg, sizeof(status_msg));
            try_save_current_qso(&current_qso, &time_frozen, &active_field,
                                 &field_cursor_pos, &editing_index,
                                 &station, &log_list, last_smcb_call, status_msg, sizeof(status_msg));
            preferred_col = field_default_col(active_field);
        } else if (ch == '\t' || ch == KEY_RIGHT) {
            /* Tab och Högerpil hoppar till nästa inmatningsfält */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg));
            active_field = (active_field + 1) % QSO_FIELD_COUNT;
            preferred_col = field_default_col(active_field);
            sync_cursor_to_field_end(&current_qso, active_field, &field_cursor_pos);
        } else if (ch == KEY_BTAB || ch == KEY_LEFT) {
            /* Shift-Tab och Vänsterpil hoppar till föregående inmatningsfält */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg));
            active_field = (active_field + QSO_FIELD_COUNT - 1) % QSO_FIELD_COUNT;
            preferred_col = field_default_col(active_field);
            sync_cursor_to_field_end(&current_qso, active_field, &field_cursor_pos);
        } else if (ch == KEY_DOWN) {
            /* Pil ned går till närmaste fält rakt under det man står i */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg));
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
                                          status_msg, sizeof(status_msg));
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
            }
        }
    }

    apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                  last_smcb_call, sizeof(last_smcb_call),
                                  status_msg, sizeof(status_msg));
    persist_qso_radio_settings(&current_qso, &station);

    endwin();
    free(log_list.items);
    free_dxcc_db();
    curl_global_cleanup();
    return 0;
}
