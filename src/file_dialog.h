/**
 * @file file_dialog.h
 * @brief Interaktiv fildialog (filbläddrare) för import och export i UEI-Logg.
 *
 * Tillhandahåller ett modal-fönster i ncurses för att bläddra i katalogstrukturen,
 * filtrera filändelser samt välja befintliga filer för inläsning eller ange
 * nya filnamn för sparande.
 */

#ifndef FILE_DIALOG_H
#define FILE_DIALOG_H

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Kör en interaktiv fildialog i curses för att välja eller spara en fil.
 *
 * @param title Titel som visas i dialogens överkant.
 * @param start_dir Startkatalog att bläddra i (t.ex. ".").
 * @param ext_filter Semikolonseparerad lista över tillåtna filändelser (t.ex. ".adi;.adif"), eller NULL för alla.
 * @param selected_path Buffer där den fullständiga valda sökvägen skrivs vid bekräftelse.
 * @param max_path_len Storlek på bufferten selected_path.
 * @param is_save_mode 0 för öppna/importera, 1 för spara/exportera (aktiverar filnamnsinmatning).
 * @param default_filename Förvalt filnamn vid spara-läge (eller NULL).
 * @return 1 om användaren bekräftade med Enter, 0 om dialogen avbröts med Esc.
 */
int run_file_dialog(const char *title, const char *start_dir, const char *ext_filter,
                    char *selected_path, size_t max_path_len, int is_save_mode,
                    const char *default_filename);

#ifdef __cplusplus
}
#endif

#endif /* FILE_DIALOG_H */
