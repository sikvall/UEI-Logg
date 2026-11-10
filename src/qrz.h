#ifndef QRZ_H
#define QRZ_H

#include "qsolog.h"

/**
 * @file qrz.h
 * @brief Uppslagning av anropssignaler via QRZ.COM XML Data Service.
 */

/* Slå upp anropssignal synkront via QRZ.COM */
int qrz_lookup(const char *username, const char *password, const char *call_in,
               char *name_out, size_t name_sz,
               char *qth_out, size_t qth_sz,
               char *grid_out, size_t grid_sz);

/* Slå upp i intern minnescache för QRZ */
int lookup_qrz_cache(const char *call_in,
                     char *name_out, size_t name_sz,
                     char *qth_out, size_t qth_sz,
                     char *grid_out, size_t grid_sz,
                     int *was_cached);

/* Spara resultat i QRZ-cachen */
void store_qrz_cache(const char *call_in, const char *name, const char *qth, const char *grid, int found);

/* Starta asynkron uppslagning mot QRZ.COM i bakgrundstråd */
void start_qrz_background_lookup(const char *username, const char *password, const char *call_in);

/* Kontrollera om QRZ-bakgrundsuppslagningen har blivit klar och uppdatera formuläret */
void poll_qrz_background_result(QSO *qso, const StationSettings *st,
                                int active_field, int *cursor_pos,
                                char *status_msg, size_t status_sz,
                                char *current_qrb, size_t qrb_sz);

/* Invalidera cachad sessionsnyckel (t.ex. vid felaktiga inloggningsuppgifter eller session timeout) */
void qrz_invalidate_session(void);

/* Avbryt bakgrundstrådar och städa upp */
void qrz_cleanup(void);

#endif /* QRZ_H */
