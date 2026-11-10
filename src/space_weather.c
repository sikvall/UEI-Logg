/**
 * @file space_weather.c
 * @brief Implementation av asynkron hämtning och tolkning av rymdvädersdata från https://hf.avitec.se.
 */

#include "space_weather.h"
#include "util.h"
#include <ctype.h>
#include <curl/curl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#define SW_URL "https://hf.avitec.se"
#define SW_REFRESH_INTERVAL_SEC 600 /* Uppdatera var 10:e minut */

static SpaceWeatherData g_sw_data;
static pthread_mutex_t g_sw_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t g_sw_cond   = PTHREAD_COND_INITIALIZER;
static pthread_t g_sw_thread;
static volatile int g_sw_running  = 0;
static volatile int g_sw_trigger  = 0;

/**
 * @brief Hjälpfunktion för att extrahera ett flyttalsvärde från en sträng (hanterar både komma och punkt).
 * @param str Strängen (t.ex. "11,6 MHz" eller "627 km/s").
 * @return Värdet som double, eller 0.0 vid fel.
 */
static double parse_first_double(const char *str) {
    if (str == NULL) return 0.0;
    while (*str != '\0' && !isdigit((unsigned char)*str) && *str != '-') {
        str++;
    }
    if (*str == '\0') return 0.0;

    char buf[32];
    size_t bj = 0;
    while (*str != '\0' && (isdigit((unsigned char)*str) || *str == '.' || *str == ',' || *str == '-') && bj < sizeof(buf) - 1) {
        buf[bj++] = (*str == ',') ? '.' : *str;
        str++;
    }
    buf[bj] = '\0';
    return atof(buf);
}

/**
 * @brief Extraherar kortets mätvärde och enhet från HTML-koden för ett givet kortnamn.
 * @param html Hela HTML-dokumentet från hf.avitec.se.
 * @param card_name Kortets titel, t.ex. "K-index", "MUF", "LUF", "CME", etc.
 * @param val_out Buffer för det formaterade värdet (t.ex. "11,6 MHz").
 * @param val_sz Storlek på utdatabuffert.
 * @return 1 om värde hittades, annars 0.
 */
static int extract_card_value(const char *html, const char *card_name, char *val_out, size_t val_sz) {
    if (html == NULL || card_name == NULL || val_out == NULL || val_sz == 0) return 0;
    val_out[0] = '\0';

    char target[64];
    snprintf(target, sizeof(target), ">%s</div>", card_name);
    const char *pos = strstr(html, target);
    if (pos == NULL) return 0;

    /* Sök bakåt upp till 250 tecken efter 'whitespace-nowrap">' */
    const char *search_start = (pos > html + 250) ? (pos - 250) : html;
    const char *val_tag = "whitespace-nowrap\">";
    const char *val_start = NULL;

    const char *scan = search_start;
    while (scan < pos) {
        const char *match = strstr(scan, val_tag);
        if (match != NULL && match < pos) {
            val_start = match + strlen(val_tag);
            scan = val_start;
        } else {
            break;
        }
    }

    if (val_start == NULL || val_start >= pos) return 0;

    /* Läs ut det primära värdet */
    char raw_val[32] = "";
    size_t vj = 0;
    const char *vp = val_start;
    while (vp < pos && *vp != '<' && vj < sizeof(raw_val) - 1) {
        raw_val[vj++] = *vp++;
    }
    raw_val[vj] = '\0';
    trim_str(raw_val);

    /* Kontrollera om en enhet finns inuti <span ...>UNIT</span> före pos */
    char raw_unit[32] = "";
    if (vp < pos && strncmp(vp, "<span", 5) == 0) {
        const char *span_close = strchr(vp, '>');
        if (span_close != NULL && span_close < pos) {
            const char *unit_start = span_close + 1;
            const char *span_end = strstr(unit_start, "</span>");
            if (span_end != NULL && span_end <= pos) {
                size_t uj = 0;
                while (unit_start < span_end && uj < sizeof(raw_unit) - 1) {
                    raw_unit[uj++] = *unit_start++;
                }
                raw_unit[uj] = '\0';
                trim_str(raw_unit);
            }
        }
    }

    if (raw_val[0] == '\0') return 0;

    if (raw_unit[0] != '\0') {
        if (strcmp(raw_unit, "%") == 0) {
            snprintf(val_out, val_sz, "%s %%", raw_val);
        } else {
            snprintf(val_out, val_sz, "%s %s", raw_val, raw_unit);
        }
    } else {
        copy_str(val_out, val_sz, raw_val);
    }

    return 1;
}

/**
 * @brief Tolkar hela HTML-svaret från hf.avitec.se och bygger upp en SpaceWeatherData-struktur med färgstatusar.
 * @param html Nedladdad HTML.
 * @param out Pekare till struktur som ska fyllas i.
 * @return 1 om minst K-index och MUF hittades, annars 0.
 */
