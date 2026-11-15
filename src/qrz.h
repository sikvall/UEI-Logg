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

/**
 * @brief Bygger en ADIF-post för ett enskilt QSO anpassad för QRZ.COM Logbook API.
 * @param qso QSO som ska formateras.
 * @param st Stationsinställningar (för egen signal, lokator m.m.).
 * @param out Målbuffer för ADIF-strängen.
 * @param out_sz Storlek på målbuffer.
 */
void qrz_build_adif_record(const QSO *qso, const StationSettings *st, char *out, size_t out_sz);

/**
 * @brief Tolkar svaret från QRZ.COM Logbook API.
 * @param resp Rå svarssträng från API:et.
 * @param err_out Buffer för felmeddelande vid misslyckande.
 * @param err_sz Storlek på felmeddelandebuffert.
 * @return 1 vid framgång (RESULT=OK eller RESULT=REPLACE), 0 vid fel.
 */
int qrz_parse_logbook_response(const char *resp, char *err_out, size_t err_sz);

/**
 * @brief Skickar ett QSO direkt till QRZ.COM Logbook via dess HTTPS API (ACTION=INSERT).
 * @param api_key QRZ Logbook API-nyckel.
 * @param qso QSO som ska loggas.
 * @param st Stationsinställningar.
 * @param err_out Buffer för felmeddelande vid misslyckande.
 * @param err_sz Storlek på felmeddelandebuffert.
 * @return 1 vid framgång, 0 vid fel.
 */
int qrz_logbook_insert(const char *api_key, const QSO *qso, const StationSettings *st,
                       char *err_out, size_t err_sz);

#ifdef __cplusplus
}
#endif

#endif /* QRZ_H */
