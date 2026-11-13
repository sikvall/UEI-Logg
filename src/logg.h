/**
 * @file logg.h
 * @brief Hantering av QSO-poster, loggrader, ADIF-filhantering och formulärvalidering.
 */

#ifndef LOGG_H
#define LOGG_H

#include "qsolog.h"
#include <stdio.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Sammanställd statistik för loggen.
 */
typedef struct {
    int total_qsos;       /**< Totalt antal loggade samtal */
    int unique_stations;  /**< Antal unika anropssignaler */
    int unique_dxcc;      /**< Antal unika DXCC-entiteter körda */
    int unique_grids;     /**< Antal unika lokatorrutor (4 tecken) */
    int unique_bandland;  /**< Antal unika kombinationer av DXCC och band */
    int rate_last_hour;   /**< Antal loggade QSO de senaste 60 minuterna */
} LogStats;

/**
 * @brief Beräknar statistik över loggen för presentationsrutan.
 * Uppdateras automatiskt varje minut för Rate och vid loggändringar.
 * @param list Pekare till logglistan.
 * @param stats Pekare till LogStats-struktur som fylls i.
 */
void compute_log_stats(const QSOList *list, LogStats *stats);

/**
 * @brief Invaliderar cachen för loggstatistik så att nästa anrop beräknar om direkt.
 */
void invalidate_log_stats_cache(void);

/**
 * @brief Lägger till ett QSO i slutet av QSO-listan (dynamisk array).
 * @param list Pekare till QSO-listan.
 * @param qso Det QSO som ska läggas till.
 */
void qso_list_push(QSOList *list, const QSO *qso);

/**
 * @brief Tar bort ett QSO vid ett specifikt index ur listan.
 * @param list Pekare till QSO-listan.
 * @param index Index för posten som ska raderas.
 */
void qso_list_delete_at(QSOList *list, size_t index);

/**
 * @brief Tar bort samtliga poster som är markerade (marked == 1) i QSO-listan.
 * @param list Pekare till QSO-listan.
 * @return Antal poster som raderades.
 */
int qso_list_delete_marked(QSOList *list);

/**
 * @brief Kontrollerar om ett QSO är en dubblett av ett redan loggat QSO (samma anrop, band och datum).
 * @param list Pekare till QSO-listan.
 * @param q Det QSO som kontrolleras.
 * @return 1 om dubblett föreligger, annars 0.
 */
int qso_is_duplicate(const QSOList *list, const QSO *q);

/**
 * @brief Jämförelsefunktion för qsort för att sortera QSO-poster kronologiskt (datum och tid).
 * @param a_ptr Pekare till första QSO-posten.
 * @param b_ptr Pekare till andra QSO-posten.
 * @return Negativt om a är tidigare än b, positivt om b är tidigare än a.
 */
int cmp_qso_chronological(const void *a_ptr, const void *b_ptr);

/**
 * @brief Läser in alla QSO-poster från en ADIF-fil till en QSO-lista.
 * @param filename Sökväg till ADIF-filen.
 * @param list Pekare till listan där posterna lagras.
 */
void load_qsos_from_adif(const char *filename, QSOList *list);

/**
 * @brief Sparar samtliga QSO-poster i listan till en ADIF-fil (skriver över filen).
 * @param filename Sökväg till ADIF-filen.
 * @param list Pekare till QSO-listan.
 * @param st Min stations inställningar.
 * @return 1 vid framgång, 0 vid skrivfel.
 */
int save_all_qsos_adif(const char *filename, const QSOList *list, const StationSettings *st);

/**
 * @brief Lägger till ett enskilt QSO i slutet av en befintlig ADIF-fil (append).
 * @param filename Sökväg till ADIF-filen.
 * @param qso Det QSO som ska sparas.
 * @param st Min stations inställningar.
 * @return 1 vid framgång, 0 vid skrivfel.
 */
int append_qso_adif(const char *filename, const QSO *qso, const StationSettings *st);

/**
 * @brief Skriver ett enskilt ADIF-fält (<TAG:LEN>VAL) till filen om värdet inte är tomt.
 * @param fp Filpekare.
 * @param tag ADIF-tagg (t.ex. "CALL").
 * @param val Fältets textvärde.
 */
void write_adif_field(FILE *fp, const char *tag, const char *val);

/**
 * @brief Skriver ADIF 3.1.8-huvud till en ny fil.
 * @param fp Filpekare.
 */
void write_adif_header(FILE *fp);

/**
 * @brief Skriver ett fullständigt QSO-rekord till en öppen ADIF-fil avslutad med <EOR>.
 * @param fp Filpekare.
 * @param qso Det QSO som ska skrivas.
 * @param st Min stations inställningar.
 */
void write_qso_record_adif(FILE *fp, const QSO *qso, const StationSettings *st);

