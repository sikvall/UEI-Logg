/**
 * @file util.c
 * @brief Implementation av hjälpfunktioner för strängar, UTF-8, datum, tid, frekvenser och band.
 */

#include "util.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>

/* Tillåtna amatörradioband i bandlistan och deras standardanropsfrekvenser */
const char * const BAND_OPTIONS[] = {
    "160 m", "80 m", "60 m", "40 m", "30 m",
    "20 m",  "17 m", "15 m", "12 m", "10 m",
    "6 m",   "2 m",  "70 cm", "23 cm"
};

const char * const BAND_DEFAULT_FREQ[] = {
    "1.840", "3.650", "5.354", "7.100", "10.120",
    "14.200", "18.120", "21.200", "24.940", "28.500",
    "50.150", "144.200", "432.200", "1296.200"
};

/**
 * @brief cURL callback som dynamiskt ackumulerar nedladdade bytes till en CurlBuf-struktur.
 * @param ptr Pekare till inkommande data från libcurl.
 * @param size Storlek på varje dataelement.
 * @param nmemb Antal element.
 * @param userdata Pekare till vår CurlBuf-struktur.
 * @return Antal bytes som hanterades (totalt = size * nmemb).
 */
size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata) {
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

/**
 * @brief Säker strängkopiering med garanterad null-terminering och bufferskydd.
 * @param dest Målbuffer.
 * @param dest_size Storlek på målbuffer i bytes.
 * @param src Källsträng (kan vara NULL).
 */
void copy_str(char *dest, size_t dest_size, const char *src) {
    if (dest == NULL || dest_size == 0) return;
    if (src == NULL) {
        dest[0] = '\0';
        return;
    }
    size_t i = 0;
    while (i + 1 < dest_size && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/**
 * @brief Tar bort radbrytningstecken (\\r och \\n) från slutet av en sträng.
 * @param str Strängen som ska trimmas.
 */
void trim_newline(char *str) {
    if (str == NULL) return;
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == '\n' || str[len - 1] == '\r')) {
        str[len - 1] = '\0';
        len--;
    }
}

/**
 * @brief Tar bort inledande och avslutande mellanslag och tabbar.
 * @param str Strängen som ska trimmas.
 */
void trim_str(char *str) {
    if (str == NULL) return;
    char *start = str;
    while (*start == ' ' || *start == '\t') start++;
    if (start != str) {
        memmove(str, start, strlen(start) + 1);
    }
    size_t len = strlen(str);
    while (len > 0 && (str[len - 1] == ' ' || str[len - 1] == '\t' || str[len - 1] == '\r' || str[len - 1] == '\n')) {
        str[len - 1] = '\0';
        len--;
    }
}

/**
 * @brief Konverterar en ASCII-sträng till versaler (in-place).
 * @param str Strängen som ska omvandlas.
 */
void to_upper_str(char *str) {
    if (str == NULL) return;
    for (char *p = str; *p; ++p) {
        *p = (char)toupper((unsigned char)*p);
    }
}

/**
 * @brief Hämtar basnamnet för en sökväg (filnamnet efter sista snedstrecket).
 * @param path Sökvägen.
 * @return Pekare till basnamnet i sökvägen.
 */
const char *get_path_basename(const char *path) {
    if (path == NULL) return "";
    const char *slash = strrchr(path, '/');
    return slash ? slash + 1 : path;
}

/**
 * @brief Enkel Base64-kodning för strängar (används bland annat för SMCB token-generering).
 * @param in Indatasträng.
 * @param out Utdatabuffer för Base64-strängen.
 * @param out_sz Storlek på utdatabuffert.
 */
void base64_encode_str(const char *in, char *out, size_t out_sz) {
    static const char b64[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
    if (in == NULL || out == NULL || out_sz == 0) return;
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

/**
 * @brief Beräknar den synliga kolumnbredden för en UTF-8-sträng i terminalen (wcwidth).
 * @param str UTF-8-sträng.
 * @return Antal synliga kolumner.
 */
int utf8_display_width(const char *str) {
    if (str == NULL) return 0;
    int width = 0;
    const char *p = str;
    mbstate_t ps;
    memset(&ps, 0, sizeof(ps));
    while (*p != '\0') {
        wchar_t wc;
        size_t len = mbrtowc(&wc, p, MB_CUR_MAX, &ps);
        if (len == (size_t)-1 || len == (size_t)-2 || len == 0) {
            if (len == (size_t)-1 || len == (size_t)-2) {
                p++;
                width += 1;
                memset(&ps, 0, sizeof(ps));
            } else {
                break;
            }
        } else {
            int w = wcwidth(wc);
            width += (w >= 0) ? w : 1;
            p += len;
        }
    }
    return width;
}

/**
 * @brief Konverterar en teckenposition (antal unicode-tecken) till byte-offset i en UTF-8-sträng.
 * @param str UTF-8-sträng.
 * @param char_pos Antal tecken från början.
 * @return Byte-offset i strängen.
 */
size_t utf8_char_to_byte(const char *str, int char_pos) {
    if (str == NULL || char_pos <= 0) return 0;
    size_t byte_idx = 0;
    int cur_char = 0;
    mbstate_t ps;
    memset(&ps, 0, sizeof(ps));
    while (str[byte_idx] != '\0' && cur_char < char_pos) {
        wchar_t wc;
        size_t len = mbrtowc(&wc, str + byte_idx, MB_CUR_MAX, &ps);
        if (len == (size_t)-1 || len == (size_t)-2 || len == 0) {
            byte_idx++;
            memset(&ps, 0, sizeof(ps));
        } else {
            byte_idx += len;
        }
        cur_char++;
    }
    return byte_idx;
}

/**
 * @brief Raderar tecknet omedelbart till vänster om markören i en UTF-8-sträng (backspace).
 * @param str Strängen som redigeras.
 * @param char_pos Pekare till markörens aktuella teckenposition (minskas med 1).
 */
void utf8_backspace_at(char *str, int *char_pos) {
    if (str == NULL || char_pos == NULL || *char_pos <= 0) return;
    size_t left_byte = utf8_char_to_byte(str, *char_pos - 1);
    size_t cur_byte  = utf8_char_to_byte(str, *char_pos);
    if (cur_byte > left_byte) {
        size_t rest_len = strlen(str + cur_byte);
        memmove(str + left_byte, str + cur_byte, rest_len + 1);
        (*char_pos)--;
    }
}

/**
 * @brief Raderar tecknet under markören i en UTF-8-sträng (delete).
 * @param str Strängen som redigeras.
 * @param char_pos Markörens aktuella teckenposition.
 */
void utf8_delete_at(char *str, int char_pos) {
    if (str == NULL || char_pos < 0) return;
    size_t cur_byte  = utf8_char_to_byte(str, char_pos);
    size_t next_byte = utf8_char_to_byte(str, char_pos + 1);
    if (next_byte > cur_byte) {
        size_t rest_len = strlen(str + next_byte);
        memmove(str + cur_byte, str + next_byte, rest_len + 1);
    }
}

/**
 * @brief Infogar en byte/ett ASCII-tecken vid angiven markörposition i en UTF-8-sträng.
 * @param str Strängen som redigeras.
 * @param max_size Maximal storlek på strängens buffer i bytes.
 * @param char_pos Pekare till markörens aktuella teckenposition (ökas med 1).
 * @param ch Tecknet som ska infogas.
 */
void utf8_insert_byte_at(char *str, size_t max_size, int *char_pos, char ch) {
    if (str == NULL || char_pos == NULL) return;
    size_t cur_len = strlen(str);
    if (cur_len + 1 >= max_size) return;
    size_t byte_idx = utf8_char_to_byte(str, *char_pos);
    memmove(str + byte_idx + 1, str + byte_idx, cur_len - byte_idx + 1);
    str[byte_idx] = ch;
    (*char_pos)++;
}

/**
 * @brief Skriver över tecknet vid angiven markörposition i en UTF-8-sträng med en byte/ett ASCII-tecken.
 * Om markören är i slutet av eller bortom strängen läggs tecknet till i slutet.
 * @param str Strängen som redigeras.
 * @param max_size Maximal storlek på strängens buffer i bytes inklusive null-terminator.
 * @param char_pos Pekare till markörens aktuella teckenposition (ökas med 1).
 * @param ch Tecknet som ska skrivas.
 */
void utf8_overwrite_byte_at(char *str, size_t max_size, int *char_pos, char ch) {
    if (str == NULL || char_pos == NULL || max_size < 2) return;
    if (*char_pos < 0) *char_pos = 0;

    size_t cur_len = strlen(str);
    size_t byte_idx = utf8_char_to_byte(str, *char_pos);

    if (byte_idx < cur_len) {
        /* Ersätt befintligt tecken vid byte_idx med ch (1 byte) */
        size_t next_byte = utf8_char_to_byte(str, *char_pos + 1);
        size_t cur_char_bytes = (next_byte > byte_idx) ? (next_byte - byte_idx) : 1;
        if (cur_char_bytes != 1) {
            memmove(str + byte_idx + 1, str + byte_idx + cur_char_bytes, cur_len - (byte_idx + cur_char_bytes) + 1);
        }
        str[byte_idx] = ch;
    } else {
        /* Markören är i slutet: lägg till i slutet om plats finns */
        if (cur_len + 1 >= max_size) return;
        str[cur_len] = ch;
        str[cur_len + 1] = '\0';
    }
    (*char_pos)++;
}

/**
 * @brief Skiftlägesokänslig sökning (case-insensitive substring check) för UTF-8-strängar.
 * @param haystack Strängen att söka i.
 * @param needle Delsträngen att söka efter.
 * @return 1 om delsträngen finns, annars 0.
 */
int utf8_contains_ci(const char *haystack, const char *needle) {
    if (haystack == NULL || needle == NULL) return 0;
    if (needle[0] == '\0') return 1;

    size_t hlen = strlen(haystack);
    size_t nlen = strlen(needle);
    if (nlen > hlen) return 0;

    for (size_t i = 0; i <= hlen - nlen; ++i) {
        size_t j = 0;
        while (j < nlen && tolower((unsigned char)haystack[i + j]) == tolower((unsigned char)needle[j])) {
            j++;
        }
        if (j == nlen) return 1;
    }
    return 0;
}

/**
 * @brief Tolkar och normaliserar frekvensinmatning till MHz med 3 decimaler (t.ex. "14.200" eller "14200" -> "14.200").
 * @param in Indatasträng med frekvens.
 * @param norm_out Buffer för normaliserat MHz-värde (kan vara NULL).
 * @param out_sz Storlek på målbuffer.
 * @return Frekvensen som double i MHz.
 */
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
            if (khz < 1000) {
                mhz = (double)khz;
            } else if (khz >= 1240 && khz <= 1300) {
                mhz = (double)khz; /* 23 cm bandet (1240-1300 MHz) */
            } else {
                mhz = (double)khz / 1000.0;
            }
        }
    }

    if (norm_out && out_sz > 0) {
        if (mhz > 0.0) {
            unsigned long whole = (unsigned long)mhz;
            unsigned long frac = (unsigned long)((mhz - (double)whole) * 1000.0 + 0.5);
            if (frac >= 1000) {
                whole += frac / 1000;
                frac %= 1000;
            }
            snprintf(norm_out, out_sz, "%lu.%03lu", whole, frac);
        } else {
            norm_out[0] = '\0';
        }
    }

    return mhz;
}

