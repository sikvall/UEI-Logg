/**
 * @file file_dialog.c
 * @brief Interaktiv curses-baserad filbläddrare för import och export av ADIF-filer.
 *
 * Implementerar en fullfjädrad fildialog med katalogbläddring, storleksvisning,
 * ändelsefiltrering, filnamnsinmatning vid export samt bekräftelse/avbrytande.
 */

#define _XOPEN_SOURCE 700
#include "file_dialog.h"
#include "qsolog.h"
#include "util.h"
#include "tui.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <dirent.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>
#include <curses.h>

/**
 * @brief Representerar en fil eller katalog i fillistan.
 */
typedef struct {
    char name[256];   /**< Fil- eller mappnamn */
    int is_dir;       /**< 1 om katalog, 0 om fil */
    long size_bytes;  /**< Storlek i bytes (-1 för kataloger) */
} FileDialogItem;

/**
 * @brief Sorteringsfunktion för fildialogen: mappar först, sedan alfabetiskt.
 * @param a_ptr Pekare till första elementet.
 * @param b_ptr Pekare till andra elementet.
 * @return Negativt om a ska ligga före b, positivt om b ska ligga före a.
 */
static int cmp_file_dialog_item(const void *a_ptr, const void *b_ptr) {
    const FileDialogItem *a = (const FileDialogItem *)a_ptr;
    const FileDialogItem *b = (const FileDialogItem *)b_ptr;
    if (strcmp(a->name, "..") == 0) return -1;
    if (strcmp(b->name, "..") == 0) return 1;
    if (a->is_dir != b->is_dir) {
        return a->is_dir ? -1 : 1;
    }
    return strcasecmp(a->name, b->name);
}

/**
 * @brief Matchar ett filnamn mot en semikolonseparerad lista av tillåtna ändelser (t.ex. ".adi;.adif").
 * @param filename Filnamn att kontrollera.
 * @param ext_list Lista med ändelser (t.ex. ".adi;.adif"), eller NULL för alla.
 * @return 1 om filen matchar filtret, annars 0.
 */
static int matches_extension(const char *filename, const char *ext_list) {
    if (ext_list == NULL || ext_list[0] == '\0') return 1;
    const char *dot = strrchr(filename, '.');
    if (dot == NULL) return 0;

    char buf[128];
    copy_str(buf, sizeof(buf), ext_list);
    char *token = strtok(buf, ";, ");
    while (token != NULL) {
        while (*token == '*' || *token == ' ') token++;
        if (*token == '.') {
            if (strcasecmp(dot, token) == 0) return 1;
        } else {
            if (strcasecmp(dot + 1, token) == 0) return 1;
        }
        token = strtok(NULL, ";, ");
    }
    return 0;
}

/**
 * @brief Formaterar en filstorlek i ett lättläst format (B, KB, MB, GB).
 * @param bytes Storlek i bytes.
 * @param out Buffer för formaterad sträng.
 * @param out_sz Storlek på målbuffer.
 */
static void format_file_size(long bytes, char *out, size_t out_sz) {
    if (bytes < 0) {
        copy_str(out, out_sz, "");
    } else if (bytes < 1024) {
        snprintf(out, out_sz, "%ld B", bytes);
    } else if (bytes < 1024 * 1024) {
        snprintf(out, out_sz, "%.1f KB", (double)bytes / 1024.0);
    } else if (bytes < 1024L * 1024 * 1024) {
        snprintf(out, out_sz, "%.1f MB", (double)bytes / (1024.0 * 1024.0));
    } else {
        snprintf(out, out_sz, "%.1f GB", (double)bytes / (1024.0 * 1024.0 * 1024.0));
    }
}

/**
 * @brief Läser in undermappar och matchande filer i en angiven katalog.
 * @param dir_path Sökväg till katalogen som ska genomsökas.
 * @param filter_active 1 om ändelsefiltret ska tillämpas, 0 för alla filer.
 * @param ext_filter Filtersträng (t.ex. ".adi;.adif").
 * @param items_out Utdatapekare där dynamisk array allokeras.
 * @param count_out Utdatapekare för antal hittade element.
 * @return 0 vid framgång, -1 vid fel (t.ex. kunde inte öppna katalogen).
 */
