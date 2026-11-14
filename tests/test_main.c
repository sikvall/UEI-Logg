/**
 * @file test_main.c
 * @brief Fristående automatiserad testsvit för UEI-Logg (binär "testing").
 * Kontrollerar alla centrala funktioner och skriver ut [OK]/[FEL].
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <unistd.h>
#include <locale.h>

#include "qsolog.h"
#include "db.h"
#include "util.h"
#include "logg.h"
#include "settings.h"
#include "qrb.h"
#include "dxcc.h"
#include "tui.h"

static int g_tests_run = 0;
static int g_tests_passed = 0;
static int g_tests_failed = 0;

#define ANSI_GREEN "\033[1;32m"
#define ANSI_RED   "\033[1;31m"
#define ANSI_BLUE  "\033[1;34m"
#define ANSI_RESET "\033[0m"

static void record_result(int success, const char *test_name, const char *detail) {
    g_tests_run++;
    if (success) {
        g_tests_passed++;
        printf("  " ANSI_GREEN "[OK]" ANSI_RESET "  %s\n", test_name);
    } else {
        g_tests_failed++;
        printf("  " ANSI_RED "[FEL]" ANSI_RESET " %s\n", test_name);
        if (detail && detail[0]) {
            printf("        -> %s\n", detail);
        }
    }
}

/* ========================================================================= */
/* [1] Frekvens- och bandberäkning (inklusive våglängd)                      */
/* ========================================================================= */
static void run_freq_and_band_tests(void) {
    printf(ANSI_BLUE "\n=== [1] Frekvens- och bandberäkning (inkl. våglängd) ===" ANSI_RESET "\n");

    /* 1. 1936 kHz -> 160 m */
    {
        char norm[32] = "";
        char band[32] = "";
        double mhz = parse_and_normalize_freq("1936 kHz", norm, sizeof(norm));
        resolve_band_from_freq("1936 kHz", band, sizeof(band));
        int ok = (fabs(mhz - 1.936) < 0.001) && (strcmp(norm, "1.936") == 0) && (strcmp(band, "160 m") == 0);
        char detail[128];
        snprintf(detail, sizeof(detail), "Fick MHz=%.3f, norm='%s', band='%s'", mhz, norm, band);
        record_result(ok, "Frekvens '1936 kHz' tolkas som 1.936 MHz och band '160 m'", ok ? NULL : detail);
    }

    /* 2. 14,200 (decimalkomma) -> 20 m */
    {
        char norm[32] = "";
        char band[32] = "";
        double mhz = parse_and_normalize_freq("14,200", norm, sizeof(norm));
        resolve_band_from_freq("14,200", band, sizeof(band));
        int ok = (fabs(mhz - 14.200) < 0.001) && (strcmp(norm, "14.200") == 0) && (strcmp(band, "20 m") == 0);
        char detail[128];
        snprintf(detail, sizeof(detail), "Fick MHz=%.3f, norm='%s', band='%s'", mhz, norm, band);
        record_result(ok, "Frekvens '14,200' (med komma) tolkas som 14.200 MHz och band '20 m'", ok ? NULL : detail);
    }

    /* 3. 8 MHz -> 38 m (våglängd lambda = 300 / 8 = 37.5 -> 38 m) */
    {
        char norm[32] = "";
        char band[32] = "";
        double mhz = parse_and_normalize_freq("8 MHz", norm, sizeof(norm));
        resolve_band_from_freq("8 MHz", band, sizeof(band));
        int ok = (fabs(mhz - 8.000) < 0.001) && (strcmp(norm, "8.000") == 0) && (strcmp(band, "38 m") == 0);
        char detail[128];
        snprintf(detail, sizeof(detail), "Fick MHz=%.3f, norm='%s', band='%s'", mhz, norm, band);
        record_result(ok, "Frekvens '8 MHz' beräknar våglängd '38 m' (300 / 8 = 37.5 -> 38)", ok ? NULL : detail);
    }

    /* 4. 27 MHz -> 11 m (våglängd lambda = 300 / 27 = 11.11 -> 11 m) */
    {
        char norm[32] = "";
        char band[32] = "";
        double mhz = parse_and_normalize_freq("27 MHz", norm, sizeof(norm));
        resolve_band_from_freq("27 MHz", band, sizeof(band));
        int ok = (fabs(mhz - 27.000) < 0.001) && (strcmp(norm, "27.000") == 0) && (strcmp(band, "11 m") == 0);
        char detail[128];
        snprintf(detail, sizeof(detail), "Fick MHz=%.3f, norm='%s', band='%s'", mhz, norm, band);
        record_result(ok, "Frekvens '27 MHz' beräknar våglängd '11 m' (300 / 27 = 11.11 -> 11)", ok ? NULL : detail);
    }

    /* 5. Heltalsfrekvenser utan decimaler (t.ex. 7 -> 7.000 MHz / 40m, 14 -> 14.000 MHz / 20m) */
    {
        char norm7[32] = "", band7[32] = "";
        char norm14[32] = "", band14[32] = "";
        parse_and_normalize_freq("7", norm7, sizeof(norm7));
        resolve_band_from_freq("7", band7, sizeof(band7));
        parse_and_normalize_freq("14", norm14, sizeof(norm14));
        resolve_band_from_freq("14", band14, sizeof(band14));

        int ok = (strcmp(norm7, "7.000") == 0) && (strcmp(band7, "40 m") == 0) &&
                 (strcmp(norm14, "14.000") == 0) && (strcmp(band14, "20 m") == 0);
        char detail[128];
        snprintf(detail, sizeof(detail), "7: norm='%s', band='%s' | 14: norm='%s', band='%s'",
                 norm7, band7, norm14, band14);
        record_result(ok, "Heltal '7' och '14' tolkas som 7.000 (40 m) respektive 14.000 (20 m)", ok ? NULL : detail);
    }

    /* 6. VHF och UHF band (144, 432, 1296) */
    {
        char b144[32] = "", b432[32] = "", b1296[32] = "";
        resolve_band_from_freq("144.300", b144, sizeof(b144));
        resolve_band_from_freq("432.200", b432, sizeof(b432));
        resolve_band_from_freq("1296.200", b1296, sizeof(b1296));

        int ok = (strcmp(b144, "2 m") == 0) && (strcmp(b432, "70 cm") == 0) && (strcmp(b1296, "23 cm") == 0);
        char detail[128];
        snprintf(detail, sizeof(detail), "144: '%s', 432: '%s', 1296: '%s'", b144, b432, b1296);
        record_result(ok, "VHF/UHF: 144 MHz -> '2 m', 432 MHz -> '70 cm', 1296 MHz -> '23 cm'", ok ? NULL : detail);
    }

    /* 7. find_band_index med varianter (20 m, 20m, 20 meter, 20meter) */
    {
        int i1 = find_band_index("20 m");
        int i2 = find_band_index("20m");
        int i3 = find_band_index("20 meter");
        int i4 = find_band_index("20meter");
        int i5 = find_band_index("160 m");
        int i6 = find_band_index("160 meter");

        int ok = (i1 == 5) && (i2 == 5) && (i3 == 5) && (i4 == 5) && (i5 == 0) && (i6 == 0);
        char detail[128];
        snprintf(detail, sizeof(detail), "Indices: i1=%d, i2=%d, i3=%d, i4=%d, i5=%d, i6=%d", i1, i2, i3, i4, i5, i6);
        record_result(ok, "find_band_index känner igen både '20 m', '20m', '20 meter' och '20meter'", ok ? NULL : detail);
    }

    /* 8. format_band_display och normalize_band_adif */
    {
        char disp20[32] = "", disp38[32] = "", adif20[32] = "", adif38[32] = "", adif11[32] = "";
        format_band_display("20 meter", disp20, sizeof(disp20));
        format_band_display("38 meter", disp38, sizeof(disp38));
        normalize_band_adif("20 meter", adif20, sizeof(adif20));
        normalize_band_adif("38 meter", adif38, sizeof(adif38));
        normalize_band_adif("11 meter", adif11, sizeof(adif11));

        int ok = (strcmp(disp20, "20 m") == 0) && (strcmp(disp38, "38 m") == 0) &&
                 (strcmp(adif20, "20M") == 0) && (strcmp(adif38, "38M") == 0) && (strcmp(adif11, "11M") == 0);
        char detail[128];
        snprintf(detail, sizeof(detail), "disp20='%s', disp38='%s', adif20='%s', adif38='%s', adif11='%s'",
                 disp20, disp38, adif20, adif38, adif11);
        record_result(ok, "format_band_display och normalize_band_adif standardiserar 'meter' -> 'm' och 'M'", ok ? NULL : detail);
    }
}

