/**
 * @file help.c
 * @brief Implementering av den interaktiva hjälpdialogen (F9) i UEI-Logg.
 *
 * Visar omfattande hjälp indelad i sex informativa flikar:
 * 1. Globala snabbkommandon
 * 2. QSO-inmatning & markör/överskrivningsregler
 * 3. Uppslagsfunktioner (Logg -> SM Callbook -> QRZ.COM)
 * 4. Loggrader, redigering och statusmarkering
 * 5. Inställningar, fildialog och ADIF import/export
 * 6. Rymdväder, K-index och HF-konditioner
 */

#define _XOPEN_SOURCE 700
#include "help.h"
#include "tui.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <curses.h>

/**
 * @brief Hjälpsektion / flik.
 */
typedef struct {
    const char *tab_name;
    const char *title;
    const char *lines[40];
    int line_count;
} HelpTopic;

static const HelpTopic HELP_TOPICS[] = {
    {
        "[1] Tangenter",
        "Globala tangentbordskommandon",
        {
            "F1           - Rensa inmatningsformulär & återstarta UTC-klockan",
            "F2 el. Enter - Logga och spara pågående QSO till loggboken",
            "F3 el. ^F    - Sök och filtrera rader i logglistan i realtid",
            "F4 el. ^O    - Växla tangentbordsfokus mellan inmatningsfält och loggrader",
            "F5           - Manuellt uppslag (Lokal logg -> SM Callbook -> QRZ.COM)",
            "F6           - Manuellt uppslag direkt via QRZ.COM",
            "F9           - Denna hjälpdialog (tillgänglig i logg- och inställningsskärm)",
            "F10 el. ^Q   - Avsluta UEI-Logg på ett säkert sätt",
            "F12          - Stationsinställningar, ADIF-filhantering & färgscheman",
            "ESC          - Återställ aktiv sökning, lämna loggläge eller stäng fönster",
            "",
            "Navigering:",
            "  Tab / Shift-Tab - Stega framåt / bakåt mellan inmatningsfälten",
            "  Piltangenter    - Flytta markör / stega vertikalt mellan fälten",
            "  Home / End      - Hoppa till första (Signal) / sista (Notering) fältet",
            "  PgUp / PgDown   - Bläddra en hel sida i logglistan",
            NULL
        },
        15
    },
    {
        "[2] Inmatning",
        "QSO-inmatning & Formulärregler",
        {
            "Markör och överskrivning:",
            "  * När ett fält väljs ställs markören alltid på det första tecknet (pos 0).",
            "  * När du skriver skrivs befintliga tecken i fältet över.",
            "  * Backspace raderar tecknet till vänster, Delete raderar tecknet under markören.",
            "",
            "Fältordning och format (4 rader):",
            "  * Rad 1: Signal (10), RST T (6), RST R (6), Namn (14)",
            "  * Rad 2: QTH (14), Lokator (6), QRB (avstånd), DXCC (land)",
            "  * Rad 3: Frekvens (MHz), Band (dropdown), Mode (t.ex. SSB/CW/FT8)",
            "  * Rad 4: Datum (YYYY-MM-DD), UTC (HH:MM), Notering (resten av raden)",
            "",
            "Smarta funktioner:",
            "  * Klockan uppdateras live varje sekund tills inmatning påbörjas i ett fält.",
            "  * Frekvens i MHz (t.ex. 14.074 eller 14074) identifierar automatiskt amatörbandet.",
            "  * I Band-fältet: tryck Enter för interaktiv drop-down, eller 0-9 för snabbval.",
            "  * QRB (avstånd) beräknas kontinuerligt mot egen lokator i inställningarna.",
            "  * Auto-RST: sätter automatiskt 59 (telefoni/digi) eller 599 (CW) om aktiverat.",
            NULL
        },
        17
    },
    {
        "[3] Uppslag",
        "Uppslagsfunktioner & Prioritetsordning",
        {
            "Prioritetsordning vid uppslag av anropssignal:",
            "  1. Den lokala loggboken (qsolog.adi) kontrolleras alltid först.",
            "  2. Om ej i loggen och anropssignalen är svensk: SM Callbook (om aktiverat).",
            "  3. Om SMCB saknar träff eller om signalen är utländsk: QRZ.COM (om aktiverat).",
            "",
            "Automatiskt uppslag:",
            "  * Utförs i bakgrunden utan att blockera programmet när markören lämnar Signal,",
            "    eller automatiskt efter ca 1 sekunds inaktivitet på Signal-fältet.",
            "  * Information om Namn, QTH och Lokator fylls automatiskt i vid träff.",
            "",
            "Manuellt uppslag:",
            "  * F5: Slår upp direkt mot aktiv tjänst (Lokal logg -> SMCB -> QRZ).",
            "        Skriver över eventuellt befintliga fält med de nya uppgifterna.",
            "  * F6: Slår upp anropssignalen direkt mot QRZ.COM oavsett nationalitet.",
            "",
            "Konfiguration (F12):",
            "  * Både SM Callbook och QRZ.COM kan aktiveras eller stängas av individuellt.",
            "  * För QRZ.COM krävs giltigt användarnamn och lösenord.",
            NULL
        },
        19
    },
    {
        "[4] Logghantering",
        "Loggrader, Redigering & Markering",
        {
            "Fokus och navigering:",
            "  * Tryck F4 (eller Ctrl+O) för att växla mellan formulär och loggrader.",
            "  * Piltangenterna Upp / Ned stegar radvis i loggen.",
            "  * Page Up / Page Down bläddrar hela sidor i logghistoriken.",
            "",
            "Multimarkering och statusändring:",
            "  * Insert (INS): Markerar eller avmarkerar aktuell loggrad.",
            "  * F5: Sätter status som Hanterad (visas med ett grönt bockmärke ✓).",
            "  * F6: Sätter status som Ny (visas med ett kryss ✗).",
            "  * Om rader är markerade med INS påverkas alla markerade rader samtidigt.",
            "",
            "Redigering och radering:",
            "  * Enter på en rad: Laddar upp QSO:t i formuläret för direkt korrigering.",
            "    När du trycker Enter i formuläret sparas ändringen på samma plats.",
            "  * F8: Raderar markerade rader (eller vald rad) efter bekräftelse.",
            "",
            "Sök och filtrering (F3 / Ctrl+F):",
            "  * Filtrera rader på anropssignal, namn, QTH, frekvens, band eller mode.",
            "  * Tryck ESC för att återställa sökningen och visa hela loggboken igen.",
            NULL
        },
        21
    },
    {
        "[5] Inställningar",
        "Konfiguration, Fildialog & ADIF (F12)",
        {
            "Stationsinställningar (station.conf):",
            "  * Min signal, Operatörsnamn, QTH och Lokator (6 tecken, t.ex. JO89VJ).",
            "  * Lokal tidszon (t.ex. Europe/Stockholm) för korrekt tidsvisning i listen.",
            "  * Inloggningsuppgifter för QRZ.COM (lösenord maskeras med **********).",
            "  * På/Av för automatiskt uppslag mot SM Callbook och QRZ.COM.",
            "  * Färgscheman: välj mellan Mörkblå, Amber Retro, Grön CRT, Matrix m.fl.",
            "",
            "ADIF Import (F5 i inställningar):",
            "  * Interaktiv katalogbläddrare med filter för .adi och .adif-filer.",
            "  * Importerar kompletta QSO-fält med automatisk dubblettkontroll.",
            "",
            "ADIF Export (F6 i inställningar):",
            "  * Exportera alla rader, endast markerade rader, eller endast nya (ohanterade).",
            "  * Skapar standardiserade ADIF-filer med automatiska tidsstämplade namn.",
            "",
            "Töm loggbok:",
            "  * Möjlighet att rensa loggboken inför nya tävlingar eller tester (med bekräftelse).",
            NULL
        },
        19
    },
    {
        "[6] Rymdväder",
        "Rymdväder & HF-Konditioner (hf.avitec.se)",
        {
            "Realtidsdata hämtas asynkront i bakgrunden utan fördröjning:",
            "",
            "Geomagnetiskt K-index (Kiruna):",
            "  * 0 - 2 (Grönt):  Lugna geomagnetiska förhållanden. Utmärkta HF-signaler.",
            "  * 3 - 4 (Gult):   Oroliga förhållanden. Risk för fading och dämpning i norr.",
            "  * 5 - 9 (Rött):   Geomagnetisk storm! Kraftig dämpning av HF-vågor och absorption.",
            "",
            "MUF och LUF (Maximal & Lägsta användbara frekvens):",
            "  * MUF anger den högsta frekvens som jonosfären reflekterar.",
            "  * Frekvenser över MUF passerar ut i rymden; frekvenser under LUF dämpas ut.",
            "",
            "CME & Solvind:",
            "  * CME (Coronal Mass Ejection): Solstormar som slungas ut mot jorden.",
            "  * Solvindshastighet (km/s): Normalt 300-400 km/s. Vid >500 km/s ökar K-index.",
            "",
            "Aurora / Polarsken:",
            "  * Vid förhöjd aurora-aktivitet uppstår möjlighet till norrskensstuds på 6m och 2m.",
            NULL
        },
        18
    }
};

