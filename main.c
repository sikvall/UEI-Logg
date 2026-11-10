#include "qsolog.h"
#include "tui.h"
#include <curl/curl.h>
#include <pthread.h>
#include <wchar.h>
#include <wctype.h>
#include <math.h>

/* Tillåtna band i bandlistan samt standardfrekvens (MHz) med 3 decimaler */
const char * const BAND_OPTIONS[] = {
    "160 m", "80 m", "60 m", "40 m", "30 m", "20 m",
    "17 m",  "15 m", "12 m", "10 m", "6 m",  "2 m",
    "70 cm", "23 cm"
};
const char * const BAND_DEFAULT_FREQ[] = {
    "1.900",   "3.600",   "5.400",   "7.100",   "10.100",  "14.200",
    "18.100",  "21.200",  "24.900",  "28.800",  "51.000",  "145.000",
    "432.200", "1296.200"
};

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
void copy_str(char *dest, size_t dest_size, const char *src) {
    if (dest_size == 0) return;
    size_t len = strlen(src);
    if (len >= dest_size) {
        len = dest_size - 1;
    }
    memcpy(dest, src, len);
    dest[len] = '\0';
}

/* Trimma radbrytningar och mellanslag i slutet av en sträng */
void trim_newline(char *str) {
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r' || str[len - 1] == ' ')) {
        str[--len] = '\0';
    }
}

/* Rensa bort inledande och avslutande mellanslag/tabbar */
void trim_str(char *str) {
    if (str == NULL) return;
    char *start = str;
    while (*start == ' ' || *start == '\t') start++;
    if (*start == '\0') {
        str[0] = '\0';
        return;
    }
    char *end = start + strlen(start) - 1;
    while (end > start && (*end == ' ' || *end == '\t')) {
        *end = '\0';
        end--;
    }
    if (start > str) {
        memmove(str, start, strlen(start) + 1);
    }
}

/* Beräkna antalet UTF-8-tecken (synliga tecken) i en sträng */
int utf8_display_width(const char *str) {
    int width = 0;
    for (const unsigned char *p = (const unsigned char *)str; *p != '\0'; ++p) {
        if ((*p & 0xC0) != 0x80) {
            width++;
        }
    }
    return width;
}