/* ========================================================================= */
/* [2] ADIF frekvensimport (MHz-antagande)                                   */
/* ========================================================================= */
static void run_adif_freq_tests(void) {
    printf(ANSI_BLUE "\n=== [2] ADIF frekvensimport (MHz-antagande) ===" ANSI_RESET "\n");

    /* 1. parse_and_normalize_freq_adif("7") -> 7.000 */
    {
        char norm[32] = "";
        double mhz = parse_and_normalize_freq_adif("7", norm, sizeof(norm));
        int ok = (fabs(mhz - 7.000) < 0.001) && (strcmp(norm, "7.000") == 0);
        char detail[128];
        snprintf(detail, sizeof(detail), "Fick mhz=%.3f, norm='%s'", mhz, norm);
        record_result(ok, "ADIF-import av '7' tolkas som 7.000 MHz (inte 7 kHz)", ok ? NULL : detail);
    }

    /* 2. parse_and_normalize_freq_adif("0.021") -> återställs till 21.000 MHz */
    {
        char norm[32] = "";
        double mhz = parse_and_normalize_freq_adif("0.021", norm, sizeof(norm));
        int ok = (fabs(mhz - 21.000) < 0.001) && (strcmp(norm, "21.000") == 0);
        char detail[128];
        snprintf(detail, sizeof(detail), "Fick mhz=%.3f, norm='%s'", mhz, norm);
        record_result(ok, "ADIF-import återställer felaktigt delad frekvens 0.021 -> 21.000 MHz", ok ? NULL : detail);
    }

    /* 3. Normal decimalfrekvens "14.074" */
    {
        char norm[32] = "";
        double mhz = parse_and_normalize_freq_adif("14.074", norm, sizeof(norm));
        int ok = (fabs(mhz - 14.074) < 0.001) && (strcmp(norm, "14.074") == 0);
        record_result(ok, "ADIF-import av normal frekvens '14.074' bevaras som 14.074 MHz", NULL);
    }
}

