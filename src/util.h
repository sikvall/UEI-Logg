/**
 * @file util.h
 * @brief Hjälpfunktioner för stränghantering, UTF-8, datum, tid, frekvenser och band.
 */

#ifndef UTIL_H
#define UTIL_H

#include "qsolog.h"
#include <curl/curl.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Buffer-struktur för dynamisk nedladdning via cURL.
 */
typedef struct {
    char *data;  /**< Pekare till allokerad textbuffer */
    size_t len;  /**< Aktuell längd i bytes */
} CurlBuf;

/**
 * @brief cURL skriv-callback som allokerar minne dynamiskt allt eftersom data laddas ned.
 * @param ptr Pekare till inkommande data.
 * @param size Storlek på dataelement.
 * @param nmemb Antal element.
 * @param userdata Pekare till mottagande CurlBuf-struktur.
 * @return Antal hanterade bytes.
 */
size_t curl_write_cb(void *ptr, size_t size, size_t nmemb, void *userdata);

/**
 * @brief Säker strängkopiering med garanterad null-terminering och bufferskydd.
 * @param dest Målbuffer.
 * @param dest_size Storlek på målbuffer i bytes.
 * @param src Källsträng (kan vara NULL).
 */
void copy_str(char *dest, size_t dest_size, const char *src);

/**
 * @brief Raderar avslutande radbrytningstecken (\r, \n) i en sträng.
 * @param str Sträng att rensa.
 */
void trim_newline(char *str);

/**
 * @brief Raderar inledande och avslutande blanksteg (mellanslag, tab, radbrytningar).
 * @param str Sträng att rensa.
 */
void trim_str(char *str);

/**
 * @brief Konverterar alla ASCII-tecken i en sträng till versaler (in-place).
 * @param str Sträng att konvertera.
 */
void to_upper_str(char *str);

/**
 * @brief Returnerar pekare till basnamnet i en sökväg (efter sista '/').
 * @param path Sökvägen.
 * @return Pekare till fil- eller mappnamnet.
 */
const char *get_path_basename(const char *path);

/**
 * @brief Enkel Base64-kodning för strängar.
 * @param in Indata.
 * @param out Utdatabuffer för Base64.
 * @param out_sz Storlek på utdatabuffert.
 */
void base64_encode_str(const char *in, char *out, size_t out_sz);

/**
 * @brief Beräknar den synliga kolumnbredden för en UTF-8-sträng i terminalen (wcwidth).
 * @param str UTF-8-sträng.
 * @return Antal synliga teckenkolumner.
 */
int utf8_display_width(const char *str);

/**
 * @brief Konverterar en teckenposition (antal unicode-tecken) till byte-offset i en UTF-8-sträng.
 * @param str UTF-8-sträng.
 * @param char_pos Teckenposition från start.
 * @return Byte-offset.
 */
size_t utf8_char_to_byte(const char *str, int char_pos);

/**
 * @brief Raderar tecknet omedelbart till vänster om markören i en UTF-8-sträng (backspace).
 * @param str Sträng som redigeras.
 * @param char_pos Pekare till markörposition (minskas med 1).
 */
void utf8_backspace_at(char *str, int *char_pos);

/**
 * @brief Raderar tecknet under markören i en UTF-8-sträng (delete).
 * @param str Sträng som redigeras.
 * @param char_pos Markörposition.
 */
void utf8_delete_at(char *str, int char_pos);

/**
 * @brief Infogar en byte/ett ASCII-tecken vid angiven markörposition i en UTF-8-sträng (insert).
 * @param str Strängen som redigeras.
 * @param max_size Maximal bufferstorlek inklusive null.
 * @param char_pos Pekare till markörposition (ökas med 1).
 * @param ch Tecknet som ska infogas.
 */
void utf8_insert_byte_at(char *str, size_t max_size, int *char_pos, char ch);

/**
 * @brief Skriver över tecknet vid angiven markörposition i en UTF-8-sträng med en byte/ett ASCII-tecken.
 * @param str Strängen som redigeras.
 * @param max_size Maximal bufferstorlek inklusive null.
 * @param char_pos Pekare till markörposition (ökas med 1).
 * @param ch Tecknet som ska skrivas.
 */
void utf8_overwrite_byte_at(char *str, size_t max_size, int *char_pos, char ch);

/**
 * @brief Skiftlägesokänslig sökning (case-insensitive substring check) för UTF-8-strängar.
 * @param haystack Strängen att söka i.
 * @param needle Delsträngen att söka efter.
 * @return 1 om delsträngen finns, annars 0.
 */
int utf8_contains_ci(const char *haystack, const char *needle);

/**
 * @brief Tolkar och normaliserar en frekvenssträng till MHz med 3 decimaler.
 * @param freq_str Inmatad frekvenssträng (t.ex. "14074" eller "14.074").
 * @param norm_out Buffer för normaliserad sträng ("14.074").
 * @param out_sz Storlek på målbuffer.
 * @return Frekvensen som flyttal i MHz, eller 0.0 vid ogiltig inmatning.
 */
double parse_and_normalize_freq(const char *freq_str, char *norm_out, size_t out_sz);

