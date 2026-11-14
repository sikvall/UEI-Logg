/**
 * @file db.c
 * @brief Implementering av SQLite3-databaslagret för UEI-Logg (qsolog.sq3).
 */

#include "db.h"
#include "logg.h"
#include "util.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static sqlite3 *s_db = NULL;

static const char *SCHEMA_SQL =
    "PRAGMA journal_mode = WAL;\n"
    "PRAGMA synchronous = NORMAL;\n"
    "CREATE TABLE IF NOT EXISTS qsos (\n"
    "    id INTEGER PRIMARY KEY AUTOINCREMENT,\n"
    "    call TEXT NOT NULL,\n"
    "    name TEXT,\n"
    "    qso_date TEXT NOT NULL,\n"
    "    time_utc TEXT NOT NULL,\n"
    "    gridsquare TEXT,\n"
    "    freq TEXT,\n"
    "    band TEXT,\n"
    "    mode TEXT,\n"
    "    rst_sent TEXT,\n"
    "    rst_rcvd TEXT,\n"
    "    qth TEXT,\n"
    "    dxcc TEXT,\n"
    "    comment TEXT,\n"
    "    distance TEXT,\n"
    "    handled INTEGER DEFAULT 0\n"
    ");\n"
    "CREATE INDEX IF NOT EXISTS idx_qsos_call ON qsos(call);\n"
    "CREATE INDEX IF NOT EXISTS idx_qsos_date_time ON qsos(qso_date, time_utc);\n"
    "CREATE INDEX IF NOT EXISTS idx_qsos_band ON qsos(band);\n"
    "CREATE INDEX IF NOT EXISTS idx_qsos_handled ON qsos(handled);\n";

int db_open(const char *path) {
    if (s_db != NULL) {
        db_close();
    }
    if (path == NULL || path[0] == '\0') {
        path = DEFAULT_LOG_FILE;
    }

    int rc = sqlite3_open(path, &s_db);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Kunde inte öppna SQLite3-databasen '%s': %s\n",
                path, sqlite3_errmsg(s_db));
        if (s_db != NULL) {
            sqlite3_close(s_db);
            s_db = NULL;
        }
        return -1;
    }

    char *errmsg = NULL;
    rc = sqlite3_exec(s_db, SCHEMA_SQL, NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Fel vid initiering av databasschema: %s\n",
                errmsg ? errmsg : "okänt fel");
        sqlite3_free(errmsg);
        sqlite3_close(s_db);
        s_db = NULL;
        return -1;
    }

    return 0;
}

void db_close(void) {
    if (s_db != NULL) {
        sqlite3_close(s_db);
        s_db = NULL;
    }
}

int db_is_open(void) {
    return (s_db != NULL);
}

sqlite3 *db_get_handle(void) {
    return s_db;
}

static void read_column_text(sqlite3_stmt *stmt, int col, char *dest, size_t dest_sz) {
    const unsigned char *text = sqlite3_column_text(stmt, col);
    if (text != NULL) {
        copy_str(dest, dest_sz, (const char *)text);
    } else {
        dest[0] = '\0';
    }
}

int db_load_all_qsos(QSOList *list) {
    if (s_db == NULL || list == NULL) return -1;

    list->count = 0;

    const char *sql =
        "SELECT id, call, name, qso_date, time_utc, gridsquare, freq, band, mode, "
        "rst_sent, rst_rcvd, qth, dxcc, comment, distance, handled "
        "FROM qsos ORDER BY qso_date ASC, time_utc ASC, id ASC;";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Fel vid prepare i db_load_all_qsos: %s\n", sqlite3_errmsg(s_db));
        return -1;
    }

    while (sqlite3_step(stmt) == SQLITE_ROW) {
        QSO q;
        memset(&q, 0, sizeof(q));
        q.id = sqlite3_column_int64(stmt, 0);
        read_column_text(stmt, 1, q.call, sizeof(q.call));
        read_column_text(stmt, 2, q.name, sizeof(q.name));
        read_column_text(stmt, 3, q.date, sizeof(q.date));
        read_column_text(stmt, 4, q.time_utc, sizeof(q.time_utc));
        read_column_text(stmt, 5, q.gridsquare, sizeof(q.gridsquare));
        read_column_text(stmt, 6, q.freq, sizeof(q.freq));
        read_column_text(stmt, 7, q.band, sizeof(q.band));
        read_column_text(stmt, 8, q.mode, sizeof(q.mode));
        read_column_text(stmt, 9, q.rst_sent, sizeof(q.rst_sent));
        read_column_text(stmt, 10, q.rst_rcvd, sizeof(q.rst_rcvd));
        read_column_text(stmt, 11, q.qth, sizeof(q.qth));
        read_column_text(stmt, 12, q.dxcc, sizeof(q.dxcc));
        read_column_text(stmt, 13, q.comment, sizeof(q.comment));
        read_column_text(stmt, 14, q.distance, sizeof(q.distance));
        q.handled = sqlite3_column_int(stmt, 15);
        q.marked = 0;

        qso_list_push(list, &q);
    }

    sqlite3_finalize(stmt);
    return (int)list->count;
}