/**
 * @brief Gissar amatörband utifrån ett numeriskt frekvensvärde i MHz.
 * @param f Frekvens i MHz.
 * @return Pekare till konstant bandnamn, eller NULL om utanför standardbanden.
 */
const char *guess_band_from_mhz(double f) {
    if (f <= 0.0) return NULL;

    if (f >= 1.810   && f <= 2.000)   return "160 m";
    if (f >= 3.500   && f <= 3.800)   return "80 m";
    if (f >= 5.3515  && f <= 5.3665)  return "60 m";
    if (f >= 7.000   && f <= 7.200)   return "40 m";
    if (f >= 10.100  && f <= 10.150)  return "30 m";
    if (f >= 14.000  && f <= 14.350)  return "20 m";
    if (f >= 18.068  && f <= 18.168)  return "17 m";
    if (f >= 21.000  && f <= 21.450)  return "15 m";
    if (f >= 24.890  && f <= 24.990)  return "12 m";
    if (f >= 28.000  && f <= 29.700)  return "10 m";
    if (f >= 50.000  && f <= 52.000)  return "6 m";
    if (f >= 70.000  && f <= 70.500)  return "4 m";
    if (f >= 144.000 && f <= 146.000) return "2 m";
    if (f >= 430.000 && f <= 440.000) return "70 cm";
    if (f >= 1240.0  && f <= 1300.0)  return "23 cm";

    return NULL;
}

