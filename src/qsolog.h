/**
 * @file qsolog.h
 * @brief Globala definitioner, strukturer och konstanter för UEI-Logg / qsolog.
 */

#ifndef QSOLOG_H
#define QSOLOG_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif
#define _POSIX_C_SOURCE 200809L

#include <ctype.h>
#include <limits.h>
#include <locale.h>
#include <math.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef LOG_FILE
#define LOG_FILE "qsolog.adi"
#endif
#ifndef CONFIG_FILE
#define CONFIG_FILE "station.conf"
#endif
#ifndef DXCC_FILE
#define DXCC_FILE "dxcc.txt"
#endif

#define MIN_COLS 60
#define MIN_LINES 20
#define QSO_FIELD_COUNT 13
#define STATION_FIELD_COUNT 15

#ifndef KEY_CTRL_HOME
#define KEY_CTRL_HOME 2001
#endif
#ifndef KEY_CTRL_END
#define KEY_CTRL_END 2002
#endif

/* Tillåtna band i bandlistan samt standardfrekvens (MHz) med 3 decimaler */
extern const char * const BAND_OPTIONS[];
extern const char * const BAND_DEFAULT_FREQ[];
#define BAND_OPTION_COUNT 14

/* Färgscheman (Borland Turbo Pascal, Midnight Commander, Monokrom, Retro Ambra m.fl.) */
extern const char * const THEME_NAMES[];
#define THEME_COUNT 8

/**
 * @brief Fokusläge i huvudgränssnittet (F4 växlar mellan inmatning och loggrader).
 */
typedef enum {
    FOCUS_INPUT = 0, /**< Tangentbordsfokus i QSO-inmatningsfälten */
    FOCUS_LOG   = 1  /**< Tangentbordsfokus i listan över tidigare loggrader */
} FocusMode;

/**
 * @brief Exportläge vid skapande av ADIF-filer.
 */
enum {
    EXPORT_MODE_ALL      = 0, /**< Exportera alla QSO-poster */
    EXPORT_MODE_MARKED   = 1, /**< Exportera endast rader markerade med INS */
    EXPORT_MODE_ONLY_NEW = 2  /**< Exportera endast nya/ohanerande rader */
};

/**
 * @brief Index för inmatningsfälten i visnings- och tabulatorordning.
 */
enum {
    FLD_CALL = 0,  /**< Signal (10 tecken) */
    FLD_RSTS = 1,  /**< RST T / sänd rapport (6 tecken) */
    FLD_RSTR = 2,  /**< RST R / mottagen rapport (6 tecken) */
    FLD_NAME = 3,  /**< Namn (12 tecken) */
    FLD_QTH  = 4,  /**< QTH (15 tecken) */
    FLD_DATE = 5,  /**< Datum (YYYY-MM-DD) */
    FLD_TIME = 6,  /**< Tid UTC (HH:MM) */
    FLD_FREQ = 7,  /**< Frekvens i MHz */
    FLD_BAND = 8,  /**< Amatörband (t.ex. 20m) */
    FLD_GRID = 9,  /**< Maidenhead-lokator (6 tecken) */
    FLD_DXCC = 10, /**< DXCC-land */
    FLD_MODE = 11, /**< Trafiksätt (t.ex. SSB, CW, FT8) */
    FLD_COMM = 12  /**< Notering / kommentar */
};

/**
 * @brief Konfiguration för min egen station (station.conf).
 */
typedef struct {
    char call[32];         /**< Egen anropssignal (ADIF: STATION_CALLSIGN) */
    char name[64];         /**< Eget namn (ADIF: MY_NAME) */
    char qth[64];          /**< Eget QTH (ADIF: MY_CITY) */
    char gridsquare[16];   /**< Egen lokator (ADIF: MY_GRIDSQUARE) */
    char timezone[32];     /**< Lokal tidszon (t.ex. UTC+1, Europe/Stockholm) */
    char qrz_username[36]; /**< QRZ.COM användarnamn */
    char qrz_password[36]; /**< QRZ.COM lösenord */
    int  qrz_enabled;      /**< 1 om automatisk QRZ-uppslagning är aktiv */
    int  theme;            /**< Index för aktivt färgschema (0..THEME_COUNT-1) */
    int  smcb_enabled;     /**< 1 om automatisk SM Callbook-uppslagning är aktiv */
    int  auto_rst_59;      /**< 1 om RST förifylls automatiskt med 59/599 vid nytt QSO */
    int  filter_log;       /**< 1 om loggrader filtreras automatiskt efter inmatad signal */
    char last_freq[32];    /**< Senast använda frekvens */
    char last_band[16];    /**< Senast använda band */
    char last_mode[16];    /**< Senast använda mode */
} StationSettings;

/**
 * @brief Data för ett enskilt QSO (kontakt).
 */
typedef struct {
    char call[32];       /**< Motstationens anropssignal (ADIF: CALL) */
    char name[64];       /**< Motstationens operatörsnamn (ADIF: NAME) */
    char date[16];       /**< QSO-datum i format YYYY-MM-DD (ADIF: QSO_DATE) */
    char time_utc[16];   /**< QSO-tid i UTC format HH:MM (ADIF: TIME_ON) */
    char gridsquare[16]; /**< Motstationens Maidenhead-lokator (ADIF: GRIDSQUARE) */
    char freq[32];       /**< Frekvens i MHz (ADIF: FREQ) */
    char band[16];       /**< Amatörband (ADIF: BAND) */
    char mode[16];       /**< Trafiksätt (ADIF: MODE) */
    char rst_sent[16];   /**< Sänd rapport RST T (ADIF: RST_SENT) */
    char rst_rcvd[16];   /**< Mottagen rapport RST R (ADIF: RST_RCVD) */
    char qth[64];        /**< Motstationens ort eller land (ADIF: QTH) */
    char dxcc[64];       /**< DXCC-land (ADIF: COUNTRY) */
    char comment[256];   /**< Kommentar eller notering (ADIF: COMMENT) */
    char distance[16];   /**< Beräknat avstånd i km (ADIF: DISTANCE) */
    int  handled;        /**< Hanteringsstatus: 0 = ny (✗), 1 = hanterad (✓) */
    int  marked;         /**< 1 om raden är tillfälligt markerad med INS */
} QSO;

/**
 * @brief Dynamisk lista av lagrade QSO-poster (Loggrader).
 */
typedef struct {
    QSO *items;      /**< Pekare till allokerad array av QSO:n */
    size_t count;    /**< Antal poster i listan */
    size_t capacity; /**< Allokerad kapacitet */
} QSOList;

#endif /* QSOLOG_H */
