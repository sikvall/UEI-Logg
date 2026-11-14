/**
 * @file db.h
 * @brief SQLite3-databaslager för lokal logglagring i UEI-Logg (qsolog.sq3).
 */

#ifndef DB_H
#define DB_H

#include "qsolog.h"
#include <stdint.h>
#include <sqlite3.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Öppnar eller skapar SQLite3-databasen på angiven sökväg.
 * Initierar tabell 'qsos' och index samt sätter WAL-läge.
 * @param path Sökväg till databasfilen (t.ex. "qsolog.sq3").
 * @return 0 vid framgång, -1 vid fel.
 */
int db_open(const char *path);

/**
 * @brief Stänger den aktiva databasanslutningen.
 */
void db_close(void);

/**
 * @brief Kontrollerar om databasen är öppen och redo.
 * @return 1 om öppen, annars 0.
 */
int db_is_open(void);

/**
 * @brief Hämtar intern sqlite3-anslutningspekare.
 * @return Pekare till sqlite3, eller NULL om inte öppen.
 */
sqlite3 *db_get_handle(void);

/**
 * @brief Läser in samtliga loggade QSO:n från databasen till minneslistan (ordnade kronologiskt).
 * @param list Pekare till QSOList som fylls på.
 * @return Totalt antal inlästa poster, eller -1 vid databasfel.
 */
int db_load_all_qsos(QSOList *list);

/**
 * @brief Lägger till ett nytt QSO i databasen. Tilldelar qso->id det genererade rad-ID:t.
 * @param qso Pekare till QSO som ska sparas.
 * @return 0 vid framgång, -1 vid databasfel.
 */
int db_insert_qso(QSO *qso);

/**
 * @brief Uppdaterar ett befintligt QSO i databasen baserat på qso->id.
 * @param qso Pekare till det uppdaterade QSO:t.
 * @return 0 vid framgång, -1 vid fel eller om id saknas.
 */
int db_update_qso(const QSO *qso);

/**
 * @brief Raderar ett enskilt QSO ur databasen baserat på dess ID.
 * @param id Databasens primärnyckel.
 * @return 0 vid framgång, -1 vid fel.
 */
int db_delete_qso(int64_t id);

/**
 * @brief Raderar alla poster som är markerade (marked == 1) i listan ur databasen inom en transaktion.
 * @param list Pekare till QSO-listan.
 * @return Antal raderade poster ur databasen.
 */
int db_delete_marked(const QSOList *list);

/**
 * @brief Sätter hanteringsstatus (handled) för ett enskilt QSO i databasen.
 * @param id Databasens primärnyckel.
 * @param handled 1 för hanterad (✓), 0 för ny (✗).
 * @return 0 vid framgång, -1 vid fel.
 */
int db_set_handled(int64_t id, int handled);

/**
 * @brief Sätter hanteringsstatus (handled) för ALLA QSO:n i databasen (F7).
 * @param handled 1 för hanterad (✓), 0 för ny (✗).
 * @return Antal uppdaterade rader, eller -1 vid fel.
 */
int db_set_all_handled(int handled);

/**
 * @brief Tömmer hela QSO-tabellen i databasen.
 * @return 0 vid framgång, -1 vid fel.
 */
int db_clear_all_qsos(void);

/**
 * @brief Infogar en uppsättning QSO-poster i databasen inom en enda transaktion.
 * Tilldelar qsos[i].id för samtliga infogade poster.
 * @param qsos Array med QSO-poster.
 * @param count Antal poster att infoga.
 * @return Antal infogade poster, eller -1 vid fel.
 */
int db_batch_insert(QSO *qsos, size_t count);

/**
 * @brief Räknar totalt antal QSO:n i databasen.
 * @return Antal rader, eller -1 vid fel.
 */
int db_count_qsos(void);

/**
 * @brief Migrerar automatiskt från en ADIF-fil till databasen om databasen är tom och filen finns.
 * @param adif_path Sökväg till befintlig ADIF-fil (t.ex. "qsolog.adi").
 * @param st Stationsinställningar.
 * @return Antal migrerade poster, eller 0 om ingen migrering utfördes, -1 vid fel.
 */
int db_migrate_from_adif(const char *adif_path, const StationSettings *st);

#ifdef __cplusplus
}
#endif

#endif /* DB_H */