/**
 * @brief Tolkar och tilldelar ett inläst ADIF-fält till motsvarande medlem i QSO-strukturen.
 * @param qso Mål-QSO.
 * @param tag ADIF-taggens namn.
 * @param val Fältets textinnehåll.
 */
void assign_adif_field(QSO *qso, const char *tag, const char *val);

/**
 * @brief Söker bakåt i loggboken efter en tidigare kontakt med samma anropssignal för att hämta namn, QTH och lokator.
 * @param log_list Logglistan att söka i.
 * @param call_in Anropssignal att söka efter.
 * @param name_out Buffer för namn.
 * @param name_sz Storlek på namnbuffer.
 * @param qth_out Buffer för QTH.
 * @param qth_sz Storlek på QTH-buffer.
 * @param grid_out Buffer för lokator.
 * @param grid_sz Storlek på lokatorbuffer.
 * @return 1 om information hittades i loggen, annars 0.
 */
int find_call_in_log(const QSOList *log_list, const char *call_in,
                     char *name_out, size_t name_sz,
                     char *qth_out, size_t qth_sz,
                     char *grid_out, size_t grid_sz);

/**
 * @brief Utför automatisk bakgrundsuppslagning vid inaktivitet eller fältbyte (Logg -> SMCB -> QRZ).
 * @param qso Aktuellt QSO.
 * @param log_list Loggbokslistan.
 * @param st Stationsinställningar.
 * @param last_smcb_call Senast efterfrågade signal (minskar dubbelanrop).
 * @param last_smcb_sz Storlek på last_smcb_call.
 * @param status_msg Buffer för statusmeddelande.
 * @param status_sz Storlek på statusbufferten.
 */
void maybe_lookup_call_details(QSO *qso, const QSOList *log_list,
                               const StationSettings *st,
                               char *last_smcb_call, size_t last_smcb_sz,
                               char *status_msg, size_t status_sz);

/**
 * @brief Sparar senast använda frekvens, band och mode till stationsinställningarna.
 * @param qso Aktuellt QSO.
 * @param st Stationsinställningar.
 */
void persist_qso_radio_settings(const QSO *qso, StationSettings *st);

/**
 * @brief Sätter valt band utifrån index i BAND_OPTIONS-tabellen.
 * @param qso Aktuellt QSO.
 * @param st Stationsinställningar.
 * @param idx Index i BAND_OPTIONS.
 */
void set_band_choice(QSO *qso, StationSettings *st, int idx);

/**
 * @brief Stegar bandvalet uppåt eller nedåt med delta steg.
 * @param qso Aktuellt QSO.
 * @param st Stationsinställningar.
 * @param delta Steg (-1 eller +1).
 */
void step_band_choice(QSO *qso, StationSettings *st, int delta);

/**
 * @brief Tillämpar standardvärden och utför kontroller när användaren lämnar ett specifikt fält.
 * @param qso Aktuellt QSO.
 * @param leaving_field Index för fältet som användaren lämnar.
 * @param log_list Logglistan.
 * @param st Stationsinställningar.
 * @param last_smcb_call Buffer för senast uppslagna signal.
 * @param last_smcb_sz Storlek på last_smcb_call.
 * @param status_msg Buffer för statusmeddelande.
 * @param status_sz Storlek på statusbufferten.
 * @param current_qrb Buffer för beräknat QRB.
 * @param qrb_sz Storlek på QRB-bufferten.
 */
void apply_field_defaults_on_leave(QSO *qso, int leaving_field,
                                   const QSOList *log_list,
                                   StationSettings *st,
                                   char *last_smcb_call, size_t last_smcb_sz,
                                   char *status_msg, size_t status_sz,
                                   char *current_qrb, size_t qrb_sz);

/**
 * @brief Validerar formuläret och sparar pågående QSO till loggboken (eller uppdaterar vid redigering).
 * @param qso Aktuellt QSO.
 * @param time_frozen Pekare till flagga för fryst tid.
 * @param active_field Pekare till aktivt fält.
 * @param field_cursor_pos Pekare till markörpositionen.
 * @param editing_index Pekare till redigeringsindex (-1 vid nytt QSO).
 * @param st Stationsinställningar.
 * @param log_list Logglistan.
 * @param last_smcb_call Buffer för senast uppslagna signal.
 * @param status_msg Buffer för statusmeddelande.
 * @param status_sz Storlek på statusbufferten.
 * @param current_qrb Buffer för QRB.
 */
void try_save_current_qso(QSO *qso, int *time_frozen, int *active_field,
                          int *field_cursor_pos, int *editing_index,
                          StationSettings *st, QSOList *log_list,
                          char *last_smcb_call,
                          char *status_msg, size_t status_sz,
                          char *current_qrb);

#ifdef __cplusplus
}
#endif

#endif /* LOGG_H */
