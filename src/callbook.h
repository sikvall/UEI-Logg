/**
 * @file callbook.h
 * @brief Uppslagning av svenska amatörradiosignaler via SM Callbook (SSA).
 */

#ifndef CALLBOOK_H
#define CALLBOOK_H

#include "qsolog.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Kontrollerar om en signal är svensk (SA-SM, 7S, 8S) och extraherar bassignal.
 * @param call_in Inkommande signal (t.ex. "SM5UEI/P").
 * @param base_out Buffer för extraherad bassignal ("SM5UEI").
 * @param out_sz Storlek på målbuffer.
 * @return 1 om svensk anropssignal, annars 0.
 */
int extract_swedish_base_call(const char *call_in, char *base_out, size_t out_sz);

/**
 * @brief Utför synkront uppslag mot SM Callbook (hanterar även extrasignaler).
 * @param call_in Anropssignal att söka efter.
 * @param name_out Buffer för operatörens förnamn.
 * @param name_sz Storlek på namnbuffer.
 * @param qth_out Buffer för operatörens QTH/ort.
 * @param qth_sz Storlek på QTH-buffer.
 * @return 1 om träff erhölls, annars 0.
 */
int lookup_sm_callbook(const char *call_in, char *name_out, size_t name_sz, char *qth_out, size_t qth_sz);

/**
 * @brief Slår upp i intern minnescache för SM Callbook.
 * @param call_in Anropssignal att söka efter.
 * @param name_out Buffer för förnamn vid träff.
 * @param name_sz Storlek på namnbuffer.
 * @param qth_out Buffer för QTH vid träff.
 * @param qth_sz Storlek på QTH-buffer.
 * @param was_cached Sätts till 1 om signalen fanns i cachen (oavsett träff eller ej).
 * @return 1 om positiv träff fanns i cachen, annars 0.
 */
int lookup_smcb_cache(const char *call_in, char *name_out, size_t name_sz, char *qth_out, size_t qth_sz, int *was_cached);

/**
 * @brief Sparar ett uppslagsresultat i intern minnescache.
 * @param call_in Anropssignal.
 * @param name Operatörens förnamn vid träff.
 * @param qth Operatörens QTH vid träff.
 * @param found 1 om signalen gav träff, 0 om signalen saknades.
 */
void store_smcb_cache(const char *call_in, const char *name, const char *qth, int found);

/**
 * @brief Startar asynkron uppslagning i bakgrundstråd mot SM Callbook.
 * @param base_call Bassignal att slå upp.
 */
void start_smcb_background_lookup(const char *base_call);

/**
 * @brief Kontrollerar om bakgrundsuppslagningen har blivit klar och uppdaterar formuläret.
 * @param qso Det aktiva QSO:t.
 * @param st Min stations inställningar.
 * @param active_field Det för tillfället aktiva inmatningsfältet.
 * @param cursor_pos Pekare till markörpositionen.
 * @param status_msg Buffer för statusmeddelande.
 * @param status_sz Storlek på statusbufferten.
 * @param current_qrb Buffer för QRB-avstånd.
 * @param qrb_sz Storlek på QRB-bufferten.
 */
void poll_smcb_background_result(QSO *qso, const StationSettings *st,
                                 int active_field, int *cursor_pos,
                                 char *status_msg, size_t status_sz,
                                 char *current_qrb, size_t qrb_sz);

/**
 * @brief Avbryter pågående bakgrundstrådar och frigör resurser för SM Callbook.
 */
void callbook_cleanup(void);

#ifdef __cplusplus
}
#endif

#endif /* CALLBOOK_H */
