/**
 * @file qrz.h
 * @brief Uppslagning av anropssignaler via QRZ.COM XML Data Service.
 */

#ifndef QRZ_H
#define QRZ_H

#include "qsolog.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Slår upp en anropssignal synkront via QRZ.COM XML-tjänst.
 * @param username QRZ.COM användarnamn.
 * @param password QRZ.COM lösenord.
 * @param call_in Anropssignal att söka efter.
 * @param name_out Buffer för operatörens förnamn.
 * @param name_sz Storlek på namnbuffer.
 * @param qth_out Buffer för ort/land.
 * @param qth_sz Storlek på QTH-buffer.
 * @param grid_out Buffer för Maidenhead-lokator.
 * @param grid_sz Storlek på lokatorbuffer.
 * @return 1 om information hämtades med framgång, annars 0.
 */
int qrz_lookup(const char *username, const char *password, const char *call_in,
               char *name_out, size_t name_sz,
               char *qth_out, size_t qth_sz,
               char *grid_out, size_t grid_sz);

/**
 * @brief Slår upp i intern minnescache för QRZ.COM.
 * @param call_in Anropssignal.
 * @param name_out Buffer för förnamn.
 * @param name_sz Storlek på namnbuffer.
 * @param qth_out Buffer för QTH.
 * @param qth_sz Storlek på QTH-buffer.
 * @param grid_out Buffer för lokator.
 * @param grid_sz Storlek på lokatorbuffer.
 * @param was_cached Sätts till 1 om signalen fanns i cachen.
 * @return 1 vid positiv träff i cachen, annars 0.
 */
int lookup_qrz_cache(const char *call_in,
                     char *name_out, size_t name_sz,
                     char *qth_out, size_t qth_sz,
                     char *grid_out, size_t grid_sz,
                     int *was_cached);

/**
 * @brief Sparar ett uppslagsresultat i den interna QRZ-cachen.
 * @param call_in Anropssignal.
 * @param name Operatörens förnamn.
 * @param qth Operatörens QTH.
 * @param grid Operatörens lokator.
 * @param found 1 om signalen gav träff, 0 om den saknades i QRZ.
 */
void store_qrz_cache(const char *call_in, const char *name, const char *qth, const char *grid, int found);

/**
 * @brief Startar asynkron uppslagning mot QRZ.COM i en bakgrundstråd.
 * @param username QRZ användarnamn.
 * @param password QRZ lösenord.
 * @param call_in Anropssignal att söka efter.
 */
void start_qrz_background_lookup(const char *username, const char *password, const char *call_in);

/**
 * @brief Kontrollerar om QRZ-bakgrundsuppslagningen har slutförts och uppdaterar formuläret.
 * @param qso Det aktiva QSO:t.
 * @param st Min stations inställningar.
 * @param active_field Det för tillfället aktiva formulärfältet.
 * @param cursor_pos Pekare till markörpositionen.
 * @param status_msg Buffer för statusmeddelande.
 * @param status_sz Storlek på statusbufferten.
 * @param current_qrb Buffer för QRB.
 * @param qrb_sz Storlek på QRB-bufferten.
 */
void poll_qrz_background_result(QSO *qso, const StationSettings *st,
                                int active_field, int *cursor_pos,
                                char *status_msg, size_t status_sz,
                                char *current_qrb, size_t qrb_sz);

/**
 * @brief Invaliderar cachad sessionsnyckel för QRZ.COM (t.ex. vid felaktig session eller credentials).
 */
void qrz_invalidate_session(void);

/**
 * @brief Avbryter pågående QRZ-bakgrundstrådar och städar upp resurser.
 */
void qrz_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* QRZ_H */