static int parse_space_weather_html(const char *html, SpaceWeatherData *out) {
    if (html == NULL || out == NULL) return 0;

    SpaceWeatherData temp;
    memset(&temp, 0, sizeof(temp));

    /* 1. K-index Kiruna */
    if (extract_card_value(html, "K-index", temp.k_kiruna, sizeof(temp.k_kiruna))) {
        double k_val = parse_first_double(temp.k_kiruna);
        if (k_val <= 4.0)       temp.k_tone = SW_TONE_OK;
        else if (k_val <= 6.0)  temp.k_tone = SW_TONE_WARN;
        else if (k_val <= 8.0)  temp.k_tone = SW_TONE_BAD;
        else                    temp.k_tone = SW_TONE_SEVERE;
    } else {
        copy_str(temp.k_kiruna, sizeof(temp.k_kiruna), "-");
        temp.k_tone = SW_TONE_NORMAL;
    }

    /* 2. MUF */
    if (extract_card_value(html, "MUF", temp.muf, sizeof(temp.muf))) {
        double muf_val = parse_first_double(temp.muf);
        if (muf_val < 10.0)     temp.muf_tone = SW_TONE_WARN;
        else                    temp.muf_tone = SW_TONE_OK;
    } else {
        copy_str(temp.muf, sizeof(temp.muf), "-");
        temp.muf_tone = SW_TONE_NORMAL;
    }

    /* 3. LUF */
    if (extract_card_value(html, "LUF", temp.luf, sizeof(temp.luf))) {
        double luf_val = parse_first_double(temp.luf);
        if (luf_val <= 5.0)     temp.luf_tone = SW_TONE_OK;
        else if (luf_val <= 10) temp.luf_tone = SW_TONE_WARN;
        else                    temp.luf_tone = SW_TONE_BAD;
    } else {
        copy_str(temp.luf, sizeof(temp.luf), "-");
        temp.luf_tone = SW_TONE_NORMAL;
    }

    /* 4. CME */
    if (extract_card_value(html, "CME", temp.cme, sizeof(temp.cme))) {
        double cme_val = parse_first_double(temp.cme);
        if (cme_val <= 25.0)    temp.cme_tone = SW_TONE_OK;
        else if (cme_val <= 55) temp.cme_tone = SW_TONE_WARN;
        else if (cme_val <= 70) temp.cme_tone = SW_TONE_BAD;
        else                    temp.cme_tone = SW_TONE_SEVERE;
    } else {
        copy_str(temp.cme, sizeof(temp.cme), "-");
        temp.cme_tone = SW_TONE_NORMAL;
    }

    /* 5. Solfackla */
    if (extract_card_value(html, "Solfackla", temp.solfackla, sizeof(temp.solfackla))) {
        char cls = (char)toupper((unsigned char)temp.solfackla[0]);
        if (cls == 'A' || cls == 'B') temp.solfackla_tone = SW_TONE_OK;
        else if (cls == 'C')          temp.solfackla_tone = SW_TONE_WARN;
        else if (cls == 'M')          temp.solfackla_tone = SW_TONE_BAD;
        else if (cls == 'X')          temp.solfackla_tone = SW_TONE_SEVERE;
        else                          temp.solfackla_tone = SW_TONE_NORMAL;
    } else {
        copy_str(temp.solfackla, sizeof(temp.solfackla), "-");
        temp.solfackla_tone = SW_TONE_NORMAL;
    }

    /* 6. Solvind */
    if (extract_card_value(html, "Solvind", temp.solvind, sizeof(temp.solvind))) {
        double v_val = parse_first_double(temp.solvind);
        if (v_val < 450.0)      temp.solvind_tone = SW_TONE_OK;
        else if (v_val <= 600)  temp.solvind_tone = SW_TONE_WARN;
        else if (v_val <= 800)  temp.solvind_tone = SW_TONE_BAD;
        else                    temp.solvind_tone = SW_TONE_SEVERE;
    } else {
        copy_str(temp.solvind, sizeof(temp.solvind), "-");
        temp.solvind_tone = SW_TONE_NORMAL;
    }

    /* 7. Norrsken */
    if (extract_card_value(html, "Norrsken", temp.norrsken, sizeof(temp.norrsken))) {
        double n_val = parse_first_double(temp.norrsken);
        if (n_val < 20.0)       temp.norrsken_tone = SW_TONE_OK;
        else if (n_val <= 50.0) temp.norrsken_tone = SW_TONE_WARN;
        else if (n_val <= 200)  temp.norrsken_tone = SW_TONE_BAD;
        else                    temp.norrsken_tone = SW_TONE_SEVERE;
    } else {
        copy_str(temp.norrsken, sizeof(temp.norrsken), "-");
        temp.norrsken_tone = SW_TONE_NORMAL;
    }

    time_t now = time(NULL);
    struct tm *l = localtime(&now);
    if (l) {
        strftime(temp.updated_time, sizeof(temp.updated_time), "%H:%M", l);
    }
    temp.has_data = 1;

    *out = temp;
    return 1;
}

