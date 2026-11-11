/**
 * @file space_weather.h
 * @brief Rymdvädersdata inhämtat asynkront från https://hf.avitec.se.
 */

#ifndef SPACE_WEATHER_H
#define SPACE_WEATHER_H

#include "qsolog.h"

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Färgstatus / varningsnivå för mätvärden motsvarande mätarna på hf.avitec.se.
 */
typedef enum {
    SW_TONE_NORMAL = 0, /**< Normal färg */
    SW_TONE_OK,         /**< Grön - lugna/gynnsamma förhållanden */
    SW_TONE_WARN,       /**< Gul - förhöjd aktivitet / begränsande förhållanden */
    SW_TONE_BAD,        /**< Röd - stört läge / storm */
    SW_TONE_SEVERE      /**< Kraftsatt röd/magenta - extremt läge */
} SwTone;

/**
 * @brief Struktur med extraherad rymdvädersdata för visning i gränssnittet.
 */
typedef struct {
    char k_kiruna[32];      /**< K-index från Kiruna (t.ex. "3/2" eller "3") */
    SwTone k_tone;          /**< Färgstatus för K-index */

    char muf[32];           /**< Maximum Usable Frequency (t.ex. "11,6 MHz") */
    SwTone muf_tone;        /**< Färgstatus för MUF */

    char luf[32];           /**< Lowest Usable Frequency (t.ex. "1,5 MHz") */
    SwTone luf_tone;        /**< Färgstatus för LUF */

    char cme[32];           /**< Coronal Mass Ejection sannolikhet (t.ex. "0 %") */
    SwTone cme_tone;        /**< Färgstatus för CME */

    char solfackla[32];     /**< Solfackla / Solar Flare (t.ex. "C1.4") */
    SwTone solfackla_tone;  /**< Färgstatus för solfackla */

    char solvind[32];       /**< Solvindshastighet (t.ex. "627 km/s") */
    SwTone solvind_tone;    /**< Färgstatus för solvind */

    char norrsken[32];      /**< Norrskenseffekt (t.ex. "12 GW") */
    SwTone norrsken_tone;   /**< Färgstatus för norrsken */

    int has_data;           /**< 1 om data framgångsrikt hämtats från nätet */
    char updated_time[32];  /**< Tidsstämpel för senaste lyckade uppdatering */
} SpaceWeatherData;

/**
 * @brief Initierar bakgrundstråd för rymdväder och startar första hämtningen från https://hf.avitec.se.
 */
void space_weather_init(void);

/**
 * @brief Avbryter bakgrundstråden och frigör trådresurser vid programavslut.
 */
void space_weather_cleanup(void);

/**
 * @brief Hämtar en trådsäker kopia av senaste inhämtade rymdvädersdata.
 * @param out Pekare till struktur där datat kopieras.
 */
void space_weather_get_data(SpaceWeatherData *out);

/**
 * @brief Begär en omedelbar asynkron uppdatering från hf.avitec.se.
 */
void space_weather_refresh_async(void);

#ifdef __cplusplus
}
#endif

#endif /* SPACE_WEATHER_H */