/**
 * @brief Tolkar och normaliserar frekvens vid ADIF-import till MHz med decimaler (t.ex. "7" -> "7.000").
 * Vid import antas frekvensen vara i MHz enligt ADIF-standarden.
 * @param in Inmatad frekvenssträng från ADIF.
 * @param norm_out Buffer för normaliserad sträng ("7.000").
 * @param out_sz Storlek på målbuffer.
 * @return Frekvensen som flyttal i MHz, eller 0.0 vid ogiltig inmatning.
 */
double parse_and_normalize_freq_adif(const char *in, char *norm_out, size_t out_sz) {
    if (norm_out && out_sz > 0) norm_out[0] = '\0';
    if (in == NULL) return 0.0;

    while (*in == ' ' || *in == '\t' || *in == '\r' || *in == '\n') in++;
    if (*in == '\0') return 0.0;

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

        /* Återställning om frekvensen tidigare felaktigt delats med 1000 (t.ex. 0.021 -> 21.000) */
        if (mhz > 0.0 && mhz < 1.0) {
            double test_mhz = mhz * 1000.0;
            if (guess_band_from_mhz(test_mhz) != NULL) {
                mhz = test_mhz;
            }
        }
    } else {
        /* Inget decimaltecken: Vid import ska antas att frekvensen är i MHz med decimaler (t.ex. 7 -> 7.000) */
        char digits[24];
        size_t dj = 0;
        for (const char *p = in; *p != '\0' && dj < sizeof(digits) - 1; ++p) {
            if (isdigit((unsigned char)*p)) {
                digits[dj++] = *p;
            }
        }
        digits[dj] = '\0';

        if (dj > 0) {
            double val_num = atof(digits);
            if (val_num >= 10000.0 ||
                (val_num >= 1800.0 && val_num <= 2000.0) ||
                (val_num >= 3500.0 && val_num <= 3800.0) ||
                (val_num >= 7000.0 && val_num <= 7300.0)) {
                mhz = val_num / 1000.0;
            } else {
                mhz = val_num;
            }
        }
    }

    if (norm_out && out_sz > 0) {
        if (mhz > 0.0) {
            unsigned long whole = (unsigned long)mhz;
            unsigned long frac = (unsigned long)((mhz - (double)whole) * 1000.0 + 0.5);
            if (frac >= 1000) {
                whole += frac / 1000;
                frac %= 1000;
            }
            snprintf(norm_out, out_sz, "%lu.%03lu", whole, frac);
        } else {
            norm_out[0] = '\0';
        }
    }

    return mhz;
}