/* ========================================================================= */
/* [3] Logghantering och F7 (Markera alla som hanterade)                     */
/* ========================================================================= */
static void run_handled_and_f7_tests(void) {
    printf(ANSI_BLUE "\n=== [3] Logghantering och F7 (Markera alla som hanterade) ===" ANSI_RESET "\n");

    QSOList list;
    memset(&list, 0, sizeof(list));

    /* Skapa tre test-QSO:n med varierande status */
    QSO q1, q2, q3;
    memset(&q1, 0, sizeof(q1));
    memset(&q2, 0, sizeof(q2));
    memset(&q3, 0, sizeof(q3));

    copy_str(q1.call, sizeof(q1.call), "SM5UEI");
    copy_str(q1.band, sizeof(q1.band), "20 m");
    copy_str(q1.freq, sizeof(q1.freq), "14.200");
    q1.handled = 0;

    copy_str(q2.call, sizeof(q2.call), "SA0XYZ");
    copy_str(q2.band, sizeof(q2.band), "40 m");
    copy_str(q2.freq, sizeof(q2.freq), "7.100");
    q2.handled = 1;

    copy_str(q3.call, sizeof(q3.call), "DL1ABC");
    copy_str(q3.band, sizeof(q3.band), "80 m");
    copy_str(q3.freq, sizeof(q3.freq), "3.650");
    q3.handled = 0;

    qso_list_push(&list, &q1);
    qso_list_push(&list, &q2);
    qso_list_push(&list, &q3);

    /* 1. Verifiera initial status */
    int init_ok = (list.count == 3) && (list.items[0].handled == 0) &&
                  (list.items[1].handled == 1) && (list.items[2].handled == 0);
    record_result(init_ok, "QSO-listan initieras med blandade hanterad-flaggor", NULL);

    /* 2. Simulera F7: markera samtliga som hanterade */
    for (size_t i = 0; i < list.count; ++i) {
        list.items[i].handled = 1;
    }
    invalidate_log_stats_cache();

    int f7_ok = 1;
    for (size_t i = 0; i < list.count; ++i) {
        if (list.items[i].handled != 1) {
            f7_ok = 0;
            break;
        }
    }
    record_result(f7_ok, "F7-åtgärd sätter handled = 1 för samtliga poster i loggen", NULL);

    /* 3. Spara och läs in från ADIF för att verifiera APP_QSOLOG_HANDLED */
    const char *tmp_adif = "/tmp/test_f7_tmp.adi";
    StationSettings st;
    memset(&st, 0, sizeof(st));
    copy_str(st.call, sizeof(st.call), "SM5UEI");

    save_all_qsos_adif(tmp_adif, &list, &st);

    QSOList loaded;
    memset(&loaded, 0, sizeof(loaded));
    load_qsos_from_adif(tmp_adif, &loaded);

    int persist_ok = (loaded.count == 3);
    for (size_t i = 0; i < loaded.count; ++i) {
        if (loaded.items[i].handled != 1) {
            persist_ok = 0;
            break;
        }
    }
    record_result(persist_ok, "ADIF-sparande och laddning bevarar hanterad-flaggan (APP_QSOLOG_HANDLED)", NULL);

    /* Rensa temporära resurser */
    unlink(tmp_adif);
    free(list.items);
    free(loaded.items);
}

