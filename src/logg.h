#ifndef LOGG_H
#define LOGG_H

#include "qsolog.h"
#include <stdio.h>

/**
 * @file logg.h
 * @brief Hantering av QSO-poster, loggrader, ADIF-filhantering och formulärvalidering.
 */

/* QSO-listfunktioner */
void qso_list_push(QSOList *list, const QSO *qso);
void qso_list_delete_at(QSOList *list, size_t index);
int qso_list_delete_marked(QSOList *list);
int qso_is_duplicate(const QSOList *list, const QSO *q);
int cmp_qso_chronological(const void *a_ptr, const void *b_ptr);

/* ADIF filhantering */
void load_qsos_from_adif(const char *filename, QSOList *list);
int save_all_qsos_adif(const char *filename, const QSOList *list, const StationSettings *st);
int append_qso_adif(const char *filename, const QSO *qso, const StationSettings *st);
void write_adif_field(FILE *fp, const char *tag, const char *val);
void write_adif_header(FILE *fp);
void write_qso_record_adif(FILE *fp, const QSO *qso, const StationSettings *st);
void assign_adif_field(QSO *qso, const char *tag, const char *val);

/* Loggsökning och autofyll */
int find_call_in_log(const QSOList *log_list, const char *call_in,
                     char *name_out, size_t name_sz,
                     char *qth_out, size_t qth_sz,
                     char *grid_out, size_t grid_sz);

void maybe_lookup_call_details(QSO *qso, const QSOList *log_list,
                               const StationSettings *st,
                               char *last_smcb_call, size_t last_smcb_sz,
                               char *status_msg, size_t status_sz);

/* Radio- och bandhantering */
void persist_qso_radio_settings(const QSO *qso, StationSettings *st);
void set_band_choice(QSO *qso, StationSettings *st, int idx);
void step_band_choice(QSO *qso, StationSettings *st, int delta);

void apply_field_defaults_on_leave(QSO *qso, int leaving_field,
                                   const QSOList *log_list,
                                   StationSettings *st,
                                   char *last_smcb_call, size_t last_smcb_sz,
                                   char *status_msg, size_t status_sz,
                                   char *current_qrb, size_t qrb_sz);

/* Spara/logga QSO */
void try_save_current_qso(QSO *qso, int *time_frozen, int *active_field,
                          int *field_cursor_pos, int *editing_index,
                          StationSettings *st, QSOList *log_list,
                          char *last_smcb_call,
                          char *status_msg, size_t status_sz,
                          char *current_qrb);

#endif /* LOGG_H */