/**
 * @brief Gissar amatörradioband (t.ex. "20 m", "80 m", "70 cm") utifrån frekvens i MHz.
 * @param freq_str Frekvenssträng.
 * @return Pekare till konstant bandnamn, eller NULL om utanför standardbanden.
 */
const char *guess_band_from_freq(const char *freq_str) {
    double f = parse_and_normalize_freq(freq_str, NULL, 0);
    return guess_band_from_mhz(f);
}

/**
 * @brief Avgör och sätter bandnamn utifrån frekvenssträng, med fallback.
 * @param freq_str Frekvens i MHz.
 * @param band_out Utdatabuffer för bandnamn.
 * @param out_sz Storlek på utdatabuffert.
 */
void resolve_band_from_freq(const char *freq_str, char *band_out, size_t out_sz) {
    if (band_out == NULL || out_sz == 0) return;
    const char *guessed = guess_band_from_freq(freq_str);
    if (guessed != NULL) {
        copy_str(band_out, out_sz, guessed);
    }
}

/**
 * @brief Hitta index för ett givet band i BAND_OPTIONS-arrayen (0..BAND_OPTION_COUNT-1).
 * Hanterar både former med och utan mellanslag (t.ex. "80m" och "80 m").
 * @param band_str Bandnamn.
 * @return Index i BAND_OPTIONS, eller -1 om inte hittat.
 */