/* Översätt teckenposition (0..utf8_display_width) till byte-offset i en UTF-8-sträng */
size_t utf8_char_to_byte(const char *str, int char_pos) {
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
void utf8_backspace_at(char *str, int *char_pos) {
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
void utf8_delete_at(char *str, int char_pos) {
    int total_chars = utf8_display_width(str);
    if (char_pos < 0 || char_pos >= total_chars) return;

    size_t start_b = utf8_char_to_byte(str, char_pos);
    size_t end_b = utf8_char_to_byte(str, char_pos + 1);
    size_t tail_len = strlen(str + end_b);
    memmove(str + start_b, str + end_b, tail_len + 1);
}

/* Infoga en byte vid markörpositionen i en UTF-8-sträng */
void utf8_insert_byte_at(char *str, size_t max_size, int *char_pos, char ch) {
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
void to_upper_str(char *str) {
    for (; *str; ++str) {
        if ((unsigned char)*str < 128) {
            *str = (char)toupper((unsigned char)*str);
        }
    }
}

/* Skiftlägesokänslig delsökning som hanterar både ASCII och UTF-8 (t.ex. å, ä, ö) */
int utf8_contains_ci(const char *haystack, const char *needle) {
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
double parse_and_normalize_freq(const char *in, char *norm_out, size_t out_sz) {
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
const char *guess_band_from_freq(const char *freq_str) {
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
void resolve_band_from_freq(const char *freq_str, char *band_out, size_t out_sz) {
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

int find_band_index(const char *band_str) {
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
void format_band_display(const char *in, char *out, size_t out_sz) {
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

void format_date_yyyy_mm_dd(const char *raw, char *out, size_t out_sz) {
    char digits[16];
    normalize_date_adif(raw, digits, sizeof(digits));
    if (strlen(digits) == 8) {
        snprintf(out, out_sz, "%.4s-%.2s-%.2s", digits, digits + 4, digits + 6);
    } else {
        copy_str(out, out_sz, raw);
    }
}

void format_time_hh_mm(const char *raw, char *out, size_t out_sz) {
    char digits[16];
    normalize_time_adif(raw, digits, sizeof(digits));
    if (strlen(digits) >= 4) {
        snprintf(out, out_sz, "%.2s:%.2s", digits, digits + 2);
    } else {
        copy_str(out, out_sz, raw);
    }
}

/* Läs in stationsinställningar från station.conf (SMCB är default 0 = avstängd) */
void load_station_settings(const char *filename, StationSettings *st) {
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
    } else if (strcasecmp(tag, "APP_QSOLOG_HANDLED") == 0) {
        qso->handled = (atoi(val) != 0 || strcasecmp(val, "Y") == 0 || strcasecmp(val, "YES") == 0 || strcasecmp(val, "TRUE") == 0);
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

static int qso_is_duplicate(const QSOList *list, const QSO *q) {
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

static void finalize_imported_qso(QSO *current, const StationSettings *st) {
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

static int cmp_qso_chronological(const void *a_ptr, const void *b_ptr) {
    const QSO *a = (const QSO *)a_ptr;
    const QSO *b = (const QSO *)b_ptr;
    int c = strcmp(a->date, b->date);
    if (c != 0) return c;
    return strcmp(a->time_utc, b->time_utc);
}

const char *get_path_basename(const char *path) {
    if (path == NULL) return "";
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

/* Importera och slå samman ADIF-poster från en annan fil till log_list */
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

/* Exportera QSO till en extern ADIF-fil.
 * export_mode:
 *   EXPORT_MODE_ALL (0)      = alla QSO i listan
 *   EXPORT_MODE_MARKED (1)   = endast markerade poster (marked == 1)
 *   EXPORT_MODE_ONLY_NEW (2) = endast nya poster (handled == 0)
 * Rader som exporteras markeras som hanterade (handled = 1).
 * Returnerar antal exporterade QSO, eller -1 vid filfel. */
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

    /* Spara loggen så att handled-flaggan bevaras även i qsolog.adi */
    if (count > 0) {
        save_all_qsos_adif(LOG_FILE, list, st);
    }

    return count;
}

/* Tolka tidszon-offset från inställningar (t.ex. "+0100", "+02:00", "+1", "-0500", "UTC+2").
 * Returnerar 1 om giltig offset hittades och sätter *offset_sec_out, annars 0 (använd locale). */
int parse_tz_offset_seconds(const char *tz_str, long *offset_sec_out) {
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

void get_utc_strings(char *date_buf, size_t date_sz,
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

/* Rensa hela loggboken i minnet och på disk (qsolog.adi skrivs över med tom ADIF 3.1.8-header) */
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

/* Bygg standardfilnamn för export:
 * Egen stationssignal_datum_tid där datum är YYMMDD och tid är HHMMSS.
 * Om endast nya loggrader exporteras läggs _nya till på slutet.
 * Filnamnet avslutas alltid med .adi. */
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

    /* Rensa tecken som inte bör ingå i filnamn (t.ex. snedstreck vid portabel-suffix) */
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

    tui_init();
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
            } else if (ch == KEY_CTRL_HOME) {
                /* Ctrl-Home: översta raden i hela loggen */
                selected_log_pos = 0;
                log_scroll_offset = 0;
            } else if (ch == KEY_CTRL_END) {
                /* Ctrl-End: sista raden i hela loggen */
                if (f_cnt > 0) {
                    selected_log_pos = f_cnt - 1;
                    int avail = get_avail_log_rows();
                    log_scroll_offset = f_cnt - avail;
                    if (log_scroll_offset < 0) log_scroll_offset = 0;
                }
            } else if (ch == KEY_HOME || ch == KEY_SHOME || ch == KEY_A1) {
                /* Home: första raden på den sida vi ser */
                if (f_cnt > 0) {
                    selected_log_pos = log_scroll_offset;
                    if (selected_log_pos >= f_cnt) selected_log_pos = f_cnt - 1;
                    if (selected_log_pos < 0) selected_log_pos = 0;
                }
            } else if (ch == KEY_END || ch == KEY_SEND || ch == KEY_LL || ch == KEY_C1) {
                /* End: sista raden på den sida vi ser */
                if (f_cnt > 0) {
                    int avail = get_avail_log_rows();
                    int last_vis = log_scroll_offset + avail - 1;
                    if (last_vis >= f_cnt) last_vis = f_cnt - 1;
                    if (last_vis < 0) last_vis = 0;
                    selected_log_pos = last_vis;
                }
            } else if (ch == KEY_PPAGE) {
                /* PgUp: bläddra en sida uppåt i taget */
                int avail = get_avail_log_rows();
                selected_log_pos -= avail;
                if (selected_log_pos < 0) selected_log_pos = 0;
                log_scroll_offset -= avail;
                if (log_scroll_offset < 0) log_scroll_offset = 0;
            } else if (ch == KEY_NPAGE) {
                /* PgDown: bläddra en sida nedåt i taget */
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
                /* F5: Markera som hanterad (alla markerade rader, eller aktuell om ingen markerad) */
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
                /* F6: Markera som ny (alla markerade rader, eller aktuell om ingen markerad) */
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
        } else if (ch == KEY_HOME || ch == KEY_SHOME || ch == KEY_A1 || ch == KEY_CTRL_HOME) {
            /* Home i inmatningsfältet: gå till anropssignal (FLD_CALL) */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg),
                                          current_qrb, sizeof(current_qrb));
            active_field = FLD_CALL;
            preferred_col = 0;
            sync_cursor_to_field_end(&current_qso, active_field, &field_cursor_pos);
        } else if (ch == KEY_END || ch == KEY_SEND || ch == KEY_LL || ch == KEY_C1 || ch == KEY_CTRL_END) {
            /* End i inmatningsfältet: gå till Not / Kommentarsfältet (FLD_COMM) */
            band_selecting = 0;
            apply_field_defaults_on_leave(&current_qso, active_field, &log_list, &station,
                                          last_smcb_call, sizeof(last_smcb_call),
                                          status_msg, sizeof(status_msg),
                                          current_qrb, sizeof(current_qrb));
            active_field = FLD_COMM;
            preferred_col = 0;
            sync_cursor_to_field_end(&current_qso, active_field, &field_cursor_pos);
        } else if (ch == KEY_PPAGE) {
            /* PgUp i inmatningsfältet: bläddra loggraderna uppåt */
            int avail = get_avail_log_rows();
            log_scroll_offset -= avail;
            if (log_scroll_offset < 0) log_scroll_offset = 0;
            selected_log_pos -= avail;
            if (selected_log_pos < 0) selected_log_pos = 0;
        } else if (ch == KEY_NPAGE) {
            /* PgDown i inmatningsfältet: bläddra loggraderna nedåt */
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
    tui_cleanup();
    free(log_list.items);
    free_dxcc_db();
    curl_global_cleanup();
    return 0;
}