/**
 * @brief Tolkar och normaliserar frekvens vid ADIF-import till MHz med decimaler (t.ex. "7" -> "7.000").
 * Vid import antas frekvensen vara i MHz enligt ADIF-standarden.
 * @param in Inmatad frekvenssträng från ADIF.
 * @param norm_out Buffer för normaliserad sträng ("7.000").
 * @param out_sz Storlek på målbuffer.
 * @return Frekvensen som flyttal i MHz, eller 0.0 vid ogiltig inmatning.
 */
double parse_and_normalize_freq_adif(const char *in, char *norm_out, size_t out_sz);

/**
 * @brief Gissar amatörband utifrån ett flyttal i MHz.
 * @param f Frekvens i MHz.
 * @return Namn på bandet (t.ex. "20 m"), eller NULL om utanför amatörbanden.
 */
const char *guess_band_from_mhz(double f);

/**
 * @brief Gissar amatörband utifrån en frekvenssträng i MHz eller kHz.
 * @param freq_str Frekvenssträngen.
 * @return Namn på bandet (t.ex. "20 m"), eller NULL om utanför amatörbanden.
 */
const char *guess_band_from_freq(const char *freq_str);

/**
 * @brief Identifierar amatörbandet för en frekvens och sparar resultatet i band_out.
 * @param freq_str Frekvenssträngen.
 * @param band_out Buffer för identifierat band.
 * @param out_sz Storlek på bandbuffert.
 */
void resolve_band_from_freq(const char *freq_str, char *band_out, size_t out_sz);

/**
 * @brief Letar upp index i BAND_OPTIONS för ett givet bandnamn.
 * @param band_str Bandnamn (t.ex. "20 m" eller "20M").
 * @return Index (0..BAND_OPTION_COUNT-1), eller -1 om okänt.
 */
int find_band_index(const char *band_str);

/**
 * @brief Formaterar ett bandnamn för prydlig visning i gränssnittet (t.ex. "20m" -> "20 m").
 * @param in Rå bandsträng.
 * @param out Buffer för formaterad sträng.
 * @param out_sz Storlek på målbuffer.
 */
void format_band_display(const char *in, char *out, size_t out_sz);

/**
 * @brief Normaliserar ett bandnamn för standardiserad ADIF-lagring (t.ex. "20 m" -> "20m").
 * @param in Bandsträng.
 * @param out Buffer för normaliserat namn.
 * @param out_sz Storlek på målbuffer.
 */
void normalize_band_adif(const char *in, char *out, size_t out_sz);

/**
 * @brief Normaliserar ett datum till standard ADIF-format (YYYYMMDD).
 * @param in Datumsträng (t.ex. "2026-11-11").
 * @param out Buffer för ADIF-datum.
 * @param out_sz Storlek på målbuffer.
 */
void normalize_date_adif(const char *in, char *out, size_t out_sz);

/**
 * @brief Normaliserar en tidsstämpel till standard ADIF-format (HHMM eller HHMMSS).
 * @param in Tidssträng (t.ex. "14:25").
 * @param out Buffer för ADIF-tid.
 * @param out_sz Storlek på målbuffer.
 */
void normalize_time_adif(const char *in, char *out, size_t out_sz);

/**
 * @brief Formaterar ett rått datum till läsbart format YYYY-MM-DD.
 * @param raw Rått datum (t.ex. "20261111").
 * @param out Buffer för formaterat datum.
 * @param out_sz Storlek på målbuffer.
 */
void format_date_yyyy_mm_dd(const char *raw, char *out, size_t out_sz);

/**
 * @brief Formaterar en rå tidsstämpel till läsbart format HH:MM.
 * @param raw Rå tid (t.ex. "142500").
 * @param out Buffer för formaterad tid.
 * @param out_sz Storlek på målbuffer.
 */
void format_time_hh_mm(const char *raw, char *out, size_t out_sz);

/**
 * @brief Tolkar en tidszonssträng (t.ex. "UTC+1", "CET", "+02:00") till offset i sekunder.
 * @param tz_str Tidszonssträng.
 * @param offset_sec_out Utdatapekare för offset i sekunder från UTC.
 * @return 1 om zonen tolkades, annars 0.
 */
int parse_tz_offset_seconds(const char *tz_str, long *offset_sec_out);

/**
 * @brief Hämtar aktuella formaterade strängar för datum, UTC-tid och fönsterlist.
 * @param date_buf Buffer för aktuellt datum (YYYY-MM-DD).
 * @param date_sz Storlek på datumbuffert.
 * @param time_buf Buffer för aktuell UTC-tid (HH:MM).
 * @param time_sz Storlek på tidsbuffert.
 * @param top_buf Buffer för formaterad fönsterlist med datum, lokal tid och UTC (kan vara NULL).
 * @param top_sz Storlek på top_buf.
 */
void get_utc_strings(char *date_buf, size_t date_sz,
                     char *time_buf, size_t time_sz,
                     char *top_buf, size_t top_sz);

#ifdef __cplusplus
}
#endif

#endif /* UTIL_H */