int find_band_index(const char *band_str) {
    if (band_str == NULL || band_str[0] == '\0') return -1;
    char clean[16];
    size_t cj = 0;
    for (const char *p = band_str; *p != '\0' && cj < sizeof(clean) - 1; ++p) {
        if (!isspace((unsigned char)*p)) {
            clean[cj++] = (char)tolower((unsigned char)*p);
        }
    }
    clean[cj] = '\0';

    for (int i = 0; i < BAND_OPTION_COUNT; ++i) {
        char opt_clean[16];
        size_t oj = 0;
        for (const char *p = BAND_OPTIONS[i]; *p != '\0' && oj < sizeof(opt_clean) - 1; ++p) {
            if (!isspace((unsigned char)*p)) {
                opt_clean[oj++] = (char)tolower((unsigned char)*p);
            }
        }
        opt_clean[oj] = '\0';

        if (strcmp(clean, opt_clean) == 0) {
            return i;
        }
    }
    return -1;
}

/**
 * @brief Formaterar bandnamn för visning med mellanslag mellan siffra och enhet (t.ex. "80 m", "70 cm").
 * @param in Inkommande bandsträng.
 * @param out Målbuffer.
 * @param out_sz Storlek på målbuffer.
 */
void format_band_display(const char *in, char *out, size_t out_sz) {
    if (out == NULL || out_sz == 0) return;
    if (in == NULL || in[0] == '\0') {
        out[0] = '\0';
        return;
    }
    int idx = find_band_index(in);
    if (idx >= 0 && idx < BAND_OPTION_COUNT) {
        copy_str(out, out_sz, BAND_OPTIONS[idx]);
        return;
    }
    char digits[16] = "";
    char unit[16] = "";
    size_t dj = 0, uj = 0;
    for (const char *p = in; *p != '\0'; ++p) {
        if (isdigit((unsigned char)*p)) {
            if (dj < sizeof(digits) - 1) digits[dj++] = *p;
        } else if (isalpha((unsigned char)*p)) {
            if (uj < sizeof(unit) - 1) unit[uj++] = (char)tolower((unsigned char)*p);
        }
    }
    digits[dj] = '\0';
    unit[uj] = '\0';
    if (dj > 0 && uj > 0) {
        snprintf(out, out_sz, "%s %s", digits, unit);
    } else {
        copy_str(out, out_sz, in);
    }
}

/**
 * @brief Normaliserar bandnamn för ADIF-export enligt specifikation (utan mellanslag, t.ex. "80M").
 * @param in Visningsnamn (t.ex. "80 m").
 * @param out ADIF-format (t.ex. "80M").
 * @param out_sz Storlek på buffer.
 */