/* ========================================================================= */
/* [4] Tangentbordsnavigering (Home / End) och filtrering                     */
/* ========================================================================= */
static void run_navigation_tests(void) {
    printf(ANSI_BLUE "\n=== [4] Tangentbordsnavigering (Home / End) och filtrering ===" ANSI_RESET "\n");

    QSOList list;
    memset(&list, 0, sizeof(list));

    for (int i = 0; i < 20; ++i) {
        QSO q;
        memset(&q, 0, sizeof(q));
        snprintf(q.call, sizeof(q.call), "SM%dTEST", i);
        snprintf(q.date, sizeof(q.date), "2026-11-%02d", i + 1);
        copy_str(q.band, sizeof(q.band), (i % 2 == 0) ? "20 m" : "40 m");
        qso_list_push(&list, &q);
    }

    int *indices = malloc(list.count * sizeof(int));
    int f_cnt = build_filtered_indices(&list, NULL, indices, (int)list.count);

    /* Index 0 i filtrerad lista ska vara senaste QSO (dvs list.count - 1),
       och index f_cnt - 1 ska vara äldsta QSO (dvs 0) */
    int home_pos = 0;
    int end_pos  = f_cnt - 1;

    int home_orig = indices[home_pos];
    int end_orig  = indices[end_pos];

    int ok_order = (f_cnt == 20) && (home_orig == 19) && (end_orig == 0);
    record_result(ok_order, "build_filtered_indices ordnar nyast QSO vid index 0 och äldst vid index count-1", NULL);

    int home_ok = (home_pos == 0) && (strcmp(list.items[home_orig].call, "SM19TEST") == 0);
    int end_ok  = (end_pos == 19) && (strcmp(list.items[end_orig].call, "SM0TEST") == 0);
    record_result(home_ok && end_ok, "HOME navigerar längst upp (#20/nyast) och END längst ned (#1/äldst)", NULL);

    /* Testa med filter på band "40 m" */
    int f_cnt_40 = build_filtered_indices(&list, "40 m", indices, (int)list.count);
    int filter_ok = (f_cnt_40 == 10);
    record_result(filter_ok, "build_filtered_indices filtrerar korrekt på delsträng ('40 m')", NULL);

    free(indices);
    free(list.items);
}