static const char *INSERT_SQL =
    "INSERT INTO qsos (call, name, qso_date, time_utc, gridsquare, freq, band, mode, "
    "rst_sent, rst_rcvd, qth, dxcc, comment, distance, handled) "
    "VALUES (?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?);";

static int bind_qso_params(sqlite3_stmt *stmt, const QSO *qso) {
    sqlite3_bind_text(stmt, 1, qso->call, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 2, qso->name, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 3, qso->date, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 4, qso->time_utc, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 5, qso->gridsquare, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 6, qso->freq, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 7, qso->band, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 8, qso->mode, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 9, qso->rst_sent, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 10, qso->rst_rcvd, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 11, qso->qth, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 12, qso->dxcc, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 13, qso->comment, -1, SQLITE_TRANSIENT);
    sqlite3_bind_text(stmt, 14, qso->distance, -1, SQLITE_TRANSIENT);
    sqlite3_bind_int(stmt, 15, qso->handled);
    return SQLITE_OK;
}

int db_insert_qso(QSO *qso) {
    if (s_db == NULL || qso == NULL) return -1;

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(s_db, INSERT_SQL, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Fel vid prepare i db_insert_qso: %s\n", sqlite3_errmsg(s_db));
        return -1;
    }

    bind_qso_params(stmt, qso);

    rc = sqlite3_step(stmt);
    if (rc != SQLITE_DONE) {
        fprintf(stderr, "Fel vid insert i db_insert_qso: %s\n", sqlite3_errmsg(s_db));
        sqlite3_finalize(stmt);
        return -1;
    }

    qso->id = sqlite3_last_insert_rowid(s_db);
    sqlite3_finalize(stmt);
    return 0;
}

