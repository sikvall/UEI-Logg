#ifndef UTIL_H
#define UTIL_H

#include "qsolog.h"
#include <curl/curl.h>

/**
 * @file util.h
 * @brief Hjälpfunktioner för stränghantering, UTF-8, datum, tid, frekvenser och band.
 */

/* Buffer-struktur för cURL HTTP-svar */
typedef struct {
    char *data;
    size_t len;
} CurlBuf;

/* cURL skriv-callback som allokerar minne dynamiskt */
size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata);

/* Stränghantering */
void copy_str(char *dest, size_t dest_size, const char *src);
void trim_newline(char *str);
void trim_str(char *str);
void to_upper_str(char *str);
const char *get_path_basename(const char *path);
void base64_encode_str(const char *in, char *out, size_t out_sz);

/* UTF-8 beräkning och redigering */
int utf8_display_width(const char *str);
size_t utf8_char_to_byte(const char *str, int char_pos);
void utf8_backspace_at(char *str, int *char_pos);
void utf8_delete_at(char *str, int char_pos);
void utf8_insert_byte_at(char *str, size_t max_size, int *char_pos, char ch);
int utf8_contains_ci(const char *haystack, const char *needle);

/* Frekvens & Band */
double parse_and_normalize_freq(const char *freq_str, char *norm_out, size_t out_sz);
const char *guess_band_from_freq(const char *freq_str);
void resolve_band_from_freq(const char *freq_str, char *band_out, size_t out_sz);
int find_band_index(const char *band_str);
void format_band_display(const char *in, char *out, size_t out_sz);
void normalize_band_adif(const char *in, char *out, size_t out_sz);

/* Datum & Tid */
void normalize_date_adif(const char *in, char *out, size_t out_sz);
void normalize_time_adif(const char *in, char *out, size_t out_sz);
void format_date_yyyy_mm_dd(const char *raw, char *out, size_t out_sz);
void format_time_hh_mm(const char *raw, char *out, size_t out_sz);
int parse_tz_offset_seconds(const char *tz_str, long *offset_sec_out);
void get_utc_strings(char *date_buf, size_t date_sz,
                     char *time_buf, size_t time_sz,
                     char *top_buf, size_t top_sz);

#endif /* UTIL_H */