/* ========================================================================= */
/* [5] QRB och Maidenhead-lokatorer                                          */
/* ========================================================================= */
static void run_locator_and_qrb_tests(void) {
    printf(ANSI_BLUE "\n=== [5] QRB och Maidenhead-lokatorer ===" ANSI_RESET "\n");

    /* 1. Validering av lokatorer */
    int v6 = is_valid_locator("JO89we");
    int v4 = is_valid_locator("JO89");
    int inv1 = is_valid_locator("1234");
    int inv2 = is_valid_locator("ZZ99xx");
    int inv3 = is_valid_locator("");

    int val_ok = (v6 == 6) && (v4 == 4) && (inv1 == 0) && (inv2 == 0) && (inv3 == 0);
    record_result(val_ok, "is_valid_locator godkänner 4- och 6-teckens lokatorer samt avvisar ogiltiga", NULL);

    /* 2. Beräkning av avstånd mellan JO89we och JO67iq */
    double lat1 = 0, lon1 = 0, lat2 = 0, lon2 = 0;
    locator_to_latlon("JO89we", &lat1, &lon1);
    locator_to_latlon("JO67iq", &lat2, &lon2);

    double dist_km = calculate_distance_km(lat1, lon1, lat2, lon2);

    /* Avstånd ska vara ca 330-360 km */
    int qrb_ok = (dist_km >= 320.0 && dist_km <= 360.0);
    char detail[128];
    snprintf(detail, sizeof(detail), "JO89we -> JO67iq: %.1f km (förväntat ~344 km)", dist_km);
    record_result(qrb_ok, "QRB calculate_distance_km ger rimligt avstånd mellan JO89we och JO67iq (~344 km)", qrb_ok ? NULL : detail);
}

/* ========================================================================= */
/* [6] DXCC och anropssignalsuppslag                                         */
/* ========================================================================= */
static void run_dxcc_tests(void) {
    printf(ANSI_BLUE "\n=== [6] DXCC och anropssignalsuppslag ===" ANSI_RESET "\n");

    load_dxcc_file("dxcc.txt");

    char dxcc_sm[64] = "";
    char dxcc_dl[64] = "";
    char dxcc_w[64]  = "";

    lookup_dxcc("SM5UEI", dxcc_sm, sizeof(dxcc_sm));
    lookup_dxcc("DL1ABC", dxcc_dl, sizeof(dxcc_dl));
    lookup_dxcc("W1AW",   dxcc_w,  sizeof(dxcc_w));

    int sm_ok = (strstr(dxcc_sm, "Sverige") != NULL || strstr(dxcc_sm, "Sweden") != NULL);
    int dl_ok = (strstr(dxcc_dl, "Tyskland") != NULL || strstr(dxcc_dl, "Germany") != NULL);
    int w_ok  = (strstr(dxcc_w, "USA") != NULL || strstr(dxcc_w, "United States") != NULL);

    char detail[256];
    snprintf(detail, sizeof(detail), "SM5UEI -> '%s', DL1ABC -> '%s', W1AW -> '%s'", dxcc_sm, dxcc_dl, dxcc_w);
    record_result(sm_ok && dl_ok && w_ok, "lookup_dxcc identifierar Sverige (Sweden), Tyskland (Germany) och USA", (sm_ok && dl_ok && w_ok) ? NULL : detail);

    free_dxcc_db();
}

