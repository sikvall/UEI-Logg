/**
 * @file theme_editor.h
 * @brief Interaktiv editor för användaranpassade färgscheman (Custom Theme).
 */

#ifndef THEME_EDITOR_H
#define THEME_EDITOR_H

#include "qsolog.h"

#ifdef __cplusplus
extern "C" {
#endif

#define CUSTOM_PALETTE_SIZE 22

/**
 * @brief Initierar en CustomTheme-struktur med balanserade standardfärger (mörk antracit & cyan).
 * @param ct Pekare till CustomTheme som ska initieras.
 */
void custom_theme_init_defaults(CustomTheme *ct);

/**
 * @brief Initierar en CustomTheme-struktur utifrån en förinställd mall (preset 0..7).
 * @param ct Pekare till CustomTheme.
 * @param preset Index på förinställt tema.
 */
void custom_theme_init_preset(CustomTheme *ct, int preset);

/**
 * @brief Hämtar det svenska visningsnamnet för en färg i paletten.
 * @param color_idx Färgindex (0..CUSTOM_PALETTE_SIZE-1).
 * @return Sträng med färgnamn.
 */
const char *custom_theme_get_color_name(int color_idx);

/**
 * @brief Applicerar färginställningarna i en CustomTheme på ncurses färgpar (CP_*).
 * @param ct Pekare till CustomTheme vars färger ska sättas.
 */
void apply_custom_theme_pairs(const CustomTheme *ct);

/**
 * @brief Öppnar den interaktiva färgprofilseditorn från inställningssidan.
 * Användaren navigerar bland element och bläddrar bland färger med realtidsförhandsgranskning.
 * @param edit Stationsinställningar vars custom_theme redigeras.
 * @return 1 om profilen sparades/ändrades, 0 om användaren avbröt med Esc.
 */
int run_color_theme_editor(StationSettings *edit);

#ifdef __cplusplus
}
#endif

#endif /* THEME_EDITOR_H */
