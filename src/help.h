/**
 * @file help.h
 * @brief Interaktiv hjälpdialog med dokumentation av UEI-Loggs alla funktioner och kortkommandon.
 */

#ifndef HELP_H
#define HELP_H

#include "qsolog.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Visar och kör den interaktiva hjälpdialogen (F9).
 *
 * Innehåller flikuppdelad och detaljerad hjälp för:
 * 1. Globala tangenter & kortkommandon
 * 2. QSO-inmatning & navigering
 * 3. Uppslag (Lokal logg, SM Callbook, QRZ.COM)
 * 4. Loggrader & redigering / hantering
 * 5. Inställningar, ADIF-import/export & färgscheman
 * 6. Rymdväder & HF-konditioner
 *
 * @param st Min stations inställningar.
 */
void run_help_modal(const StationSettings *st);

#ifdef __cplusplus
}
#endif

#endif /* HELP_H */