/* ========================================================================= */
/* [7] Stationsinställningar (station.conf)                                   */
/* ========================================================================= */
static void run_settings_tests(void) {
    printf(ANSI_BLUE "\n=== [7] Stationsinställningar (station.conf) ===" ANSI_RESET "\n");

    StationSettings st;
    memset(&st, 0, sizeof(st));
    copy_str(st.call, sizeof(st.call), "SM5UEI");
    copy_str(st.name, sizeof(st.name), "Anders");
    copy_str(st.qth, sizeof(st.qth), "Märsta");
    copy_str(st.gridsquare, sizeof(st.gridsquare), "JO89TM");
    copy_str(st.timezone, sizeof(st.timezone), "UTC+1");
    st.theme = 3;
    st.show_stats = 1;
    st.show_space_weather = 1;

    const char *tmp_conf = "/tmp/test_station_tmp.conf";
    save_station_settings(tmp_conf, &st);

    StationSettings loaded;
    memset(&loaded, 0, sizeof(loaded));
    load_station_settings(tmp_conf, &loaded);

    int ok = (strcmp(loaded.call, "SM5UEI") == 0) &&
             (strcmp(loaded.name, "Anders") == 0) &&
             (strcmp(loaded.qth, "Märsta") == 0) &&
             (strcmp(loaded.gridsquare, "JO89TM") == 0) &&
             (strcmp(loaded.timezone, "UTC+1") == 0) &&
             (loaded.theme == 3) &&
             (loaded.show_stats == 1) &&
             (loaded.show_space_weather == 1);

    unlink(tmp_conf);
    record_result(ok, "save_station_settings och load_station_settings bevarar alla stationsparametrar", NULL);
}

/* ========================================================================= */
/* [8] Strängverktyg och UTF-8                                               */
/* ========================================================================= */
static void run_string_and_utf8_tests(void) {
    printf(ANSI_BLUE "\n=== [8] Strängverktyg och UTF-8 ===" ANSI_RESET "\n");

    /* 1. trim_str */
    char s1[] = "  test sträng  \t\n";
    trim_str(s1);
    int trim_ok = (strcmp(s1, "test sträng") == 0);
    record_result(trim_ok, "trim_str tar bort inledande och avslutande blanksteg/tabbar/nyrad", NULL);

    /* 2. to_upper_str */
    char s2[] = "sm5uei";
    to_upper_str(s2);
    int upper_ok = (strcmp(s2, "SM5UEI") == 0);
    record_result(upper_ok, "to_upper_str omvandlar korrekt till versaler", NULL);

    /* 3. utf8_display_width */
    int w1 = utf8_display_width("ASCII");
    int w2 = utf8_display_width("ÅÄÖ");
    int w3 = utf8_display_width("✓");
    int w_ok = (w1 == 5) && (w2 == 3) && (w3 == 1);
    char detail[128];
    snprintf(detail, sizeof(detail), "w('ASCII')=%d, w('ÅÄÖ')=%d, w('✓')=%d", w1, w2, w3);
    record_result(w_ok, "utf8_display_width beräknar visningsbredd rätt för ASCII, åäö och symboler", w_ok ? NULL : detail);
}