void normalize_band_adif(const char *in, char *out, size_t out_sz) {
    if (out == NULL || out_sz == 0) return;
    size_t oj = 0;
    for (const char *p = in ? in : ""; *p != '\0' && oj < out_sz - 1; ++p) {
        if (!isspace((unsigned char)*p)) {
            out[oj++] = (char)toupper((unsigned char)*p);
        }
    }
    out[oj] = '\0';
}

/**
 * @brief Extraherar endast siffror från en datumsträng för ADIF YYYYMMDD.
 * @param in Rå datumsträng (t.ex. "2026-11-10").
 * @param out Normaliserad datumsträng (t.ex. "20261110").
 * @param out_sz Storlek på målbuffer.
 */
void normalize_date_adif(const char *in, char *out, size_t out_sz) {
    if (out == NULL || out_sz == 0) return;
    size_t oj = 0;
    for (const char *p = in ? in : ""; *p != '\0' && oj < out_sz - 1; ++p) {
        if (isdigit((unsigned char)*p)) {
            out[oj++] = *p;
        }
    }
    out[oj] = '\0';
}

/**
 * @brief Extraherar endast siffror från en tidssträng för ADIF HHMM (eller HHMMSS).
 * @param in Rå tidssträng (t.ex. "21:30").
 * @param out Normaliserad tidssträng (t.ex. "2130").
 * @param out_sz Storlek på målbuffer.
 */
void normalize_time_adif(const char *in, char *out, size_t out_sz) {
    if (out == NULL || out_sz == 0) return;
    char clean[16];
    size_t cj = 0;
    for (const char *p = in ? in : ""; *p != '\0' && cj < sizeof(clean) - 1; ++p) {
        if (isdigit((unsigned char)*p)) {
            clean[cj++] = *p;
        }
    }
    clean[cj] = '\0';
    if (cj >= 4) {
        clean[4] = '\0';
        copy_str(out, out_sz, clean);
    } else {
        copy_str(out, out_sz, in ? in : "");
    }
}

/**
 * @brief Formaterar rått datum till YYYY-MM-DD för visning i UI.
 * @param raw Inkommande datumsträng (t.ex. "20261110").
 * @param out Utdatabuffer (t.ex. "2026-11-10").
 * @param out_sz Storlek på målbuffer.
 */
void format_date_yyyy_mm_dd(const char *raw, char *out, size_t out_sz) {
    char digits[16];
    normalize_date_adif(raw, digits, sizeof(digits));
    if (strlen(digits) == 8) {
        snprintf(out, out_sz, "%.4s-%.2s-%.2s", digits, digits + 4, digits + 6);
    } else {
        copy_str(out, out_sz, raw ? raw : "");
    }
}

/**
 * @brief Formaterar rå tid till HH:MM för visning i UI.
 * @param raw Inkommande tidssträng (t.ex. "2130").
 * @param out Utdatabuffer (t.ex. "21:30").
 * @param out_sz Storlek på målbuffer.
 */
void format_time_hh_mm(const char *raw, char *out, size_t out_sz) {
    char digits[16];
    normalize_time_adif(raw, digits, sizeof(digits));
    if (strlen(digits) >= 4) {
        snprintf(out, out_sz, "%.2s:%.2s", digits, digits + 2);
    } else {
        copy_str(out, out_sz, raw ? raw : "");
    }
}

/**
 * @brief Tolkar tidszon-offset från inställningar (t.ex. "+0100", "+02:00", "+1", "-0500", "UTC+2").
 * @param tz_str Tidszonsträng.
 * @param offset_sec_out Pekare till variabel som tar emot offset i sekunder.
 * @return 1 om giltig offset hittades, annars 0.
 */
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

/**
 * @brief Genererar aktuella UTC-strängar för datum (YYYY-MM-DD), tid (HH:MM) och statusrad.
 * @param date_buf Buffer för datum.
 * @param date_sz Storlek på datumbuffert.
 * @param time_buf Buffer för tid.
 * @param time_sz Storlek på tidsbuffert.
 * @param top_buf Buffer för statusfält i överkant.
 * @param top_sz Storlek på statusbuffert.
 */
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
