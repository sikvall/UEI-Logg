#ifndef SPACE_WEATHER_H
#define SPACE_WEATHER_H

#include "qsolog.h"

/**
 * @file space_weather.h
 * @brief Rymdvädersdata inhämtat asynkront från https://hf.avitec.se.
 */

/* Färgstatus / varningsnivå för mätvärden motsvarande mätarna på hf.avitec.se */
typedef enum {
    SW_TONE_NORMAL = 0,
    SW_TONE_OK,      /* Grön - lugna/gynnsamma förhållanden */
    SW_TONE_WARN,    /* Gul - förhöjd aktivitet / begränsande */
    SW_TONE_BAD,     /* Röd - stört läge / storm */
    SW_TONE_SEVERE   /* Magenta/Kraftsatt röd - extremt läge */
} SwTone;

/* Struktur med extraherad rymdvädersdata för visning */
typedef struct {
    char k_kiruna[32];      /* K-index från Kiruna (t.ex. "3/2" eller "3") */
    SwTone k_tone;

    char muf[32];           /* Maximum Usable Frequency (t.ex. "11,6 MHz") */
    SwTone muf_tone;

    char luf[32];           /* Lowest Usable Frequency (t.ex. "1,5 MHz") */
    SwTone luf_tone;

    char cme[32];           /* Coronal Mass Ejection sannolikhet (t.ex. "0 %") */
    SwTone cme_tone;

    char solfackla[32];     /* Solfackla / Solar Flare (t.ex. "C1.4" eller "M1.2") */
    SwTone solfackla_tone;

    char solvind[32];       /* Solvindshastighet (t.ex. "627 km/s") */
    SwTone solvind_tone;

    char norrsken[32];      /* Norrskenseffekt (t.ex. "12 GW") */
    SwTone norrsken_tone;

    int has_data;           /* 1 om data har hämtats från nätet */
    char updated_time[32];  /* Tidsstämpel för senaste lyckade uppdatering */
} SpaceWeatherData;

/* Initiera bakgrundstråd för rymdväder och starta första hämtningen */
void space_weather_init(void);

/* Avbryt bakgrundstråden och städa upp */
void space_weather_cleanup(void);

/* Hämta en trådsäker kopia av senaste rymdvädersdata */
void space_weather_get_data(SpaceWeatherData *out);

/* Begär en omedelbar asynkron uppdatering från hf.avitec.se */
void space_weather_refresh_async(void);

#endif /* SPACE_WEATHER_H */
