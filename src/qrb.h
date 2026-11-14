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
 * @brief Beräknar storcirkelbäring (0-359 grader) från punkt 1 till punkt 2.
 * @param lat1 Latitud för punkt 1.
 * @param lon1 Longitud för punkt 1.
 * @param lat2 Latitud för punkt 2.
 * @param lon2 Longitud för punkt 2.
 * @return Bäring i grader (0.0 till 359.9).
 */
double calculate_bearing_deg(double lat1, double lon1, double lat2, double lon2);

/**
 * @brief Slår upp koordinater för en ort (QTH) via OpenStreetMap Nominatim API.
 * @param qth Ortnamn (t.ex. "Linköping").
 * @param lat_out Utdatapekare för latitud.
 * @param lon_out Utdatapekare för longitud.
 * @return 1 om koordinater hittades, annars 0.
 */
int geocode_qth_osm(const char *qth, double *lat_out, double *lon_out);

/**
 * @brief Rensar cachen för en specifik ort (eller hela QTH-cachen om qth är NULL).
 * @param qth Ortnamn att ta bort ur cachen, eller NULL för att tömma hela cachen.
 */
void invalidate_qth_cache(const char *qth);

/**
 * @brief Beräknar QRB (avstånd i kilometer) mellan min station och motstationen.
 * Kontrollerar först motstationens lokator, och därefter motstationens QTH via geokodning.
 * @param my_st Min stations inställningar (innehåller egen lokator).
 * @param target_qso Motstationens QSO-post (innehåller lokator och/eller QTH).
 * @param out Buffer där avståndssträngen skrivs (t.ex. "142 km" eller "-").
 * @param out_sz Storlek på målbuffer.
 */
void compute_qrb_str(const StationSettings *my_st, const QSO *target_qso, char *out, size_t out_sz);

/**
 * @brief Beräknar både QRB (avstånd i km) och QUJ (bäring i grader) mellan min station och motstationen.
 * @param my_st Min stations inställningar.
 * @param target_qso Motstationens QSO-post.
 * @param qrb_out Buffer för formaterat avstånd (t.ex. "342 km" eller "-"), eller NULL.
 * @param qrb_sz Storlek på qrb_out.
 * @param quj_out Buffer för formaterad bäring (t.ex. "145°" eller "-"), eller NULL.
 * @param quj_sz Storlek på quj_out.
 */
void compute_qrb_quj_str(const StationSettings *my_st, const QSO *target_qso,
                         char *qrb_out, size_t qrb_sz,
                         char *quj_out, size_t quj_sz);

/**
 * @brief Beräknar QUJ (bäring i grader) mellan min station och motstationen.
 * @param my_st Min stations inställningar.
 * @param target_qso Motstationens QSO-post.
 * @param out Buffer där bäringen skrivs (t.ex. "145°" eller "-").
 * @param out_sz Storlek på målbuffer.
 */
void compute_quj_str(const StationSettings *my_st, const QSO *target_qso, char *out, size_t out_sz);

/**
 * @brief Formaterar bäring för visning i loggtabellens QUJ-kolumn (t.ex. "145°" eller "-").
 * @param my_st Min stations inställningar.
 * @param qso QSO-post att formatera QUJ för.
 * @param out Målbuffer.
 * @param out_sz Storlek på målbuffer.
 */
void format_quj_display(const StationSettings *my_st, const QSO *qso, char *out, size_t out_sz);

/**
 * @brief Uppdaterar QRB för ett enskilt QSO om det kan beräknas, och sparar till databasen om det ändrats.
 * @param my_st Min stations inställningar (innehåller egen lokator).
 * @param target_qso Motstationens QSO-post.
 * @return 1 om QRB beräknades och sparades, annars 0.
 */
int update_qso_qrb(const StationSettings *my_st, QSO *target_qso);

/**
 * @brief Går igenom en logglista och beräknar saknade QRB-värden samt sparar dem till databasen.
 * @param my_st Min stations inställningar.
 * @param list Logglista vars poster kontrolleras och uppdateras.
 * @return Antal uppdaterade poster.
 */
int update_all_missing_qrbs(const StationSettings *my_st, QSOList *list);

/**
 * @brief Formaterar avstånd för visning i loggtabellens QRB-kolumn (t.ex. "344 km" eller "-").
 * @param qso QSO-post att formatera QRB för.
 * @param out Målbuffer.
 * @param out_sz Storlek på målbuffer.
 */
void format_qrb_display(const QSO *qso, char *out, size_t out_sz);

#ifdef __cplusplus
}
#endif

#endif /* QRB_H */