/**
 * @brief Hämtar data från https://hf.avitec.se via libcurl.
 * @param out Pekare till datastruktur.
 * @return 1 om lyckad hämtning och tolkning, annars 0.
 */
static int fetch_space_weather_online(SpaceWeatherData *out) {
    CURL *curl = curl_easy_init();
    if (curl == NULL) return 0;

    CurlBuf buf = {NULL, 0};
    curl_easy_setopt(curl, CURLOPT_URL, SW_URL);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "qsolog-uei/1.0 (amateur radio log; space weather reader)");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 6L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 4L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    int ok = 0;
    if (res == CURLE_OK && buf.data != NULL) {
        ok = parse_space_weather_html(buf.data, out);
    }
    free(buf.data);
    return ok;
}

/**
 * @brief Bakgrundstråd som periodiskt uppdaterar rymdvädersdata.
 * @param arg Oanvänd.
 * @return NULL.
 */
static void *space_weather_worker_thread(void *arg) {
    (void)arg;

    /* Första uppslagning direkt vid start */
    SpaceWeatherData fresh;
    if (fetch_space_weather_online(&fresh)) {
        pthread_mutex_lock(&g_sw_mutex);
        g_sw_data = fresh;
        pthread_mutex_unlock(&g_sw_mutex);
    }

    while (g_sw_running) {
        /* Sov i 10 minuter eller tills vi väcks via konditionsvariabel */
        pthread_mutex_lock(&g_sw_mutex);
        for (int i = 0; i < SW_REFRESH_INTERVAL_SEC && g_sw_running && !g_sw_trigger; ++i) {
            pthread_mutex_unlock(&g_sw_mutex);
            sleep(1);
            pthread_mutex_lock(&g_sw_mutex);
        }
        g_sw_trigger = 0;
        pthread_mutex_unlock(&g_sw_mutex);

        if (!g_sw_running) break;

        if (fetch_space_weather_online(&fresh)) {
            pthread_mutex_lock(&g_sw_mutex);
            g_sw_data = fresh;
            pthread_mutex_unlock(&g_sw_mutex);
        }
    }

    return NULL;
}

/**
 * @brief Initierar rymdvädersmodulen och startar bakgrundstråden.
 */
void space_weather_init(void) {
    pthread_mutex_lock(&g_sw_mutex);
    if (g_sw_running) {
        pthread_mutex_unlock(&g_sw_mutex);
        return;
    }
    memset(&g_sw_data, 0, sizeof(g_sw_data));
    copy_str(g_sw_data.k_kiruna, sizeof(g_sw_data.k_kiruna), "-");
    copy_str(g_sw_data.muf, sizeof(g_sw_data.muf), "-");
    copy_str(g_sw_data.luf, sizeof(g_sw_data.luf), "-");
    copy_str(g_sw_data.cme, sizeof(g_sw_data.cme), "-");
    copy_str(g_sw_data.solfackla, sizeof(g_sw_data.solfackla), "-");
    copy_str(g_sw_data.solvind, sizeof(g_sw_data.solvind), "-");
    copy_str(g_sw_data.norrsken, sizeof(g_sw_data.norrsken), "-");
    g_sw_running = 1;
    g_sw_trigger = 0;
    pthread_mutex_unlock(&g_sw_mutex);

    pthread_create(&g_sw_thread, NULL, space_weather_worker_thread, NULL);
}

/**
 * @brief Avbryter bakgrundstråden och städar upp vid programavslut.
 */
void space_weather_cleanup(void) {
    pthread_mutex_lock(&g_sw_mutex);
    if (!g_sw_running) {
        pthread_mutex_unlock(&g_sw_mutex);
        return;
    }
    g_sw_running = 0;
    pthread_cond_broadcast(&g_sw_cond);
    pthread_mutex_unlock(&g_sw_mutex);

    pthread_join(g_sw_thread, NULL);
}

/**
 * @brief Hämtar en trådsäker kopia av aktuell rymdvädersdata.
 * @param out Pekare till mottagande SpaceWeatherData-struktur.
 */
void space_weather_get_data(SpaceWeatherData *out) {
    if (out == NULL) return;
    pthread_mutex_lock(&g_sw_mutex);
    *out = g_sw_data;
    pthread_mutex_unlock(&g_sw_mutex);
}

/**
 * @brief Triggar en omedelbar asynkron uppdatering av rymdvädret.
 */
void space_weather_refresh_async(void) {
    pthread_mutex_lock(&g_sw_mutex);
    g_sw_trigger = 1;
    pthread_cond_broadcast(&g_sw_cond);
    pthread_mutex_unlock(&g_sw_mutex);
}
