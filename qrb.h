#ifndef QRB_H
#define QRB_H

#include "qsolog.h"

/**
 * @file qrb.h
 * @brief Maidenhead-lokatorer, geokodning (OSM) och avståndsberäkning (QRB).
 */

/* Kontrollera om en sträng är en giltig Maidenhead-lokator (4 eller 6 tecken) */
int is_valid_locator(const char *loc);

/* Konvertera Maidenhead-lokator till latitud och longitud (grader) */
int locator_to_latlon(const char *loc, double *lat_out, double *lon_out);

/* Beräkna storcirkelavstånd (haversine) i kilometer mellan två koordinater */
double calculate_distance_km(double lat1, double lon1, double lat2, double lon2);

/* Slå upp koordinater för en ort (QTH) via OpenStreetMap Nominatim */
int geocode_qth_osm(const char *qth, double *lat_out, double *lon_out);

/* Beräkna QRB (avstånd i km) mellan egen station och motstationen */
void compute_qrb_str(const StationSettings *my_st, const QSO *target_qso, char *out, size_t out_sz);

#endif /* QRB_H */