static const int NUM_HELP_TOPICS = (int)(sizeof(HELP_TOPICS) / sizeof(HELP_TOPICS[0]));

/**
 * @brief Visar och kör den interaktiva hjälpdialogen (F9).
 * @param st Min stations inställningar.
 */
void run_help_modal(const StationSettings *st) {
    (void)st;
    int current_topic = 0;
    int scroll_offset = 0;

    while (1) {
        erase();
        int dialog_w = COLS - 4;
        if (dialog_w > 86) dialog_w = 86;
        if (dialog_w < 56) dialog_w = 56;

        int dialog_h = LINES - 2;
        if (dialog_h > 26) dialog_h = 26;
        if (dialog_h < 16) dialog_h = 16;

        int top_y = (LINES - dialog_h) / 2;
        int left_x = (COLS - dialog_w) / 2;
        if (top_y < 0) top_y = 0;
        if (left_x < 0) left_x = 0;

        /* Bakgrundsfyllning */
        attron(COLOR_PAIR(CP_MAIN_BG));
        for (int r = 0; r < dialog_h; ++r) {
            mvhline(top_y + r, left_x, ' ', dialog_w);
        }
        attroff(COLOR_PAIR(CP_MAIN_BG));

        /* Fönsterram med rubrik */
        draw_box_with_title(top_y, left_x, dialog_h, dialog_w, "Hjälp & Dokumentation (F9)", 1);

        /* Flikrad högst upp */
        int tab_y = top_y + 1;
        int tab_x = left_x + 2;
        for (int t = 0; t < NUM_HELP_TOPICS; ++t) {
            const char *tname = HELP_TOPICS[t].tab_name;
            int tlen = (int)strlen(tname);
            if (tab_x + tlen + 2 > left_x + dialog_w - 2) break;

            if (t == current_topic) {
                attron(COLOR_PAIR(CP_TOPBAR) | A_BOLD);
                mvprintw(tab_y, tab_x, " %s ", tname);
                attroff(COLOR_PAIR(CP_TOPBAR) | A_BOLD);
            } else {
                attron(COLOR_PAIR(CP_FIELD_INACTIVE));
                mvprintw(tab_y, tab_x, " %s ", tname);
                attroff(COLOR_PAIR(CP_FIELD_INACTIVE));
            }
            tab_x += tlen + 3;
        }

        /* Avgränsningslinje under flikarna */
        attron(COLOR_PAIR(CP_BORDER));
        mvhline(top_y + 2, left_x + 1, ACS_HLINE, dialog_w - 2);
        attroff(COLOR_PAIR(CP_BORDER));

        /* Avsnittets underrubrik */
        attron(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);
        mvprintw(top_y + 3, left_x + 3, "%s", HELP_TOPICS[current_topic].title);
        attroff(COLOR_PAIR(CP_STATUS_WARN) | A_BOLD);

        /* Textinnehåll */
        int avail_text_rows = dialog_h - 7;
        if (avail_text_rows < 1) avail_text_rows = 1;

        const HelpTopic *topic = &HELP_TOPICS[current_topic];
        int total_lines = topic->line_count;

        if (scroll_offset + avail_text_rows > total_lines) {
            scroll_offset = total_lines - avail_text_rows;
        }
        if (scroll_offset < 0) scroll_offset = 0;

        for (int i = 0; i < avail_text_rows; ++i) {
            int line_idx = scroll_offset + i;
            int row_y = top_y + 4 + i;
            if (line_idx < total_lines && topic->lines[line_idx] != NULL) {
                const char *line = topic->lines[line_idx];
                if (line[0] != '\0' && (line[0] != ' ' && line[0] != '*')) {
                    attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
                    mvprintw(row_y, left_x + 3, "%-.*s", dialog_w - 6, line);
                    attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);
                } else if (line[0] == ' ' && line[2] == '*') {
                    attron(COLOR_PAIR(CP_LOG_ROW));
                    mvprintw(row_y, left_x + 3, "%-.*s", dialog_w - 6, line);
                    attroff(COLOR_PAIR(CP_LOG_ROW));
                } else {
                    attron(COLOR_PAIR(CP_LOG_ROW));
                    mvprintw(row_y, left_x + 3, "%-.*s", dialog_w - 6, line);
                    attroff(COLOR_PAIR(CP_LOG_ROW));
                }
            }
        }

        /* Avgränsningslinje över bottenraden */
        int bot_line_y = top_y + dialog_h - 3;
        attron(COLOR_PAIR(CP_BORDER));
        mvhline(bot_line_y, left_x + 1, ACS_HLINE, dialog_w - 2);
        attroff(COLOR_PAIR(CP_BORDER));

        /* Navigeringsinstruktioner längst ned */
        attron(COLOR_PAIR(CP_LABEL) | A_BOLD);
        mvprintw(bot_line_y + 1, left_x + 3, "1-6 / Pilar V-H: Byt flik   Pilar U-N / PgUp-Dn: Rulla   Esc / Enter: Stäng");
        attroff(COLOR_PAIR(CP_LABEL) | A_BOLD);

        /* Rullningsindikator om innehållet är längre än fönstret */
        if (total_lines > avail_text_rows) {
            int pct = (int)((double)(scroll_offset + avail_text_rows) / (double)total_lines * 100.0);
            if (pct > 100) pct = 100;
            attron(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
            mvprintw(bot_line_y + 1, left_x + dialog_w - 12, "[%3d%%]", pct);
            attroff(COLOR_PAIR(CP_FIELD_ACTIVE) | A_BOLD);
        }

        refresh();

        int ch = getch();
        if (ch == ERR) continue;

        if (ch == 27 || ch == KEY_F(9) || ch == 'q' || ch == 'Q' || ch == '\n' || ch == '\r' || ch == KEY_ENTER) {
            break;
        } else if (ch >= '1' && ch <= '0' + NUM_HELP_TOPICS) {
            current_topic = ch - '1';
            scroll_offset = 0;
        } else if (ch == KEY_LEFT || ch == KEY_BTAB) {
            current_topic = (current_topic + NUM_HELP_TOPICS - 1) % NUM_HELP_TOPICS;
            scroll_offset = 0;
        } else if (ch == KEY_RIGHT || ch == '\t') {
            current_topic = (current_topic + 1) % NUM_HELP_TOPICS;
            scroll_offset = 0;
        } else if (ch == KEY_UP) {
            if (scroll_offset > 0) scroll_offset--;
        } else if (ch == KEY_DOWN) {
            if (scroll_offset + avail_text_rows < total_lines) scroll_offset++;
        } else if (ch == KEY_PPAGE) {
            scroll_offset -= avail_text_rows;
            if (scroll_offset < 0) scroll_offset = 0;
        } else if (ch == KEY_NPAGE) {
            scroll_offset += avail_text_rows;
            if (scroll_offset + avail_text_rows > total_lines) {
                scroll_offset = total_lines - avail_text_rows;
            }
            if (scroll_offset < 0) scroll_offset = 0;
        } else if (ch == KEY_HOME) {
            scroll_offset = 0;
        } else if (ch == KEY_END) {
            scroll_offset = total_lines - avail_text_rows;
            if (scroll_offset < 0) scroll_offset = 0;
        }
    }
}
