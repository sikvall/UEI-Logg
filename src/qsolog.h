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
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>
#include <unistd.h>

#ifndef PATH_MAX
#define PATH_MAX 4096
#endif

#ifndef DEFAULT_LOG_FILE
#define DEFAULT_LOG_FILE "qsolog.sq3"
#endif

extern char g_log_filename[256];
#ifndef LOG_FILE
#define LOG_FILE g_log_filename
#endif
#ifndef CONFIG_FILE
#define CONFIG_FILE "station.conf"
#endif
#ifndef DXCC_FILE
#define DXCC_FILE "dxcc.txt"
#endif

#define MIN_COLS 60
#define MIN_LINES 24
#define QSO_FIELD_COUNT 13
#define STATION_FIELD_COUNT 21

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

/* Färgscheman (UEI-tema, Borland Turbo Pascal, Midnight Commander, Monokrom, Retro Ambra, Arktisk Natt, Field Day, Mörkerläge, Solarized Dark, Dracula, Militärradio samt Egen profil) */
extern const char * const THEME_NAMES[];
#define THEME_COUNT 16
#define THEME_DEFAULT 0
#define THEME_CUSTOM 15

/**
 * @brief Radens bakgrundsfärg i loggradskomponenten.
 */
enum {
    LOG_COLOR_NORMAL = 0, /**< Standard / Normal */
    LOG_COLOR_GREEN  = 1, /**< Grön */
    LOG_COLOR_YELLOW = 2, /**< Gul */
    LOG_COLOR_RED    = 3, /**< Röd */
    LOG_COLOR_COUNT  = 4
};

/**
 * @brief Egen anpassad färgprofil (Custom Theme).
 * Innehåller index till färger i paletten för respektive gränssnittselement.
 */
typedef struct {
    int bg_main;        /**< 0: Huvudbakgrund */
    int fg_text;        /**< 1: Normal text */
    int border;         /**< 2: Normala fönsterramar */
    int border_active;  /**< 3: Aktiv fönsterram (Fokus) */
    int title;          /**< 4: Titlar & fönsterrubriker */
    int label;          /**< 5: Fältetiketter (Signal:, QTH:) */
    int field_act_bg;   /**< 6: Aktivt fält (Bakgrund) */
    int field_act_fg;   /**< 7: Aktivt fält (Text) */
    int sel_bg;         /**< 8: Vald loggrad (Bakgrund) */
    int sel_fg;         /**< 9: Vald loggrad (Text) */
    int topbar_bg;      /**< 10: Titelrad / Topplist (Bakgrund) */
    int topbar_fg;      /**< 11: Titelrad / Topplist (Text) */
    int status_ok;      /**< 12: Status OK / bock */
    int status_warn;    /**< 13: Varning / fel / kryss */
} CustomTheme;

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
 * @brief Lägen för QRZ.COM-uppslag (inställning: Auto QRZ.COM).
 */
enum {
    QRZ_MODE_OFF    = 0, /**< Av helt och hållet */
    QRZ_MODE_AUTO   = 1, /**< Auto: Slå upp automatiskt vid inmatning */
    QRZ_MODE_MANUAL = 2  /**< Manuell: Uppslag endast vid manuell F5-tryckning */
};

/**
 * @brief Lägen för direktloggning mot QRZ.COM Logbook API (inställning: Direktlogg QRZ.COM).
 */
enum {
    QRZ_LOG_OFF    = 0, /**< Av: Ingen direktloggning mot QRZ.COM */
    QRZ_LOG_AUTO   = 1, /**< Automatisk: Loggning sker direkt på Enter även till QRZ.COM */
    QRZ_LOG_MANUAL = 2  /**< Manuell: Loggning till QRZ.COM sker med Ctrl+L på loggrad eller i inmatningsrutan */
};

/**
 * @brief Index för inmatningsfälten i visnings- och tabulatorordning (4 rader).
 */
enum {
    FLD_CALL = 0,  /**< Rad 1: Signal (10 tecken) */
    FLD_RSTS = 1,  /**< Rad 1: RST T / sänd rapport (6 tecken) */
    FLD_RSTR = 2,  /**< Rad 1: RST R / mottagen rapport (6 tecken) */
    FLD_NAME = 3,  /**< Rad 1: Namn (12-16 tecken) */
    FLD_DXCC = 4,  /**< Rad 2: DXCC-land (14-15 tecken) */
    FLD_GRID = 5,  /**< Rad 2: Maidenhead-lokator (6 tecken) */
    FLD_QTH  = 6,  /**< Rad 2: QTH (12-16 tecken) */
    FLD_FREQ = 7,  /**< Rad 3: Frekvens i MHz */
    FLD_BAND = 8,  /**< Rad 3: Amatörband (dropdown) */
    FLD_MODE = 9,  /**< Rad 3: Trafiksätt (t.ex. SSB, CW, FT8) */
    FLD_DATE = 10, /**< Rad 4: Datum (YYYY-MM-DD) */
    FLD_TIME = 11, /**< Rad 4: Tid UTC (HH:MM) */
    FLD_COMM = 12  /**< Rad 4: Notering / kommentar */
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
    char qrz_api_key[64];  /**< QRZ.COM Logbook API-nyckel (för logbook.qrz.com/api) */
    int  qrz_enabled;      /**< QRZ-uppslagsläge: QRZ_MODE_OFF (0), QRZ_MODE_AUTO (1), QRZ_MODE_MANUAL (2) */
    int  qrz_log_enabled;  /**< QRZ-direktloggningsläge: QRZ_LOG_OFF (0), QRZ_LOG_AUTO (1), QRZ_LOG_MANUAL (2) */
    int  theme;            /**< Index för aktivt färgschema (0..THEME_COUNT-1) */
    CustomTheme custom_theme; /**< Egen anpassad färgprofil */
    int  smcb_enabled;     /**< 1 om automatisk SM Callbook-uppslagning är aktiv */
    int  auto_rst_59;      /**< 1 om RST förifylls automatiskt med 59/599 vid nytt QSO */
    int  filter_log;       /**< 1 om loggrader filtreras automatiskt efter inmatad signal */
    int  show_stats;       /**< 1 om statistikrutan ovanför QSO-inmatningen ska visas (default 1) */
    int  show_space_weather; /**< 1 om rymdvädersrutan längst ned ska visas (default 1) */
    int  show_notes;       /**< 1 om noteringsrader i loggen ska visas (default 1) */
    char last_freq[32];    /**< Senast använda frekvens */
    char last_band[16];    /**< Senast använda band */
    char last_mode[16];    /**< Senast använda mode */
} StationSettings;

/**
 * @brief Data för ett enskilt QSO (kontakt).
 */
typedef struct {
    int64_t id;          /**< Primärnyckel (rowid) i SQLite3-databasen */
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
    int  color;          /**< Radfärg: 0=Standard/Normal, 1=Grön, 2=Gul, 3=Röd */
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
