/**
 * @file version.h
 * @brief Centrala definitioner för applikationens namn, versionsnummer och byggdatum.
 *
 * Uppdatera dessa makron på ett enda ställe för att ändra applikationsnamn,
 * versionsnummer eller byggdatum i hela programmet.
 */

#ifndef VERSION_H
#define VERSION_H

/** Applikationens visningsnamn i titelrad, fönstertitel, hjälptext och CLI */
#define APP_NAME         "UEI-Logg"

/** Applikationens fullständiga namn (alias till APP_NAME) */
#define APP_FULL_NAME    APP_NAME

/** Program-ID i ADIF-filer och HTTP User-Agent */
#define APP_PROGRAM_ID   "qsolog"

/** Applikationens versionsnummer */
#define APP_VERSION      "1.1.0"

/** Byggdatum (ISO YYYY-MM-DD; kan även sättas till __DATE__ vid behov) */
#define APP_BUILD_DATE   "2026-11-25"

/** Sammansatt namn och version (t.ex. "UEI-Logg v1.1.0") */
#define APP_TITLE_STRING APP_NAME " v" APP_VERSION

#endif /* VERSION_H */