int db_update_qso(const QSO *qso) {
    if (s_db == NULL || qso == NULL || qso->id <= 0) return -1;

    const char *sql =
        "UPDATE qsos SET call=?, name=?, qso_date=?, time_utc=?, gridsquare=?, "
        "freq=?, band=?, mode=?, rst_sent=?, rst_rcvd=?, qth=?, dxcc=?, comment=?, "
        "distance=?, handled=? WHERE id=?;";

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        fprintf(stderr, "Fel vid prepare i db_update_qso: %s\n", sqlite3_errmsg(s_db));
        return -1;
    }

    bind_qso_params(stmt, qso);
    sqlite3_bind_int64(stmt, 16, qso->id);

    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_delete_qso(int64_t id) {
    if (s_db == NULL || id <= 0) return -1;

    const char *sql = "DELETE FROM qsos WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL);
    if (rc != SQLITE_OK) return -1;

    sqlite3_bind_int64(stmt, 1, id);
    rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_delete_marked(const QSOList *list) {
    if (s_db == NULL || list == NULL) return 0;

    char *errmsg = NULL;
    sqlite3_exec(s_db, "BEGIN TRANSACTION;", NULL, NULL, &errmsg);

    const char *sql = "DELETE FROM qsos WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL) != SQLITE_OK) {
        sqlite3_exec(s_db, "ROLLBACK;", NULL, NULL, NULL);
        return 0;
    }

    int del_cnt = 0;
    for (size_t i = 0; i < list->count; ++i) {
        if (list->items[i].marked && list->items[i].id > 0) {
            sqlite3_reset(stmt);
            sqlite3_bind_int64(stmt, 1, list->items[i].id);
            if (sqlite3_step(stmt) == SQLITE_DONE) {
                del_cnt++;
            }
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(s_db, "COMMIT;", NULL, NULL, NULL);
    return del_cnt;
}

int db_set_handled(int64_t id, int handled) {
    if (s_db == NULL || id <= 0) return -1;

    const char *sql = "UPDATE qsos SET handled = ? WHERE id = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_int(stmt, 1, handled ? 1 : 0);
    sqlite3_bind_int64(stmt, 2, id);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);
    return (rc == SQLITE_DONE) ? 0 : -1;
}

int db_set_all_handled(int handled) {
    if (s_db == NULL) return -1;

    const char *sql = "UPDATE qsos SET handled = ?;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    sqlite3_bind_int(stmt, 1, handled ? 1 : 0);

    int rc = sqlite3_step(stmt);
    sqlite3_finalize(stmt);

    if (rc == SQLITE_DONE) {
        return sqlite3_changes(s_db);
    }
    return -1;
}

int db_clear_all_qsos(void) {
    if (s_db == NULL) return -1;

    char *errmsg = NULL;
    int rc = sqlite3_exec(s_db, "DELETE FROM qsos;", NULL, NULL, &errmsg);
    if (rc != SQLITE_OK) {
        if (errmsg) sqlite3_free(errmsg);
        return -1;
    }
    sqlite3_exec(s_db, "VACUUM;", NULL, NULL, NULL);
    return 0;
}

int db_batch_insert(QSO *qsos, size_t count) {
    if (s_db == NULL || qsos == NULL || count == 0) return 0;

    sqlite3_exec(s_db, "BEGIN TRANSACTION;", NULL, NULL, NULL);

    sqlite3_stmt *stmt = NULL;
    int rc = sqlite3_prepare_v2(s_db, INSERT_SQL, -1, &stmt, NULL);
    if (rc != SQLITE_OK) {
        sqlite3_exec(s_db, "ROLLBACK;", NULL, NULL, NULL);
        return -1;
    }

    int inserted = 0;
    for (size_t i = 0; i < count; ++i) {
        sqlite3_reset(stmt);
        bind_qso_params(stmt, &qsos[i]);
        if (sqlite3_step(stmt) == SQLITE_DONE) {
            qsos[i].id = sqlite3_last_insert_rowid(s_db);
            inserted++;
        }
    }

    sqlite3_finalize(stmt);
    sqlite3_exec(s_db, "COMMIT;", NULL, NULL, NULL);
    return inserted;
}

int db_count_qsos(void) {
    if (s_db == NULL) return -1;

    const char *sql = "SELECT COUNT(*) FROM qsos;";
    sqlite3_stmt *stmt = NULL;
    if (sqlite3_prepare_v2(s_db, sql, -1, &stmt, NULL) != SQLITE_OK) return -1;

    int count = 0;
    if (sqlite3_step(stmt) == SQLITE_ROW) {
        count = sqlite3_column_int(stmt, 0);
    }
    sqlite3_finalize(stmt);
    return count;
}

int db_migrate_from_adif(const char *adif_path, const StationSettings *st) {
    if (s_db == NULL || adif_path == NULL || adif_path[0] == '\0') return 0;

    /* Kontrollera om ADIF-filen faktiskt finns */
    FILE *fp = fopen(adif_path, "r");
    if (fp == NULL) {
        return 0; /* Ingen fil att migrera från */
    }
    fclose(fp);

    /* Kontrollera om databasen redan innehåller poster */
    if (db_count_qsos() > 0) {
        return 0; /* Databasen har redan data; migrera inte över */
    }

    /* Läs in alla poster från ADIF-filen */
    QSOList adif_list = {NULL, 0, 0};
    load_qsos_from_adif(adif_path, &adif_list);

    if (adif_list.count == 0) {
        if (adif_list.items) free(adif_list.items);
        return 0;
    }

    (void)st;
    int migrated = db_batch_insert(adif_list.items, adif_list.count);

    if (adif_list.items) {
        free(adif_list.items);
    }

    return migrated;
}
