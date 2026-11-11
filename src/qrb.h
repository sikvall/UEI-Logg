/**
 * @file qrb.h
 * @brief Maidenhead-lokatorer, geokodning (OSM) och avståndsberäkning (QRB).
 */

#ifndef QRB_H
#define QRB_H

#include "qsolog.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Kontrollerar om en sträng är en giltig Maidenhead-lokator (4 eller 6 tecken).
 * @param loc Sträng att validera (t.ex. "JO89VJ").
 * @return 4 eller 6 vid giltig lokator, annars 0.
 */
int is_valid_locator(const char *loc);

/**
 * @brief Konverterar en Maidenhead-lokator till geografisk latitud och longitud (grader).
 * @param loc Lokatorsträng.
 * @param lat_out Utdatapekare för latitud i grader.
 * @param lon_out Utdatapekare för longitud i grader.
 * @return 1 vid framgångsrik konvertering, annars 0.
 */
int locator_to_latlon(const char *loc, double *lat_out, double *lon_out);

/**
 * @brief Beräknar storcirkelavstånd enligt haversine-formeln mellan två koordinater.
 * @param lat1 Latitud för punkt 1.
 * @param lon1 Longitud för punkt 1.
 * @param lat2 Latitud för punkt 2.
 * @param lon2 Longitud för punkt 2.
 * @return Avstånd i kilometer.
 */
double calculate_distance_km(double lat1, double lon1, double lat2, double lon2);

/**
 * @brief Slår upp koordinater för en ort (QTH) via OpenStreetMap Nominatim API.
 * @param qth Ortnamn (t.ex. "Linköping").
 * @param lat_out Utdatapekare för latitud.
 * @param lon_out Utdatapekare för longitud.
 * @return 1 om koordinater hittades, annars 0.
 */
int geocode_qth_osm(const char *qth, double *lat_out, double *lon_out);

/**
 * @brief Beräknar QRB (avstånd i kilometer) mellan min station och motstationen.
 * Kontrollerar först motstationens lokator, och därefter motstationens QTH via geokodning.
 * @param my_st Min stations inställningar (innehåller egen lokator).
 * @param target_qso Motstationens QSO-post (innehåller lokator och/eller QTH).
 * @param out Buffer där avståndssträngen skrivs (t.ex. "142 km" eller "-").
 * @param out_sz Storlek på målbuffer.
 */
void compute_qrb_str(const StationSettings *my_st, const QSO *target_qso, char *out, size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif /* QRB_H */