static int load_dir_items(const char *dir_path, int filter_active, const char *ext_filter,
                          FileDialogItem **items_out, size_t *count_out) {
    *items_out = NULL;
    *count_out = 0;

    DIR *d = opendir(dir_path);
    if (d == NULL) return -1;

    size_t cap = 32;
    FileDialogItem *items = malloc(cap * sizeof(FileDialogItem));
    if (items == NULL) {
        closedir(d);
        return -1;
    }
    size_t count = 0;

    if (strcmp(dir_path, "/") != 0) {
        copy_str(items[count].name, sizeof(items[count].name), "..");
        items[count].is_dir = 1;
        items[count].size_bytes = -1;
        count++;
    }

    struct dirent *ent;
    while ((ent = readdir(d)) != NULL) {
        if (strcmp(ent->d_name, ".") == 0 || strcmp(ent->d_name, "..") == 0) {
            continue;
        }
        if (ent->d_name[0] == '.') {
            continue;
        }

        char full_path[PATH_MAX + 512];
        if (strcmp(dir_path, "/") == 0) {
            snprintf(full_path, sizeof(full_path), "/%s", ent->d_name);
        } else {
            snprintf(full_path, sizeof(full_path), "%s/%s", dir_path, ent->d_name);
        }

        struct stat st;
        if (stat(full_path, &st) != 0) {
            continue;
        }

        int is_dir = S_ISDIR(st.st_mode);
        int is_reg = S_ISREG(st.st_mode);

        if (!is_dir && !is_reg) {
            continue;
        }

        if (is_reg && filter_active && !matches_extension(ent->d_name, ext_filter)) {
            continue;
        }

        if (count >= cap) {
            size_t new_cap = cap * 2;
            FileDialogItem *new_items = realloc(items, new_cap * sizeof(FileDialogItem));
            if (new_items == NULL) break;
            items = new_items;
            cap = new_cap;
        }

        copy_str(items[count].name, sizeof(items[count].name), ent->d_name);
        items[count].is_dir = is_dir;
        items[count].size_bytes = is_dir ? -1 : (long)st.st_size;
        count++;
    }

    closedir(d);

    if (count > 1) {
        qsort(items, count, sizeof(FileDialogItem), cmp_file_dialog_item);
    }

    *items_out = items;
    *count_out = count;
    return 0;
}

/**
 * @brief Säkerställer att ett filnamn har ändelsen .adi om ändelse saknas.
 * @param filename Filnamn som kontrolleras och eventuellt kompletteras.
 * @param max_len Maximal storlek på bufferten.
 */
static void ensure_adif_extension(char *filename, size_t max_len) {
    if (filename == NULL || filename[0] == '\0') return;
    const char *base = get_path_basename(filename);
    const char *dot = strrchr(base, '.');
    if (dot == NULL || dot[1] == '\0') {
        if (dot != NULL && dot[1] == '\0') {
            strncat(filename, "adi", max_len - strlen(filename) - 1);
        } else {
            strncat(filename, ".adi", max_len - strlen(filename) - 1);
        }
    }
}

