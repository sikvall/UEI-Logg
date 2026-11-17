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
 * @brief Importerar och slår samman ADIF-poster från en minnesbuffert till logglistan.
 * Kontrollerar dubbletter, beräknar QRB/DXCC, sparar i databasen och sorterar kronologiskt.
 * @param content Pekare till minnesbuffert innehållande ADIF-data.
 * @param content_len Storlek på bufferten.
 * @param list Logglistan där nya QSO:n läggs till.
 * @param st Min stations inställningar.
 * @param mark_as_handled 1 om de importerade QSO:na ska markeras som hanterade (handled=1), 0 för nya.
 * @param progress_title Titel för förloppsrutan (eller NULL för tyst läge utan UI-uppritning).
 * @return Antal importerade QSO:n, eller -1 vid fel.
 */
int import_qsos_from_memory(const char *content, size_t content_len, QSOList *list,
                            const StationSettings *st, int mark_as_handled,
                            const char *progress_title);

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
 * @brief Exporterar QSO:n till en extern CSV-fil (alla, markerade eller endast nya).
 * Första raden innehåller fältnamn i samma ordning som loggraderna.
 * @param filename Sökväg till målfilen.
 * @param list Logglistan att exportera ifrån.
 * @param st Min stations inställningar.
 * @param export_mode Exportläge (EXPORT_MODE_ALL, EXPORT_MODE_MARKED, EXPORT_MODE_ONLY_NEW).
 * @return Antal exporterade QSO:n, eller -1 vid fel.
 */
int export_qsos_csv(const char *filename, QSOList *list, const StationSettings *st, int export_mode);

/**
 * @brief Synkroniserar alla ej hanterade QSO (handled == 0) direkt mot QRZ.COM Logbook API.
 * Lyckade poster markeras som hanterade (handled = 1) i minnet och SQLite/ADIF.
 * @param log_list Logglistan med QSO.
 * @param st Stationsinställningar (innehåller QRZ API-nyckel).
 * @param status_msg Buffer för statusmeddelande.
 * @param status_sz Storlek på statusmeddelandebuffert.
 * @return Antal framgångsrikt synkade QSO, 0 om inga behövde synkas, eller negativt vid fel/saknad nyckel.
 */
int sync_unhandled_qsos_to_qrz(QSOList *log_list, const StationSettings *st,
                               char *status_msg, size_t status_sz);

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
 * @brief Skapar ett standardiserat CSV-exportfilnamn i formatet <signal>_<YYMMDD>_<HHMMSS>[_nya].csv.
 * @param out Buffer där filnamnet skrivs.
 * @param out_sz Storlek på målbuffer.
 * @param st Min stations inställningar.
 * @param only_new 1 om endast nya QSO:n exporteras, annars 0.
 */
void build_default_export_csv_filename(char *out, size_t out_sz, const StationSettings *st, int only_new);

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
void run_settings_modal(StationSettings *st, QSO *qso, int *editing_index,
                        QSOList *log_list, int *selected_log_pos, int *log_scroll_offset,
                        char *status_msg, size_t status_sz);

#ifdef __cplusplus
}
#endif

#endif /* SETTINGS_H */