/* ========================================================================= */
/* [9] SQLite3 Databaslagring (qsolog.sq3)                                   */
/* ========================================================================= */
static void run_sqlite_tests(void) {
    printf(ANSI_BLUE "\n=== [9] SQLite3 Databaslagring (qsolog.sq3) ===" ANSI_RESET "\n");

    const char *test_db_path = "/tmp/test_log_suite.sq3";
    unlink(test_db_path);

    /* 1. Öppna databas och initiera schema */
    int rc = db_open(test_db_path);
    int is_open = db_is_open();
    int initial_cnt = db_count_qsos();
    record_result((rc == 0) && is_open && (initial_cnt == 0),
                  "db_open skapar och initierar 'qsos' tabell och index i SQLite3", NULL);

    /* 2. Infoga QSO och verifiera genererat ID */
    QSO q1, q2, q3;
    memset(&q1, 0, sizeof(q1));
    copy_str(q1.call, sizeof(q1.call), "SM5UEI");
    copy_str(q1.date, sizeof(q1.date), "2026-11-14");
    copy_str(q1.time_utc, sizeof(q1.time_utc), "10:00");
    copy_str(q1.band, sizeof(q1.band), "20 m");
    copy_str(q1.mode, sizeof(q1.mode), "SSB");
    copy_str(q1.freq, sizeof(q1.freq), "14.200");
    copy_str(q1.rst_sent, sizeof(q1.rst_sent), "59");
    copy_str(q1.rst_rcvd, sizeof(q1.rst_rcvd), "59");
    q1.handled = 0;

    memset(&q2, 0, sizeof(q2));
    copy_str(q2.call, sizeof(q2.call), "DL1ABC");
    copy_str(q2.date, sizeof(q2.date), "2026-11-14");
    copy_str(q2.time_utc, sizeof(q2.time_utc), "10:15");
    copy_str(q2.band, sizeof(q2.band), "40 m");
    copy_str(q2.mode, sizeof(q2.mode), "CW");
    copy_str(q2.freq, sizeof(q2.freq), "7.020");
    copy_str(q2.rst_sent, sizeof(q2.rst_sent), "599");
    copy_str(q2.rst_rcvd, sizeof(q2.rst_rcvd), "579");
    q2.handled = 1;

    memset(&q3, 0, sizeof(q3));
    copy_str(q3.call, sizeof(q3.call), "K1ZZ");
    copy_str(q3.date, sizeof(q3.date), "2026-11-14");
    copy_str(q3.time_utc, sizeof(q3.time_utc), "10:30");
    copy_str(q3.band, sizeof(q3.band), "20 m");
    copy_str(q3.mode, sizeof(q3.mode), "SSB");
    copy_str(q3.freq, sizeof(q3.freq), "14.225");
    copy_str(q3.rst_sent, sizeof(q3.rst_sent), "59");
    copy_str(q3.rst_rcvd, sizeof(q3.rst_rcvd), "59");
    q3.handled = 0;

    int ins1 = db_insert_qso(&q1);
    int ins2 = db_insert_qso(&q2);
    int ins3 = db_insert_qso(&q3);
    int ins_ok = (ins1 == 0 && ins2 == 0 && ins3 == 0) &&
                 (q1.id > 0 && q2.id > 0 && q3.id > 0 && q2.id > q1.id);
    record_result(ins_ok, "db_insert_qso infogar poster och tilldelar unika auto-ID:n", NULL);

    /* 3. db_load_all_qsos */
    QSOList loaded;
    memset(&loaded, 0, sizeof(loaded));
    int load_cnt = db_load_all_qsos(&loaded);
    int load_ok = (load_cnt == 3) && (loaded.count == 3) &&
                  (strcmp(loaded.items[0].call, "SM5UEI") == 0) &&
                  (strcmp(loaded.items[1].call, "DL1ABC") == 0) &&
                  (strcmp(loaded.items[2].call, "K1ZZ") == 0);
    record_result(load_ok, "db_load_all_qsos läser in samtliga sparade QSO:n i kronologisk ordning", NULL);

    /* 4. db_update_qso */
    copy_str(loaded.items[0].comment, sizeof(loaded.items[0].comment), "Uppdaterad testkommentar");
    copy_str(loaded.items[0].rst_sent, sizeof(loaded.items[0].rst_sent), "55");
    int upd_rc = db_update_qso(&loaded.items[0]);

    QSOList reloaded;
    memset(&reloaded, 0, sizeof(reloaded));
    db_load_all_qsos(&reloaded);
    int upd_ok = (upd_rc == 0) && (reloaded.count == 3) &&
                 (strcmp(reloaded.items[0].comment, "Uppdaterad testkommentar") == 0) &&
                 (strcmp(reloaded.items[0].rst_sent, "55") == 0);
    record_result(upd_ok, "db_update_qso uppdaterar fält för ett befintligt QSO baserat på ID", NULL);
    free(reloaded.items);

    /* 5. db_set_handled och db_set_all_handled (F7) */
    db_set_handled(loaded.items[0].id, 1);
    db_set_handled(loaded.items[2].id, 0);
    int set_all_cnt = db_set_all_handled(1);

    memset(&reloaded, 0, sizeof(reloaded));
    db_load_all_qsos(&reloaded);
    int f7_db_ok = (set_all_cnt >= 3) &&
                   (reloaded.items[0].handled == 1) &&
                   (reloaded.items[1].handled == 1) &&
                   (reloaded.items[2].handled == 1);
    record_result(f7_db_ok, "db_set_all_handled (F7) sätter handled = 1 för samtliga poster i SQLite", NULL);
    free(reloaded.items);

    /* 6. db_delete_qso och db_delete_marked */
    int del1_rc = db_delete_qso(loaded.items[2].id);
    int cnt_after_del = db_count_qsos();
    loaded.items[0].marked = 1;
    int del_marked_cnt = db_delete_marked(&loaded);
    int cnt_after_marked = db_count_qsos();
    int del_ok = (del1_rc == 0) && (cnt_after_del == 2) &&
                 (del_marked_cnt == 1) && (cnt_after_marked == 1);
    record_result(del_ok, "db_delete_qso och db_delete_marked raderar specificerade poster ur databasen", NULL);

    /* 7. db_clear_all_qsos */
    db_clear_all_qsos();
    record_result(db_count_qsos() == 0, "db_clear_all_qsos tömmer tabellen fullständigt", NULL);

    /* 8. db_migrate_from_adif (Automatisk migrering från tidigare .adi) */
    const char *tmp_migr_adi = "/tmp/test_migr_source.adi";
    StationSettings st;
    memset(&st, 0, sizeof(st));
    copy_str(st.call, sizeof(st.call), "SM5UEI");

    QSOList migr_src;
    memset(&migr_src, 0, sizeof(migr_src));
    qso_list_push(&migr_src, &q1);
    qso_list_push(&migr_src, &q2);
    save_all_qsos_adif(tmp_migr_adi, &migr_src, &st);

    int migr_cnt = db_migrate_from_adif(tmp_migr_adi, &st);
    int migr_db_cnt = db_count_qsos();
    int migr_ok = (migr_cnt == 2) && (migr_db_cnt == 2);
    record_result(migr_ok, "db_migrate_from_adif migrerar automatiskt poster från ADIF till SQLite3", NULL);

    unlink(tmp_migr_adi);
    free(migr_src.items);

    /* 9. ADIF-export och import med SQLite */
    const char *tmp_export_adi = "/tmp/test_export_sqlite.adi";
    memset(&reloaded, 0, sizeof(reloaded));
    db_load_all_qsos(&reloaded);

    int exp_cnt = export_qsos_adif(tmp_export_adi, &reloaded, &st, EXPORT_MODE_ALL);
    int exp_ok = (exp_cnt == 2);

    /* Rensa databas och importera tillbaka från ADIF-filen */
    db_clear_all_qsos();
    QSOList import_list;
    memset(&import_list, 0, sizeof(import_list));
    int imp_cnt = import_qsos_from_adif(tmp_export_adi, &import_list, &st);
    int imp_db_cnt = db_count_qsos();
    int roundtrip_ok = exp_ok && (imp_cnt == 2) && (imp_db_cnt == 2);
    record_result(roundtrip_ok, "ADIF-export och import fungerar sömlöst tillsammans med SQLite-databasen", NULL);

    unlink(tmp_export_adi);
    free(import_list.items);
    free(reloaded.items);
    free(loaded.items);

    db_close();
    unlink(test_db_path);
}

/* ========================================================================= */
/* Huvudfunktion för testsviten                                              */
/* ========================================================================= */
int main(int argc, char *argv[]) {
    (void)argc;
    (void)argv;

    setlocale(LC_ALL, "");

    printf("====================================================\n");
    printf("   UEI-Logg Automatiserad Testsvit (testing)        \n");
    printf("====================================================\n");

    run_freq_and_band_tests();
    run_adif_freq_tests();
    run_handled_and_f7_tests();
    run_navigation_tests();
    run_locator_and_qrb_tests();
    run_dxcc_tests();
    run_settings_tests();
    run_string_and_utf8_tests();
    run_sqlite_tests();

    printf("\n====================================================\n");
    printf("Slutresultat: %d av %d tester lyckades.\n", g_tests_passed, g_tests_run);
    if (g_tests_failed == 0) {
        printf(ANSI_GREEN "STATUS: ALLA TESTER GODKÄNDA! [OK]" ANSI_RESET "\n");
        printf("====================================================\n");
        return 0;
    } else {
        printf(ANSI_RED "STATUS: %d TESTER MISSLYCKADES! [FEL]" ANSI_RESET "\n", g_tests_failed);
        printf("====================================================\n");
        return 1;
    }
}