/**
 * @brief Kör en interaktiv fildialog i curses för att välja eller spara en fil.
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
                    char *selected_path, size_t max_path_len,
                    int is_save_mode, const char *default_filename) {
    char current_dir[PATH_MAX];
    if (start_dir != NULL && start_dir[0] != '\0') {
        if (realpath(start_dir, current_dir) == NULL) {
            if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
                copy_str(current_dir, sizeof(current_dir), ".");
            }
        }
    } else {
        if (getcwd(current_dir, sizeof(current_dir)) == NULL) {
            copy_str(current_dir, sizeof(current_dir), ".");
        }
    }

    int filter_active = (ext_filter != NULL && ext_filter[0] != '\0') ? 1 : 0;
    FileDialogItem *items = NULL;
    size_t count = 0;
    load_dir_items(current_dir, filter_active, ext_filter, &items, &count);

    int selected_pos = 0;
    int scroll_offset = 0;
    int focus_area = is_save_mode ? 0 : 1; /* 0 = filnamn, 1 = fillista */

    char fname_buf[256];
    if (default_filename != NULL && default_filename[0] != '\0') {
        copy_str(fname_buf, sizeof(fname_buf), default_filename);
    } else {
        copy_str(fname_buf, sizeof(fname_buf), "");
    }
    int fname_cursor_pos = utf8_display_width(fname_buf);

    while (1) {
        erase();
        int dialog_w = COLS - 6;
        if (dialog_w > 78) dialog_w = 78;
        if (dialog_w < 50) dialog_w = 50;
        int dialog_h = LINES - 4;
        if (dialog_h > 24) dialog_h = 24;
        if (dialog_h < 14) dialog_h = 14;

        int top_y = (LINES - dialog_h) / 2;
        int left_x = (COLS - dialog_w) / 2;

        attron(COLOR_PAIR(CP_MAIN_BG));
        for (int r = 0; r < dialog_h; ++r) {
            mvhline(top_y + r, left_x, ' ', dialog_w);
        }
        attroff(COLOR_PAIR(CP_MAIN_BG));

        draw_box_with_title(top_y, left_x, dialog_h, dialog_w, title, 1);

        /* Katalograd */
        attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
        mvprintw(top_y + 1, left_x + 2, "Katalog: ");
        attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

        int max_dir_disp = dialog_w - 14;
        int dir_len = (int)strlen(current_dir);
        if (dir_len > max_dir_disp && max_dir_disp > 3) {
            attron(COLOR_PAIR(CP_FIELD_INACTIVE));
            mvprintw(top_y + 1, left_x + 11, "...%s", current_dir + (dir_len - max_dir_disp + 3));
            attroff(COLOR_PAIR(CP_FIELD_INACTIVE));
        } else {
            attron(COLOR_PAIR(CP_FIELD_INACTIVE));
            mvprintw(top_y + 1, left_x + 11, "%-.*s", max_dir_disp, current_dir);
            attroff(COLOR_PAIR(CP_FIELD_INACTIVE));
        }

        int list_top_y = top_y + 2;
        int list_height = dialog_h - 4;
        int cur_input_y = -1, cur_input_x = -1;

        if (is_save_mode) {
            int fn_y = top_y + 2;
            attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
            mvprintw(fn_y, left_x + 2, "Filnamn: ");
            attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

            int fn_w = dialog_w - 14;
            int fn_col = left_x + 11;

            if (focus_area == 0) {
                attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                for (int i = 0; i < fn_w; ++i) mvaddch(fn_y, fn_col + i, ' ');
                int sc = 0;
                if (fname_cursor_pos >= fn_w) sc = fname_cursor_pos - fn_w + 1;
                size_t sb = utf8_char_to_byte(fname_buf, sc);
                size_t eb = utf8_char_to_byte(fname_buf, sc + fn_w);
                mvprintw(fn_y, fn_col, "%.*s", (int)(eb - sb), fname_buf + sb);
                attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                cur_input_y = fn_y;
                cur_input_x = fn_col + (fname_cursor_pos - sc);
            } else {
                attron(COLOR_PAIR(CP_FIELD_INACTIVE));
                for (int i = 0; i < fn_w; ++i) mvaddch(fn_y, fn_col + i, ' ');
                mvprintw(fn_y, fn_col, "%-.*s", fn_w, fname_buf);
                attroff(COLOR_PAIR(CP_FIELD_INACTIVE));
            }

            list_top_y = top_y + 4;
            list_height = dialog_h - 6;
            attron(COLOR_PAIR(CP_BORDER));
            mvhline(top_y + 3, left_x + 1, ACS_HLINE, dialog_w - 2);
            attroff(COLOR_PAIR(CP_BORDER));
        }

        int list_avail_rows = list_height;
        if (list_avail_rows < 1) list_avail_rows = 1;

        if (count > 0) {
            if (selected_pos < scroll_offset) {
                scroll_offset = selected_pos;
            } else if (selected_pos >= scroll_offset + list_avail_rows) {
                scroll_offset = selected_pos - list_avail_rows + 1;
            }
            if (scroll_offset < 0) scroll_offset = 0;
        } else {
            scroll_offset = 0;
        }

        for (int i = 0; i < list_avail_rows; ++i) {
            int idx = scroll_offset + i;
            int r = list_top_y + i;
            if (idx < (int)count) {
                const FileDialogItem *item = &items[idx];
                int is_sel = (focus_area == 1 && idx == selected_pos);

                if (is_sel) {
                    attron(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
                    mvhline(r, left_x + 1, ' ', dialog_w - 2);
                    mvaddch(r, left_x + 1, '>');
                } else {
                    attron(COLOR_PAIR(CP_LOG_ROW));
                    mvhline(r, left_x + 1, ' ', dialog_w - 2);
                }

                char sz_str[32] = "";
                if (!item->is_dir) {
                    format_file_size(item->size_bytes, sz_str, sizeof(sz_str));
                }

                int name_max = dialog_w - 18;
                if (item->is_dir) {
                    mvprintw(r, left_x + 3, "[%-.*s]", name_max, item->name);
                    mvprintw(r, left_x + dialog_w - 10, "<Mapp>");
                } else {
                    mvprintw(r, left_x + 3, "%-.*s", name_max, item->name);
                    mvprintw(r, left_x + dialog_w - 12, "%10s", sz_str);
                }

                if (is_sel) {
                    attroff(COLOR_PAIR(CP_LOG_SELECTED) | A_BOLD);
                    if (cur_input_y < 0) {
                        cur_input_y = r;
                        cur_input_x = left_x + 2;
                    }
                } else {
                    attroff(COLOR_PAIR(CP_LOG_ROW));
                }
            } else {
                if (count == 0 && i == 0) {
                    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
                    mvprintw(r, left_x + 3, "(Katalogen är tom eller matchar inte filtret)");
                    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);
                }
            }
        }

        int bot_y = top_y + dialog_h - 2;
        attron(COLOR_PAIR(CP_BORDER));
        mvhline(bot_y, left_x + 1, ACS_HLINE, dialog_w - 2);
        attroff(COLOR_PAIR(CP_BORDER));

        attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
        if (is_save_mode) {
            mvprintw(bot_y + 1, left_x + 2, "Tab/Pilar=Växla fält  Enter=Spara  Esc=Avbryt");
        } else {
            mvprintw(bot_y + 1, left_x + 2, "Pilar=Bläddra  Enter=Välj  Backspace=Uppåt  Esc=Avbryt");
        }
        if (ext_filter != NULL && ext_filter[0] != '\0') {
            int fw_col = left_x + dialog_w - (int)strlen(ext_filter) - 11;
            if (fw_col > left_x + 40) {
                attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
                mvprintw(bot_y + 1, fw_col, "[Filter: %s]", ext_filter);
                attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
            }
        }
        attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

        if (cur_input_y >= 0 && cur_input_x >= 0) {
            move(cur_input_y, cur_input_x);
        }
        refresh();

        int ch = getch();
        if (ch == ERR) continue;

        if (ch == 27) {
            if (items) free(items);
            return 0;
        }

        if (is_save_mode && (ch == '\t' || (focus_area == 0 && (ch == KEY_DOWN)))) {
            focus_area = 1;
            continue;
        } else if (is_save_mode && (ch == KEY_BTAB || (focus_area == 1 && (ch == KEY_UP && selected_pos == 0)))) {
            focus_area = 0;
            continue;
        }

        if (focus_area == 0 && is_save_mode) {
            if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
                trim_str(fname_buf);
                if (fname_buf[0] != '\0') {
                    ensure_adif_extension(fname_buf, sizeof(fname_buf));
                    if (fname_buf[0] == '/') {
                        copy_str(selected_path, max_path_len, fname_buf);
                    } else if (strcmp(current_dir, "/") == 0) {
                        snprintf(selected_path, max_path_len, "/%s", fname_buf);
                    } else {
                        snprintf(selected_path, max_path_len, "%s/%s", current_dir, fname_buf);
                    }
                    if (items) free(items);
                    return 1;
                }
            } else if (ch == KEY_LEFT) {
                if (fname_cursor_pos > 0) fname_cursor_pos--;
            } else if (ch == KEY_RIGHT) {
                int total_c = utf8_display_width(fname_buf);
                if (fname_cursor_pos < total_c) fname_cursor_pos++;
            } else if (ch == KEY_HOME) {
                fname_cursor_pos = 0;
            } else if (ch == KEY_END) {
                fname_cursor_pos = utf8_display_width(fname_buf);
            } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
                utf8_backspace_at(fname_buf, &fname_cursor_pos);
            } else if (ch == KEY_DC) {
                fname_buf[0] = '\0';
                fname_cursor_pos = 0;
            } else if (ch >= 32 && ch != 127 && ch < KEY_MIN) {
                utf8_insert_byte_at(fname_buf, sizeof(fname_buf), &fname_cursor_pos, (char)ch);
            }
        } else {
            if (ch == KEY_UP) {
                if (selected_pos > 0) {
                    selected_pos--;
                } else if (is_save_mode) {
                    focus_area = 0;
                }
            } else if (ch == KEY_DOWN) {
                if (selected_pos + 1 < (int)count) {
                    selected_pos++;
                }
            } else if (ch == KEY_PPAGE) {
                selected_pos -= list_avail_rows;
                if (selected_pos < 0) selected_pos = 0;
            } else if (ch == KEY_NPAGE) {
                selected_pos += list_avail_rows;
                if (selected_pos >= (int)count) selected_pos = count > 0 ? (int)count - 1 : 0;
            } else if (ch == KEY_HOME) {
                selected_pos = 0;
            } else if (ch == KEY_END) {
                selected_pos = count > 0 ? (int)count - 1 : 0;
            } else if (ch == KEY_BACKSPACE || ch == 127 || ch == '\b') {
                char parent_dir[PATH_MAX + 16];
                if (strcmp(current_dir, "/") != 0) {
                    snprintf(parent_dir, sizeof(parent_dir), "%s/..", current_dir);
                    char resolved[PATH_MAX];
                    if (realpath(parent_dir, resolved) != NULL) {
                        copy_str(current_dir, sizeof(current_dir), resolved);
                        if (items) free(items);
                        items = NULL;
                        count = 0;
                        load_dir_items(current_dir, filter_active, ext_filter, &items, &count);
                        selected_pos = 0;
                        scroll_offset = 0;
                    }
                }
            } else if (ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
                if (count > 0 && selected_pos >= 0 && selected_pos < (int)count) {
                    const FileDialogItem *sel = &items[selected_pos];
                    if (sel->is_dir) {
                        char next_dir[PATH_MAX + 512];
                        if (strcmp(sel->name, "..") == 0) {
                            snprintf(next_dir, sizeof(next_dir), "%s/..", current_dir);
                        } else if (strcmp(current_dir, "/") == 0) {
                            snprintf(next_dir, sizeof(next_dir), "/%s", sel->name);
                        } else {
                            snprintf(next_dir, sizeof(next_dir), "%s/%s", current_dir, sel->name);
                        }
                        char resolved[PATH_MAX];
                        if (realpath(next_dir, resolved) != NULL) {
                            copy_str(current_dir, sizeof(current_dir), resolved);
                            if (items) free(items);
                            items = NULL;
                            count = 0;
                            load_dir_items(current_dir, filter_active, ext_filter, &items, &count);
                            selected_pos = 0;
                            scroll_offset = 0;
                        }
                    } else if (is_save_mode) {
                        copy_str(fname_buf, sizeof(fname_buf), sel->name);
                        fname_cursor_pos = utf8_display_width(fname_buf);
                        focus_area = 0;
                    } else {
                        if (strcmp(current_dir, "/") == 0) {
                            snprintf(selected_path, max_path_len, "/%s", sel->name);
                        } else {
                            snprintf(selected_path, max_path_len, "%s/%s", current_dir, sel->name);
                        }
                        if (items) free(items);
                        return 1;
                    }
                }
            }
        }
    }
}
