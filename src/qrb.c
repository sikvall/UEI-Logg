/**
 * @file qrb.c
 * @brief Implementation av Maidenhead-lokatorberäkning, OpenStreetMap-geokodning och QRB-avstånd.
 */

#include "qrb.h"
#include "util.h"
#include <ctype.h>
#include <curl/curl.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

/* Cache för QTH -> geografiska koordinater så att inte onödiga HTTP-anrop görs */
typedef struct {
    char qth[64];
    double lat;
    double lon;
    int valid;
} QthCoordCache;

static QthCoordCache g_qth_cache[64];
static size_t g_qth_cache_count = 0;

/**
 * @brief Kontrollerar om en sträng är en giltig och komplett Maidenhead-lokator (4 eller 6 tecken).
 * Exempel: "JO89" (4 tecken) eller "JO89xi" (6 tecken).
 * @param loc Lokatorsträng.
 * @return 4 eller 6 vid giltig lokator, annars 0.
 */
int is_valid_locator(const char *loc) {
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

/**
 * @brief Konverterar Maidenhead-lokator (4 eller 6 tecken) till latitud och longitud i decimalgrader.
 * Punkten placeras i centrum av rutan.
 * @param loc Lokatorsträng.
 * @param lat_out Pekare till variabel för latitud.
 * @param lon_out Pekare till variabel för longitud.
 * @return 1 om giltig lokator, annars 0.
 */
int locator_to_latlon(const char *loc, double *lat_out, double *lon_out) {
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

/**
 * @brief Beräknar storcirkelavstånd (haversine-formeln) i kilometer mellan två geografiska punkter.
 * @param lat1 Latitud punkt 1 (grader).
 * @param lon1 Longitud punkt 1 (grader).
 * @param lat2 Latitud punkt 2 (grader).
 * @param lon2 Longitud punkt 2 (grader).
 * @return Avståndet i kilometer.
 */
double calculate_distance_km(double lat1, double lon1, double lat2, double lon2) {
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

/**
 * @brief Slår upp koordinater i minnescachen för orter (QTH).
 * @param qth Ortens namn.
 * @param lat Pekare för latitud.
 * @param lon Pekare för longitud.
 * @return 1 om träff, 0 om känd ogiltig, -1 om saknas i cachen.
 */
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
    return -1;
}

/**
 * @brief Sparar en orts koordinater i minnescachen.
 * @param qth Ortens namn.
 * @param lat Latitud.
 * @param lon Longitud.
 * @param valid 1 om giltiga koordinater, 0 om ingen träff.
 */
static void store_qth_cache(const char *qth, double lat, double lon, int valid) {
    if (g_qth_cache_count < sizeof(g_qth_cache) / sizeof(g_qth_cache[0])) {
        copy_str(g_qth_cache[g_qth_cache_count].qth, sizeof(g_qth_cache[0].qth), qth);
        g_qth_cache[g_qth_cache_count].lat = lat;
        g_qth_cache[g_qth_cache_count].lon = lon;
        g_qth_cache[g_qth_cache_count].valid = valid;
        g_qth_cache_count++;
    }
}

/**
 * @brief Slår upp koordinater för en ort (QTH) via OpenStreetMap Nominatim API.
 * @param qth Ortens namn.
 * @param lat_out Pekare till målvariabel för latitud.
 * @param lon_out Pekare till målvariabel för longitud.
 * @return 1 om koordinater hittades, annars 0.
 */
int geocode_qth_osm(const char *qth, double *lat_out, double *lon_out) {
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

/**
 * @brief Beräknar QRB (avstånd i km) mellan egen station och motstationen.
 * 1. Använder i första hand Maidenhead-lokator om motstationen har komplett lokator (4 el. 6 tecken).
 * 2. Om lokator saknas och QTH finns, geokodas motstationens ort via OpenStreetMap.
 * @param my_st Min stations inställningar med lokator och/eller QTH.
 * @param target_qso Motstationens QSO-data med lokator och/eller QTH.
 * @param out Buffer för formaterat avstånd (t.ex. "342 km" eller "-").
 * @param out_sz Storlek på utdatabuffert.
 */
void compute_qrb_str(const StationSettings *my_st, const QSO *target_qso, char *out, size_t out_sz) {
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
