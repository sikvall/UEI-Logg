#ifndef CALLBOOK_H
#define CALLBOOK_H

#include "qsolog.h"

/**
 * @file callbook.h
 * @brief Uppslagning av svenska amatörradiosignaler via SM Callbook (SSA).
 */

/* Kontrollera om signalen är svensk (SA-SM, 7S, 8S) och extrahera bassignal */
int extract_swedish_base_call(const char *call_in, char *base_out, size_t out_sz);

/* Slå upp svensk anropssignal via SM Callbook (inklusive extrasignalhantering) */
int lookup_sm_callbook(const char *call_in, char *name_out, size_t name_sz, char *qth_out, size_t qth_sz);

/* Slå upp i intern minnescache för SM Callbook */
int lookup_smcb_cache(const char *call_in, char *name_out, size_t name_sz, char *qth_out, size_t qth_sz, int *was_cached);

/* Spara resultat i intern minnescache */
void store_smcb_cache(const char *call_in, const char *name, const char *qth, int found);

/* Starta asynkron uppslagning i bakgrundstråd */
void start_smcb_background_lookup(const char *base_call);

/* Kontrollera om bakgrundsuppslagningen har blivit klar och uppdatera formuläret */
void poll_smcb_background_result(QSO *qso, const StationSettings *st,
                                 int active_field, int *cursor_pos,
                                 char *status_msg, size_t status_sz,
                                 char *current_qrb, size_t qrb_sz);

/* Avbryt bakgrundstrådar och städa upp */
void callbook_cleanup(void);

#endif /* CALLBOOK_H */
