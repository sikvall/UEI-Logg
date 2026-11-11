/**
 * @file settings.h
 * @brief Stationsinställningar, ADIF-import och -export samt inställningsmodalen (F12).
 */

#ifndef SETTINGS_H
#define SETTINGS_H

#include "qsolog.h"
#include "file_dialog.h"
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Läser in stationsinställningar från konfigurationsfil (station.conf).
 * @param filename Sökväg till konfigurationsfilen.
 * @param st Struktur som fylls med inställningar.
 */
void load_station_settings(const char *filename, StationSettings *st);

/**
 * @brief Sparar stationsinställningar till konfigurationsfil (station.conf).
 * @param filename Sökväg till konfigurationsfilen.
 * @param st Inställningar att spara.
 * @return 1 vid framgång, 0 vid skrivfel.
 */
int save_station_settings(const char *filename, const StationSettings *st);

/**
 * @brief Slutbehandlar ett importerat QSO: formaterar fält, sätter handled=0 (ny) och beräknar QRB/DXCC.
 * @param current Det importerade QSO:t.
 * @param st Min stations inställningar.
 */
void finalize_imported_qso(QSO *current, const StationSettings *st);

/**
 * @brief Importerar QSO:n från en extern ADIF-fil till logglistan med dubblettkontroll.
 * @param filename Sökväg till ADIF-filen som ska importeras.
 * @param list Logglistan där nya QSO:n läggs till.
 * @param st Min stations inställningar.
 * @return Antal importerade QSO:n, eller -1 vid läsfel.
 */
int import_qsos_from_adif(const char *filename, QSOList *list, const StationSettings *st);

/**
 * @brief Exporterar QSO:n till en extern ADIF-fil (alla, markerade eller endast nya).
 * @param filename Sökväg till målfilen.
 * @param list Logglistan att exportera ifrån.
 * @param st Min stations inställningar.
 * @param export_mode Exportläge (EXPORT_MODE_ALL, EXPORT_MODE_MARKED, EXPORT_MODE_ONLY_NEW).
 * @return Antal exporterade QSO:n, eller -1 vid fel.
 */
int export_qsos_adif(const char *filename, QSOList *list, const StationSettings *st, int export_mode);

/**
 * @brief Tömmer hela loggboken på disk och i minnet.
 * @param log_list Logglistan som rensas.
 * @param st Min stations inställningar.
 * @return 1 vid framgång, 0 vid fel.
 */
int clear_log_book(QSOList *log_list, const StationSettings *st);

/**
 * @brief Skapar ett standardiserat ADIF-exportfilnamn i formatet <signal>_<YYMMDD>_<HHMMSS>[_nya].adi.
 * @param out Buffer där filnamnet skrivs.
 * @param out_sz Storlek på målbuffer.
 * @param st Min stations inställningar.
 * @param only_new 1 om endast nya QSO:n exporteras, annars 0.
 */
void build_default_export_filename(char *out, size_t out_sz, const StationSettings *st, int only_new);

/**
 * @brief Visar och hanterar inställningsdialogen (F12).
 * @param st Min stations inställningar.
 * @param qso Aktuellt QSO i formuläret.
 * @param editing_index Pågående redigeringsindex (-1 vid nytt QSO).
 * @param log_list Loggbokslistan.
 * @param selected_log_pos Pekare till vald rad i loggen.
 * @param log_scroll_offset Pekare till loggens rullningsposition.
 * @param status_msg Buffer för statusmeddelande.
 * @param status_sz Storlek på statusbufferten.
 */
void run_settings_modal(StationSettings *st, QSO *qso, int editing_index,
                        QSOList *log_list, int *selected_log_pos, int *log_scroll_offset,
                        char *status_msg, size_t status_sz);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_H */
