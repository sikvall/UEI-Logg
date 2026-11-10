#ifndef SETTINGS_H
#define SETTINGS_H

#include "qsolog.h"

/**
 * @file settings.h
 * @brief Stationsinställningar, ADIF import och export, fildialog samt inställningsmodalen.
 */

/* Läs in stationsinställningar från fil (station.conf) */
void load_station_settings(const char *filename, StationSettings *st);

/* Spara stationsinställningar till fil (station.conf) */
int save_station_settings(const char *filename, const StationSettings *st);

/* Slutbehandling av importerat QSO (formatering, DXCC, QRB, sätter handled = 0) */
void finalize_imported_qso(QSO *current, const StationSettings *st);

/* Importera QSO från extern ADIF-fil */
int import_qsos_from_adif(const char *filename, QSOList *list, const StationSettings *st);

/* Exportera QSO till extern ADIF-fil (alla, markerade eller endast nya) */
int export_qsos_adif(const char *filename, QSOList *list, const StationSettings *st, int export_mode);

/* Töm hela loggboken på disk och i minnet */
int clear_log_book(QSOList *log_list, const StationSettings *st);

/* Bygg standardiserat exportfilnamn (<signal>_<YYMMDD>_<HHMMSS>[_nya].adi) */
void build_default_export_filename(char *out, size_t out_sz, const StationSettings *st, int only_new);

/* Generisk fildialog för val eller inmatning av filnamn */
int run_file_dialog(const char *title, const char *start_dir, const char *ext_filter,
                    char *selected_path, size_t max_path_len,
                    int is_save_mode, const char *default_filename);

/* Visa och hantera inställningsskärmen (F12) */
void run_settings_modal(StationSettings *st, QSO *qso, int editing_index,
                        QSOList *log_list, int *selected_log_pos, int *log_scroll_offset,
                        char *status_msg, size_t status_sz);

#endif /* SETTINGS_H */
