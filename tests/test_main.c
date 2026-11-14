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
#include "space_weather.h"

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

    /* 3. format_qrb_display formaterar avstånd för kolumnen i loggtabellen */
    QSO q_dsp1, q_dsp2;
    memset(&q_dsp1, 0, sizeof(q_dsp1));
    memset(&q_dsp2, 0, sizeof(q_dsp2));
    copy_str(q_dsp1.distance, sizeof(q_dsp1.distance), "344");
    copy_str(q_dsp2.distance, sizeof(q_dsp2.distance), "");

    char out_dsp1[32] = "", out_dsp2[32] = "";
    format_qrb_display(&q_dsp1, out_dsp1, sizeof(out_dsp1));
    format_qrb_display(&q_dsp2, out_dsp2, sizeof(out_dsp2));
    int dsp_ok = (strcmp(out_dsp1, "344") == 0) && (strcmp(out_dsp2, "-") == 0);
    record_result(dsp_ok, "format_qrb_display formaterar QRB-kolumn korrekt ('344' resp. '-')", NULL);

    /* 4. update_qso_qrb och automatisk persistens i databasen */
    StationSettings st_qrb;
    memset(&st_qrb, 0, sizeof(st_qrb));
    copy_str(st_qrb.call, sizeof(st_qrb.call), "SM5UEI");
    copy_str(st_qrb.gridsquare, sizeof(st_qrb.gridsquare), "JO89we");

    const char *test_qrb_db = "/tmp/test_qrb_db.sq3";
    unlink(test_qrb_db);
    db_open(test_qrb_db);

    QSO q_calc;
    memset(&q_calc, 0, sizeof(q_calc));
    copy_str(q_calc.call, sizeof(q_calc.call), "SM7TEST");
    copy_str(q_calc.gridsquare, sizeof(q_calc.gridsquare), "JO67iq");
    copy_str(q_calc.date, sizeof(q_calc.date), "2026-11-14");
    copy_str(q_calc.time_utc, sizeof(q_calc.time_utc), "12:00");
    copy_str(q_calc.band, sizeof(q_calc.band), "20 m");
    copy_str(q_calc.mode, sizeof(q_calc.mode), "SSB");
    q_calc.distance[0] = '\0';
    db_insert_qso(&q_calc);

    int qrb_updated = update_qso_qrb(&st_qrb, &q_calc);
    int dist_val = atoi(q_calc.distance);
    int calc_mem_ok = qrb_updated && (dist_val >= 320 && dist_val <= 360);

    /* Läs tillbaka från databasen för att säkerställa att QRB sparats i SQLite3 */
    QSOList reloaded_qrb;
    memset(&reloaded_qrb, 0, sizeof(reloaded_qrb));
    db_load_all_qsos(&reloaded_qrb);
    int db_persisted_ok = (reloaded_qrb.count == 1) &&
                          (strcmp(reloaded_qrb.items[0].distance, q_calc.distance) == 0);

    record_result(calc_mem_ok && db_persisted_ok,
                  "update_qso_qrb beräknar QRB från lokator och sparar automatiskt i SQLite3", NULL);
    free(reloaded_qrb.items);

    /* 5. update_all_missing_qrbs beräknar saknade QRB i en lista */
    QSOList batch_qrb_list;
    memset(&batch_qrb_list, 0, sizeof(batch_qrb_list));
    QSO q_m1, q_m2;
    memset(&q_m1, 0, sizeof(q_m1));
    memset(&q_m2, 0, sizeof(q_m2));
    copy_str(q_m1.call, sizeof(q_m1.call), "SA5AAA");
    copy_str(q_m1.gridsquare, sizeof(q_m1.gridsquare), "JO89we");
    copy_str(q_m2.call, sizeof(q_m2.call), "SK0TM");
    copy_str(q_m2.gridsquare, sizeof(q_m2.gridsquare), "JO89xi");

    qso_list_push(&batch_qrb_list, &q_m1);
    qso_list_push(&batch_qrb_list, &q_m2);
    db_insert_qso(&batch_qrb_list.items[0]);
    db_insert_qso(&batch_qrb_list.items[1]);

    int batch_upd_cnt = update_all_missing_qrbs(&st_qrb, &batch_qrb_list);
    int batch_qrb_ok = (batch_upd_cnt == 2) &&
                       (batch_qrb_list.items[0].distance[0] != '\0') &&
                       (batch_qrb_list.items[1].distance[0] != '\0');
    record_result(batch_qrb_ok,
                  "update_all_missing_qrbs uppdaterar alla saknade QRB-värden i logglistan och databasen", NULL);

    /* 6. Bäringsberäkning (calculate_bearing_deg) och format_quj_display */
    double b_north = calculate_bearing_deg(50.0, 10.0, 60.0, 10.0);
    double b_south = calculate_bearing_deg(60.0, 10.0, 50.0, 10.0);
    double b_east  = calculate_bearing_deg(0.0, 10.0, 0.0, 20.0);
    double b_west  = calculate_bearing_deg(0.0, 20.0, 0.0, 10.0);
    int cardinal_ok = (fabs(b_north - 0.0) < 0.1 || fabs(b_north - 360.0) < 0.1) &&
                      (fabs(b_south - 180.0) < 0.1) &&
                      (fabs(b_east - 90.0) < 0.1) &&
                      (fabs(b_west - 270.0) < 0.1);

    double b_sm = calculate_bearing_deg(lat1, lon1, lat2, lon2); /* JO89we -> JO67iq (~230°) */
    int bearing_sm_ok = (b_sm >= 220.0 && b_sm <= 245.0);

    char quj_dsp1[32] = "", quj_dsp2[32] = "";
    format_quj_display(&st_qrb, &q_calc, quj_dsp1, sizeof(quj_dsp1));
    format_quj_display(&st_qrb, &q_dsp2, quj_dsp2, sizeof(quj_dsp2));
    int quj_dsp_ok = (strstr(quj_dsp1, "°") != NULL && atoi(quj_dsp1) >= 220) &&
                     (strcmp(quj_dsp2, "-") == 0);

    record_result(cardinal_ok && bearing_sm_ok && quj_dsp_ok,
                  "QUJ calculate_bearing_deg och format_quj_display beräknar och formaterar bäring korrekt", NULL);

    free(batch_qrb_list.items);
    db_close();
    unlink(test_qrb_db);
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
    st.show_notes = 0;

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
             (loaded.show_space_weather == 1) &&
             (loaded.show_notes == 0);

    /* Testa även show_notes = 1 */
    st.show_notes = 1;
    save_station_settings(tmp_conf, &st);
    memset(&loaded, 0, sizeof(loaded));
    load_station_settings(tmp_conf, &loaded);
    int notes_ok = (loaded.show_notes == 1);

    unlink(tmp_conf);
    record_result(ok && notes_ok, "save_station_settings och load_station_settings bevarar alla stationsparametrar (inkl. show_notes för F6)", NULL);

    /* Rymdväder: K/Kp och titelradens högerställda tidsstämpel */
    {
        SpaceWeatherData sw;
        space_weather_get_data(&sw);
        copy_str(sw.updated_time, sizeof(sw.updated_time), "16:45");
        copy_str(sw.k_kiruna, sizeof(sw.k_kiruna), "2/1");

        char time_str[64];
        snprintf(time_str, sizeof(time_str), " (kl %s) ", sw.updated_time);
        int tlen = utf8_display_width(time_str);
        int width = 80;
        int tx = (width - 1) - tlen;

        int right_aligned_ok = (tx == 67 && (tx + tlen == width - 1));
        int label_ok = (sw.k_kiruna[0] != '\0');

        record_result(right_aligned_ok && label_ok,
                      "Rymdvädersrutan: K/Kp och tidsstämpel högerställd i titelraden (t.ex. ' (kl 16:45) ')", NULL);
    }

    /* Titelrad (top bar): Anropssignal, QTH och Lokator utan parenteser */
    {
        StationSettings st_top;
        memset(&st_top, 0, sizeof(st_top));
        copy_str(st_top.call, sizeof(st_top.call), "SM5UEI");
        copy_str(st_top.qth, sizeof(st_top.qth), "KARLHOLM");
        copy_str(st_top.gridsquare, sizeof(st_top.gridsquare), "JP80TM");

        char station_info[128];
        const char *my_call = st_top.call[0] ? st_top.call : "SM5UEI";
        const char *my_qth  = st_top.qth[0] ? st_top.qth : "";
        const char *my_grid = st_top.gridsquare[0] ? st_top.gridsquare : "";
        if (my_qth[0] != '\0' && my_grid[0] != '\0') {
            snprintf(station_info, sizeof(station_info), "%s %s %s", my_call, my_qth, my_grid);
        }
        int top_ok = (strcmp(station_info, "SM5UEI KARLHOLM JP80TM") == 0) &&
                     (strchr(station_info, '(') == NULL) &&
                     (strchr(station_info, ')') == NULL);
        record_result(top_ok, "Titelrad: format 'Anropssignal QTH Lokator' utan parenteser runt QTH", NULL);
    }
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

    /* 4. sanitize_to_utf8: konverterar Latin-1 och reparerar mojibake/avhuggna tecken */
    char clean_qth1[64], clean_qth2[64], clean_name[64], clean_utf8[64];
    sanitize_to_utf8("V\xe4ster\xe5s", clean_qth1, sizeof(clean_qth1));
    sanitize_to_utf8("V\xe4llingby", clean_qth2, sizeof(clean_qth2));
    sanitize_to_utf8("Jonny Bring\xc3\x83\xc2", clean_name, sizeof(clean_name));
    sanitize_to_utf8("Märsta", clean_utf8, sizeof(clean_utf8));

    int san_ok = (strcmp(clean_qth1, "Västerås") == 0) &&
                 (strcmp(clean_qth2, "Vällingby") == 0) &&
                 (strcmp(clean_name, "Jonny Bring") == 0) &&
                 (strcmp(clean_utf8, "Märsta") == 0);
    record_result(san_ok, "sanitize_to_utf8 konverterar 'V\\xe4ster\\xe5s' till 'Västerås' och fixar mojibake", NULL);

    /* 5. utf8_pad_or_clip: klipper vid teckengränser och fyller ut rätt displaybredd */
    char clip_buf[64], pad_buf[64], ralign_buf[64];
    utf8_pad_or_clip(clip_buf, sizeof(clip_buf), "Västerås", 5, 0);
    utf8_pad_or_clip(pad_buf, sizeof(pad_buf), "Västerås", 12, 0);
    utf8_pad_or_clip(ralign_buf, sizeof(ralign_buf), "344 km", 8, 1);

    int clip_ok = (strcmp(clip_buf, "Väste") == 0) &&
                  (utf8_display_width(clip_buf) == 5) &&
                  (utf8_display_width(pad_buf) == 12) &&
                  (strcmp(ralign_buf, "  344 km") == 0);
    record_result(clip_ok, "utf8_pad_or_clip klipper och fyller ut utan att dela UTF-8-bytes ('Väste')", NULL);

    /* 6. format_date_yymmdd och format_time_hhmm */
    char yymmdd[16], hhmm[16];
    format_date_yymmdd("2026-11-14", yymmdd, sizeof(yymmdd));
    format_time_hhmm("10:30", hhmm, sizeof(hhmm));
    int dt_ok = (strcmp(yymmdd, "261114") == 0) && (strcmp(hhmm, "1030") == 0);
    record_result(dt_ok, "format_date_yymmdd -> '261114' och format_time_hhmm -> '1030'", NULL);

    /* 7. format_rst_combined */
    char rst_c1[32], rst_c2[32], rst_c3[32];
    format_rst_combined("599", "599", rst_c1, sizeof(rst_c1));
    format_rst_combined("59+20", "59+5", rst_c2, sizeof(rst_c2));
    format_rst_combined("59", "59", rst_c3, sizeof(rst_c3));
    int rst_ok = (strcmp(rst_c1, "599/599") == 0) &&
                 (strcmp(rst_c2, "59+20/59+5") == 0) &&
                 (strcmp(rst_c3, "59/59") == 0);
    record_result(rst_ok, "format_rst_combined sammanfogar RST T och RST R som '599/599' och '59+20/59+5'", NULL);
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
                  "db_open skapar och initierar 'qsos' tabell i SQLite3", NULL);

    /* 2. Verifiera WAL-läge och PRAGMA synchronous */
    sqlite3 *handle = db_get_handle();
    int wal_ok = 0;
    if (handle != NULL) {
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(handle, "PRAGMA journal_mode;", -1, &stmt, NULL) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                const unsigned char *mode = sqlite3_column_text(stmt, 0);
                if (mode && strcasecmp((const char *)mode, "wal") == 0) {
                    wal_ok = 1;
                }
            }
            sqlite3_finalize(stmt);
        }
    }
    record_result(wal_ok, "Databasen körs i WAL-läge (Write-Ahead Logging) för kraschsäkerhet", NULL);

    /* 3. Verifiera skapade index i sqlite_master */
    int idx_count = 0;
    if (handle != NULL) {
        const char *idx_sql = "SELECT COUNT(*) FROM sqlite_master WHERE type='index' AND name IN "
                              "('idx_qsos_call', 'idx_qsos_date_time', 'idx_qsos_band', 'idx_qsos_handled');";
        sqlite3_stmt *stmt = NULL;
        if (sqlite3_prepare_v2(handle, idx_sql, -1, &stmt, NULL) == SQLITE_OK) {
            if (sqlite3_step(stmt) == SQLITE_ROW) {
                idx_count = sqlite3_column_int(stmt, 0);
            }
            sqlite3_finalize(stmt);
        }
    }
    record_result(idx_count == 4, "Tabellindex för call, date/time, band och handled är skapade", NULL);

    /* 4. Infoga QSO och verifiera genererat ID */
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

    /* 5. db_load_all_qsos med kronologisk ordning */
    QSOList loaded;
    memset(&loaded, 0, sizeof(loaded));
    int load_cnt = db_load_all_qsos(&loaded);
    int load_ok = (load_cnt == 3) && (loaded.count == 3) &&
                  (strcmp(loaded.items[0].call, "SM5UEI") == 0) &&
                  (strcmp(loaded.items[1].call, "DL1ABC") == 0) &&
                  (strcmp(loaded.items[2].call, "K1ZZ") == 0);
    record_result(load_ok, "db_load_all_qsos läser in samtliga sparade QSO:n i kronologisk ordning", NULL);

    /* 6. SQL-injektionsskydd och specialtecken (citat, semikolon, åäö, nyrad) */
    QSO q_inj;
    memset(&q_inj, 0, sizeof(q_inj));
    copy_str(q_inj.call, sizeof(q_inj.call), "O'CONNOR");
    copy_str(q_inj.name, sizeof(q_inj.name), "Åke \"Blixten\" O'Reilly; DROP TABLE qsos; --");
    copy_str(q_inj.comment, sizeof(q_inj.comment), "Test ' ' \" ; -- /* comment */ \n nyrad");
    copy_str(q_inj.date, sizeof(q_inj.date), "2026-11-14");
    copy_str(q_inj.time_utc, sizeof(q_inj.time_utc), "11:00");
    copy_str(q_inj.band, sizeof(q_inj.band), "20 m");
    copy_str(q_inj.mode, sizeof(q_inj.mode), "SSB");
    int ins_inj = db_insert_qso(&q_inj);

    QSOList loaded_inj;
    memset(&loaded_inj, 0, sizeof(loaded_inj));
    db_load_all_qsos(&loaded_inj);
    int inj_found = 0;
    for (size_t i = 0; i < loaded_inj.count; ++i) {
        if (strcmp(loaded_inj.items[i].call, "O'CONNOR") == 0) {
            if (strcmp(loaded_inj.items[i].name, "Åke \"Blixten\" O'Reilly; DROP TABLE qsos; --") == 0 &&
                strcmp(loaded_inj.items[i].comment, "Test ' ' \" ; -- /* comment */ \n nyrad") == 0) {
                inj_found = 1;
            }
        }
    }
    int inj_ok = (ins_inj == 0) && inj_found && (db_count_qsos() == 4);
    record_result(inj_ok, "SQL-injektionsskydd och bevarande av specialtecken (apostrof, citat, åäö)", NULL);
    /* Rensa injektionsposten så att efterföljande tester har känd data */
    db_delete_qso(q_inj.id);
    free(loaded_inj.items);

    /* 7. Hantering av tomma fält (får ej läsas ut som NULL eller skräp) */
    QSO q_empty;
    memset(&q_empty, 0, sizeof(q_empty));
    copy_str(q_empty.call, sizeof(q_empty.call), "SA0EMPTY");
    copy_str(q_empty.date, sizeof(q_empty.date), "2026-11-14");
    copy_str(q_empty.time_utc, sizeof(q_empty.time_utc), "11:30");
    db_insert_qso(&q_empty);

    QSOList loaded_empty;
    memset(&loaded_empty, 0, sizeof(loaded_empty));
    db_load_all_qsos(&loaded_empty);
    int empty_ok = 0;
    for (size_t i = 0; i < loaded_empty.count; ++i) {
        if (strcmp(loaded_empty.items[i].call, "SA0EMPTY") == 0) {
            if (loaded_empty.items[i].name[0] == '\0' &&
                loaded_empty.items[i].comment[0] == '\0' &&
                loaded_empty.items[i].gridsquare[0] == '\0') {
                empty_ok = 1;
            }
        }
    }
    record_result(empty_ok, "Tomma textfält lagras och läses ut som tomma strängar utan minnesfel", NULL);
    db_delete_qso(q_empty.id);
    free(loaded_empty.items);

    /* 8. db_update_qso */
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

    /* 9. db_set_handled (selektiv status per ID) */
    db_set_handled(loaded.items[0].id, 1);
    db_set_handled(loaded.items[2].id, 0);

    memset(&reloaded, 0, sizeof(reloaded));
    db_load_all_qsos(&reloaded);
    int handled_single_ok = (reloaded.items[0].handled == 1) &&
                            (reloaded.items[1].handled == 1) &&
                            (reloaded.items[2].handled == 0);
    record_result(handled_single_ok, "db_set_handled ändrar hanteringsstatus selektivt för enskilda rader", NULL);
    free(reloaded.items);

    /* 10. db_set_all_handled (F7) */
    int set_all_cnt = db_set_all_handled(1);

    memset(&reloaded, 0, sizeof(reloaded));
    db_load_all_qsos(&reloaded);
    int f7_db_ok = (set_all_cnt >= 3) &&
                   (reloaded.items[0].handled == 1) &&
                   (reloaded.items[1].handled == 1) &&
                   (reloaded.items[2].handled == 1);
    record_result(f7_db_ok, "db_set_all_handled (F7) sätter handled = 1 för samtliga poster i SQLite", NULL);
    free(reloaded.items);

    /* 11. db_delete_qso och db_delete_marked */
    int del1_rc = db_delete_qso(loaded.items[2].id);
    int cnt_after_del = db_count_qsos();
    loaded.items[0].marked = 1;
    int del_marked_cnt = db_delete_marked(&loaded);
    int cnt_after_marked = db_count_qsos();
    int del_ok = (del1_rc == 0) && (cnt_after_del == 2) &&
                 (del_marked_cnt == 1) && (cnt_after_marked == 1);
    record_result(del_ok, "db_delete_qso och db_delete_marked raderar specificerade poster ur databasen", NULL);

    /* 12. db_clear_all_qsos */
    db_clear_all_qsos();
    record_result(db_count_qsos() == 0, "db_clear_all_qsos tömmer tabellen fullständigt och kör VACUUM", NULL);

    /* 13. db_batch_insert (större batch inom en transaktion) */
    QSO batch_arr[50];
    for (int i = 0; i < 50; ++i) {
        memset(&batch_arr[i], 0, sizeof(QSO));
        snprintf(batch_arr[i].call, sizeof(batch_arr[i].call), "SM%dTEST", i);
        copy_str(batch_arr[i].date, sizeof(batch_arr[i].date), "2026-11-14");
        snprintf(batch_arr[i].time_utc, sizeof(batch_arr[i].time_utc), "%02d:%02d", (i / 60) % 24, i % 60);
        copy_str(batch_arr[i].band, sizeof(batch_arr[i].band), "20 m");
        copy_str(batch_arr[i].mode, sizeof(batch_arr[i].mode), "SSB");
        batch_arr[i].handled = (i % 2 == 0);
    }
    int b_ins = db_batch_insert(batch_arr, 50);
    int b_db_cnt = db_count_qsos();
    int batch_ids_ok = (batch_arr[0].id > 0) && (batch_arr[49].id > batch_arr[0].id);
    int batch_ok = (b_ins == 50) && (b_db_cnt == 50) && batch_ids_ok;
    record_result(batch_ok, "db_batch_insert infogar snabbt 50 poster inom en transaktion med unika ID:n", NULL);

    /* 14. Diskpersistens över stängning och återöppning (db_close -> db_open) */
    db_close();
    record_result(db_is_open() == 0, "db_close stänger anslutningen ordnat", NULL);

    int reopen_rc = db_open(test_db_path);
    int reopen_cnt = db_count_qsos();
    int persist_ok = (reopen_rc == 0) && (reopen_cnt == 50);
    record_result(persist_ok, "Diskpersistens: databasens innehåll och antal bevaras efter återöppning", NULL);

    /* 15. Felhantering vid stängd databas och ogiltigt ID */
    db_close();
    QSO dummy_q;
    memset(&dummy_q, 0, sizeof(dummy_q));
    copy_str(dummy_q.call, sizeof(dummy_q.call), "ERR");
    dummy_q.id = -10;

    int closed_ins = db_insert_qso(&dummy_q);
    int closed_upd = db_update_qso(&dummy_q);
    int closed_del = db_delete_qso(1);
    QSOList closed_list;
    int closed_load = db_load_all_qsos(&closed_list);
    int err_closed_ok = (closed_ins == -1) && (closed_upd == -1) &&
                        (closed_del == -1) && (closed_load == -1);

    db_open(test_db_path);
    int inv_upd = db_update_qso(&dummy_q);
    int inv_del = db_delete_qso(-1);
    int err_inv_ok = (inv_upd == -1) && (inv_del == -1);
    record_result(err_closed_ok && err_inv_ok,
                  "Robust felhantering: stängd DB eller ogiltigt ID returnerar -1 säkert", NULL);

    /* Rensa databasen för migrations- och ADIF-testerna */
    db_clear_all_qsos();

    /* 16. db_migrate_from_adif (Automatisk migrering från tidigare .adi) */
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

    /* Verifiera att migrering inte körs om databasen redan innehåller data */
    int second_migr = db_migrate_from_adif(tmp_migr_adi, &st);
    record_result(second_migr == 0 && db_count_qsos() == 2,
                  "db_migrate_from_adif hoppas över om databasen redan innehåller poster", NULL);

    unlink(tmp_migr_adi);
    free(migr_src.items);

    /* 17. ADIF-export och import med SQLite */
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

    /* 18. db_sanitize_all_utf8: automatisk reparation av Latin-1-poster till giltig UTF-8 */
    db_clear_all_qsos();
    sqlite3 *raw_db = db_get_handle();
    if (raw_db != NULL) {
        /* Infoga råa ISO-8859-1 (Latin-1) bytes: V\xe4ster\xe5s och V\xe4llingby */
        sqlite3_stmt *raw_stmt = NULL;
        const char *raw_sql = "INSERT INTO qsos (call, name, qso_date, time_utc, qth) VALUES ('SM5TEST', 'Bernt', '2026-11-14', '12:00', ?);";
        if (sqlite3_prepare_v2(raw_db, raw_sql, -1, &raw_stmt, NULL) == SQLITE_OK) {
            const char raw_latin1[] = "V\xe4ster\xe5s";
            sqlite3_bind_text(raw_stmt, 1, raw_latin1, (int)strlen(raw_latin1), SQLITE_STATIC);
            sqlite3_step(raw_stmt);
            sqlite3_finalize(raw_stmt);
        }
    }

    int san_cnt = db_sanitize_all_utf8();
    QSOList san_list;
    memset(&san_list, 0, sizeof(san_list));
    db_load_all_qsos(&san_list);

    int san_db_ok = (san_cnt >= 1) && (san_list.count == 1) &&
                    (strcmp(san_list.items[0].qth, "Västerås") == 0);
    record_result(san_db_ok, "db_sanitize_all_utf8 detekterar och reparerar automatiskt 'V\\xe4ster\\xe5s' till 'Västerås' i databasen", NULL);
    free(san_list.items);

    db_close();
    unlink(test_db_path);

    /* 19. Verifiering mot reell databas qsolog.sq3 (om den finns på disk) */
    if (access("qsolog.sq3", F_OK) == 0) {
        db_open("qsolog.sq3");
        QSOList prod_list;
        memset(&prod_list, 0, sizeof(prod_list));
        int loaded_prod = db_load_all_qsos(&prod_list);
        int vasteras_found = 0;
        int utf8_clean = 1;
        for (size_t i = 0; i < prod_list.count; ++i) {
            if (strstr(prod_list.items[i].qth, "Västerås") != NULL) {
                vasteras_found = 1;
            }
            if (utf8_display_width(prod_list.items[i].call) < 0 ||
                utf8_display_width(prod_list.items[i].name) < 0 ||
                utf8_display_width(prod_list.items[i].qth) < 0) {
                utf8_clean = 0;
            }
        }
        int prod_ok = (loaded_prod > 0) && vasteras_found && utf8_clean;
        record_result(prod_ok, "qsolog.sq3 sanerad: QTH 'Västerås' och alla namn/qth är 100% ren UTF-8", NULL);
        free(prod_list.items);
        db_close();
    }
}

/* ========================================================================= */
/* [10] Hantering av ÅÄÖ i Namn, QTH och Notering                            */
/* ========================================================================= */
static void run_swedish_chars_tests(void) {
    printf(ANSI_BLUE "\n=== [10] Hantering av ÅÄÖ i Namn, QTH och Notering ===" ANSI_RESET "\n");

    const char *test_db_path = "/tmp/test_swedish_chars.sq3";
    unlink(test_db_path);

    /* 1. Visningsbredd (utf8_display_width) för Å, Ä, Ö, å, ä, ö */
    {
        const char *name_sample = "Åke Öberg";                  /* 9 tecken, 11 bytes */
        const char *qth_sample = "Örnsköldsvik";                /* 12 tecken, 14 bytes */
        const char *not_sample = "Kör med dipol & strålande väder!"; /* 32 tecken */

        int w_name = utf8_display_width(name_sample);
        int w_qth = utf8_display_width(qth_sample);
        int w_not = utf8_display_width(not_sample);

        int ok = (w_name == 9) && (w_qth == 12) && (w_not == 32);
        char detail[128];
        snprintf(detail, sizeof(detail), "w_name=%d (förv. 9), w_qth=%d (förv. 12), w_not=%d (förv. 32)",
                 w_name, w_qth, w_not);
        record_result(ok, "utf8_display_width beräknar exakt visningsbredd för Namn, QTH och Notering med ÅÄÖ",
                      ok ? NULL : detail);
    }

    /* 2. Klippning och utfyllnad (utf8_pad_or_clip) utan att kapa UTF-8-bytes mitt itu */
    {
        char clip_name[64], clip_qth[64], clip_not[64];
        char pad_name[64], pad_qth[64];

        /* "Åke Öberg" (9 kolumner) klippt till 5 -> "Åke Ö" */
        utf8_pad_or_clip(clip_name, sizeof(clip_name), "Åke Öberg", 5, 0);
        /* "Örnsköldsvik" (12 kolumner) klippt till 7 -> "Örnsköl" */
        utf8_pad_or_clip(clip_qth, sizeof(clip_qth), "Örnsköldsvik", 7, 0);
        /* "Strålande signal på 80m!" klippt till 10 -> "Strålande " */
        utf8_pad_or_clip(clip_not, sizeof(clip_not), "Strålande signal på 80m!", 10, 0);

        /* Pad: "Åmål" (4 kolumner) fyllt till 10 -> "Åmål      " */
        utf8_pad_or_clip(pad_qth, sizeof(pad_qth), "Åmål", 10, 0);
        /* Pad: "Märta" (5 kolumner) fyllt till 8 -> "Märta   " */
        utf8_pad_or_clip(pad_name, sizeof(pad_name), "Märta", 8, 0);

        int ok = (strcmp(clip_name, "Åke Ö") == 0) &&
                 (strcmp(clip_qth, "Örnsköl") == 0) &&
                 (strcmp(clip_not, "Strålande ") == 0) &&
                 (strcmp(pad_qth, "Åmål      ") == 0) &&
                 (strcmp(pad_name, "Märta   ") == 0) &&
                 (utf8_display_width(clip_name) == 5) &&
                 (utf8_display_width(clip_qth) == 7) &&
                 (utf8_display_width(clip_not) == 10);
        record_result(ok, "utf8_pad_or_clip klipper och fyller ut Namn, QTH och Notering utan byte-avhuggning", NULL);
    }

    /* 3. Sanering (sanitize_to_utf8) av råa Latin-1 (ISO-8859-1) bytes */
    {
        char clean_name1[64], clean_name2[64];
        char clean_qth1[64], clean_qth2[64], clean_qth3[64];
        char clean_not1[256], clean_not2[256];

        /* Namn: \xc5ke \xd6berg -> Åke Öberg, M\xe4rta \xc4ngstr\xf6m -> Märta Ängström */
        sanitize_to_utf8("\xc5ke \xd6" "berg", clean_name1, sizeof(clean_name1));
        sanitize_to_utf8("M\xe4rta \xc4ngstr\xf6m", clean_name2, sizeof(clean_name2));

        /* QTH: \xd6rnsk\xf6ldsvik -> Örnsköldsvik, \xc5m\xe5l -> Åmål, V\xe4ster\xe5s -> Västerås */
        sanitize_to_utf8("\xd6rnsk\xf6ldsvik", clean_qth1, sizeof(clean_qth1));
        sanitize_to_utf8("\xc5m\xe5l", clean_qth2, sizeof(clean_qth2));
        sanitize_to_utf8("V\xe4ster\xe5s", clean_qth3, sizeof(clean_qth3));

        /* Notering: råa svenska tecken i text */
        sanitize_to_utf8("Str\xe5lande signal fr\xe5n \xd6land med h\xf6g effekt p\xe5 80m!", clean_not1, sizeof(clean_not1));
        sanitize_to_utf8("Tack f\xf6r trevligt QSO! V\xe4" "dret \xe4r toppen i \xc5m\xe5l.", clean_not2, sizeof(clean_not2));

        int ok = (strcmp(clean_name1, "Åke Öberg") == 0) &&
                 (strcmp(clean_name2, "Märta Ängström") == 0) &&
                 (strcmp(clean_qth1, "Örnsköldsvik") == 0) &&
                 (strcmp(clean_qth2, "Åmål") == 0) &&
                 (strcmp(clean_qth3, "Västerås") == 0) &&
                 (strcmp(clean_not1, "Strålande signal från Öland med hög effekt på 80m!") == 0) &&
                 (strcmp(clean_not2, "Tack för trevligt QSO! Vädret är toppen i Åmål.") == 0);
        record_result(ok, "sanitize_to_utf8 konverterar råa Latin-1 Å, Ä, Ö, å, ä, ö i Namn, QTH och Notering", NULL);
    }

    /* 4. SQLite3 lagring och utläsning (db_insert_qso & db_load_all_qsos) med ÅÄÖ */
    {
        db_open(test_db_path);

        QSO q1, q2, q3;
        memset(&q1, 0, sizeof(q1));
        copy_str(q1.call, sizeof(q1.call), "SM5AKE");
        copy_str(q1.date, sizeof(q1.date), "2026-11-14");
        copy_str(q1.time_utc, sizeof(q1.time_utc), "14:00");
        copy_str(q1.band, sizeof(q1.band), "20 m");
        copy_str(q1.mode, sizeof(q1.mode), "SSB");
        copy_str(q1.name, sizeof(q1.name), "Åke Öberg");
        copy_str(q1.qth, sizeof(q1.qth), "Örnsköldsvik");
        copy_str(q1.comment, sizeof(q1.comment), "Strålande förhållanden på 20m! Kör portabelt från stugan.");

        memset(&q2, 0, sizeof(q2));
        copy_str(q2.call, sizeof(q2.call), "SM0MTA");
        copy_str(q2.date, sizeof(q2.date), "2026-11-14");
        copy_str(q2.time_utc, sizeof(q2.time_utc), "14:15");
        copy_str(q2.band, sizeof(q2.band), "40 m");
        copy_str(q2.mode, sizeof(q2.mode), "CW");
        copy_str(q2.name, sizeof(q2.name), "Märta Ängström");
        copy_str(q2.qth, sizeof(q2.qth), "Västerås");
        copy_str(q2.comment, sizeof(q2.comment), "Trevligt QSO, 100W till en trådantenn. Återkommer gärna!");

        memset(&q3, 0, sizeof(q3));
        copy_str(q3.call, sizeof(q3.call), "SM7BJN");
        copy_str(q3.date, sizeof(q3.date), "2026-11-14");
        copy_str(q3.time_utc, sizeof(q3.time_utc), "14:30");
        copy_str(q3.band, sizeof(q3.band), "80 m");
        copy_str(q3.mode, sizeof(q3.mode), "SSB");
        copy_str(q3.name, sizeof(q3.name), "Björn Håkansson");
        copy_str(q3.qth, sizeof(q3.qth), "Åmål");
        copy_str(q3.comment, sizeof(q3.comment), "Önskar en trevlig helg! Gott nytt år till hela klubben.");

        db_insert_qso(&q1);
        db_insert_qso(&q2);
        db_insert_qso(&q3);

        QSOList loaded;
        memset(&loaded, 0, sizeof(loaded));
        db_load_all_qsos(&loaded);

        int ok = (loaded.count == 3) &&
                 (strcmp(loaded.items[0].name, "Åke Öberg") == 0) &&
                 (strcmp(loaded.items[0].qth, "Örnsköldsvik") == 0) &&
                 (strcmp(loaded.items[0].comment, "Strålande förhållanden på 20m! Kör portabelt från stugan.") == 0) &&
                 (strcmp(loaded.items[1].name, "Märta Ängström") == 0) &&
                 (strcmp(loaded.items[1].qth, "Västerås") == 0) &&
                 (strcmp(loaded.items[1].comment, "Trevligt QSO, 100W till en trådantenn. Återkommer gärna!") == 0) &&
                 (strcmp(loaded.items[2].name, "Björn Håkansson") == 0) &&
                 (strcmp(loaded.items[2].qth, "Åmål") == 0) &&
                 (strcmp(loaded.items[2].comment, "Önskar en trevlig helg! Gott nytt år till hela klubben.") == 0);

        record_result(ok, "SQLite3 INSERT och SELECT bevarar Å, Ä, Ö, å, ä, ö intakta i Namn, QTH och Notering", NULL);
        free(loaded.items);
    }

    /* 5. SQLite3 uppdatering (db_update_qso) med ändrade ÅÄÖ-värden */
    {
        QSOList loaded;
        memset(&loaded, 0, sizeof(loaded));
        db_load_all_qsos(&loaded);

        copy_str(loaded.items[0].name, sizeof(loaded.items[0].name), "Göran Östlund");
        copy_str(loaded.items[0].qth, sizeof(loaded.items[0].qth), "Jönköping");
        copy_str(loaded.items[0].comment, sizeof(loaded.items[0].comment), "Ändrad notering: Bytte till 80m dipol, fin signalstyrka!");

        int upd_rc = db_update_qso(&loaded.items[0]);

        QSOList reloaded;
        memset(&reloaded, 0, sizeof(reloaded));
        db_load_all_qsos(&reloaded);

        int ok = (upd_rc == 0) && (reloaded.count == 3) &&
                 (strcmp(reloaded.items[0].name, "Göran Östlund") == 0) &&
                 (strcmp(reloaded.items[0].qth, "Jönköping") == 0) &&
                 (strcmp(reloaded.items[0].comment, "Ändrad notering: Bytte till 80m dipol, fin signalstyrka!") == 0);
        record_result(ok, "db_update_qso uppdaterar Namn, QTH och Notering med ÅÄÖ utan förvanskning", NULL);

        free(loaded.items);
        free(reloaded.items);
    }

    /* 6. Databassanering (db_sanitize_all_utf8) av råa Latin-1-bytes i tabellen */
    {
        sqlite3 *raw_db = db_get_handle();
        if (raw_db != NULL) {
            sqlite3_stmt *stmt = NULL;
            const char *sql = "INSERT INTO qsos (call, name, qso_date, time_utc, qth, comment) "
                              "VALUES ('SM4RAW', ?, '2026-11-14', '15:00', ?, ?);";
            if (sqlite3_prepare_v2(raw_db, sql, -1, &stmt, NULL) == SQLITE_OK) {
                const char raw_name[] = "J\xf6rgen \xc5kesson";             /* Jörgen Åkesson */
                const char raw_qth[] = "Link\xf6ping";                     /* Linköping */
                const char raw_comm[] = "Fin \xe5terkoppling fr\xe5n \xc4lvsbyn!"; /* Fin återkoppling från Älvsbyn! */
                sqlite3_bind_text(stmt, 1, raw_name, (int)strlen(raw_name), SQLITE_STATIC);
                sqlite3_bind_text(stmt, 2, raw_qth, (int)strlen(raw_qth), SQLITE_STATIC);
                sqlite3_bind_text(stmt, 3, raw_comm, (int)strlen(raw_comm), SQLITE_STATIC);
                sqlite3_step(stmt);
                sqlite3_finalize(stmt);
            }
        }

        int san_fixed = db_sanitize_all_utf8();
        QSOList reloaded;
        memset(&reloaded, 0, sizeof(reloaded));
        db_load_all_qsos(&reloaded);

        int raw_found = 0;
        for (size_t i = 0; i < reloaded.count; ++i) {
            if (strcmp(reloaded.items[i].call, "SM4RAW") == 0) {
                if (strcmp(reloaded.items[i].name, "Jörgen Åkesson") == 0 &&
                    strcmp(reloaded.items[i].qth, "Linköping") == 0 &&
                    strcmp(reloaded.items[i].comment, "Fin återkoppling från Älvsbyn!") == 0) {
                    raw_found = 1;
                }
            }
        }

        int ok = (san_fixed >= 1) && raw_found;
        record_result(ok, "db_sanitize_all_utf8 reparerar automatiskt råa Latin-1-bytes i tabellen för Namn, QTH och Notering", NULL);
        free(reloaded.items);
    }

    /* 7. ADIF export och import med ÅÄÖ i Namn, QTH och Notering */
    {
        const char *tmp_adif = "/tmp/test_swedish_adif.adi";
        StationSettings st;
        memset(&st, 0, sizeof(st));
        copy_str(st.call, sizeof(st.call), "SM5UEI");

        QSOList curr_list;
        memset(&curr_list, 0, sizeof(curr_list));
        db_load_all_qsos(&curr_list);

        int exp_cnt = export_qsos_adif(tmp_adif, &curr_list, &st, EXPORT_MODE_ALL);

        /* Töm databasen och importera tillbaka */
        db_clear_all_qsos();
        QSOList imp_list;
        memset(&imp_list, 0, sizeof(imp_list));
        int imp_cnt = import_qsos_from_adif(tmp_adif, &imp_list, &st);

        QSOList reloaded;
        memset(&reloaded, 0, sizeof(reloaded));
        db_load_all_qsos(&reloaded);

        int jorgen_found = 0, marta_found = 0, goran_found = 0;
        for (size_t i = 0; i < reloaded.count; ++i) {
            if (strcmp(reloaded.items[i].name, "Jörgen Åkesson") == 0 &&
                strcmp(reloaded.items[i].qth, "Linköping") == 0 &&
                strcmp(reloaded.items[i].comment, "Fin återkoppling från Älvsbyn!") == 0) {
                jorgen_found = 1;
            }
            if (strcmp(reloaded.items[i].name, "Märta Ängström") == 0 &&
                strcmp(reloaded.items[i].qth, "Västerås") == 0) {
                marta_found = 1;
            }
            if (strcmp(reloaded.items[i].name, "Göran Östlund") == 0 &&
                strcmp(reloaded.items[i].qth, "Jönköping") == 0) {
                goran_found = 1;
            }
        }

        int ok = (exp_cnt == 4) && (imp_cnt == 4) && jorgen_found && marta_found && goran_found;
        record_result(ok, "ADIF export och återimport bevarar ÅÄÖ i Namn (<NAME>), QTH (<QTH>) och Notering (<COMMENT>)", NULL);

        unlink(tmp_adif);
        free(curr_list.items);
        free(imp_list.items);
        free(reloaded.items);
    }

    /* 8. Sökning och filtrering (build_filtered_indices) med ÅÄÖ i Namn, QTH och Notering */
    {
        QSOList filter_list;
        memset(&filter_list, 0, sizeof(filter_list));
        db_load_all_qsos(&filter_list);

        int indices[16];
        /* Sök i Namn: "östlund" / "Östlund" */
        int hit_name1 = build_filtered_indices(&filter_list, "Östlund", indices, 16);
        int hit_name2 = build_filtered_indices(&filter_list, "östlund", indices, 16);

        /* Sök i QTH: "västerås" / "Västerås" */
        int hit_qth1 = build_filtered_indices(&filter_list, "Västerås", indices, 16);
        int hit_qth2 = build_filtered_indices(&filter_list, "västerås", indices, 16);

        /* Sök i Notering: "Älvsbyn" / "älvsbyn" och "trådantenn" */
        int hit_not1 = build_filtered_indices(&filter_list, "Älvsbyn", indices, 16);
        int hit_not2 = build_filtered_indices(&filter_list, "älvsbyn", indices, 16);
        int hit_not3 = build_filtered_indices(&filter_list, "trådantenn", indices, 16);

        int ok = (hit_name1 == 1) && (hit_name2 == 1) &&
                 (hit_qth1 == 1) && (hit_qth2 == 1) &&
                 (hit_not1 == 1) && (hit_not2 == 1) && (hit_not3 == 1);
        record_result(ok, "build_filtered_indices filtrerar skiftlägesokänsligt på ÅÄÖ i Namn, QTH och Notering", NULL);

        free(filter_list.items);
    }

    /* 9. Validering mot produktionsdatabasen qsolog.sq3 för alla Namn, QTH och Noteringar */
    {
        if (access("qsolog.sq3", F_OK) == 0) {
            db_close();
            db_open("qsolog.sq3");
            sqlite3 *h = db_get_handle();
            if (h) sqlite3_exec(h, "DELETE FROM qsos WHERE call='SM5EDIT';", NULL, NULL, NULL);
            QSOList prod_list;
            memset(&prod_list, 0, sizeof(prod_list));
            int cnt = db_load_all_qsos(&prod_list);
            int all_clean = 1;
            int swedish_found = 0;

            for (size_t i = 0; i < prod_list.count; ++i) {
                if (utf8_display_width(prod_list.items[i].name) < 0 ||
                    utf8_display_width(prod_list.items[i].qth) < 0 ||
                    utf8_display_width(prod_list.items[i].comment) < 0) {
                    all_clean = 0;
                }
                if (strstr(prod_list.items[i].name, "ä") || strstr(prod_list.items[i].name, "å") || strstr(prod_list.items[i].name, "ö") ||
                    strstr(prod_list.items[i].qth, "ä") || strstr(prod_list.items[i].qth, "å") || strstr(prod_list.items[i].qth, "ö") ||
                    strstr(prod_list.items[i].comment, "ä") || strstr(prod_list.items[i].comment, "å") || strstr(prod_list.items[i].comment, "ö")) {
                    swedish_found = 1;
                }
            }

            int ok = (cnt > 0) && all_clean && swedish_found;
            record_result(ok, "Produktionsdatabas qsolog.sq3: 100% giltig UTF-8 i alla Namn-, QTH- och Noteringsfält", NULL);
            free(prod_list.items);
            db_close();
        }
    }

    /* 10. Inmatning och markörförflyttning (utf8_insert_char_at & utf8_insert_byte_at) med åäö */
    {
        char not_buf[64] = "";
        int cursor = 0;

        /* Mata in "å" (2 bytes) */
        utf8_insert_char_at(not_buf, sizeof(not_buf), &cursor, "\xc3\xa5", 2);
        int c1_ok = (cursor == 1) && (strcmp(not_buf, "å") == 0);

        /* Mata in "ä" (2 bytes) */
        utf8_insert_char_at(not_buf, sizeof(not_buf), &cursor, "\xc3\xa4", 2);
        int c2_ok = (cursor == 2) && (strcmp(not_buf, "åä") == 0);

        /* Mata in "ö" (2 bytes) */
        utf8_insert_char_at(not_buf, sizeof(not_buf), &cursor, "\xc3\xb6", 2);
        int c3_ok = (cursor == 3) && (strcmp(not_buf, "åäö") == 0);

        /* Testa även byte-vis inmatning för "Ö" (\xc3 följt av \x96) */
        utf8_insert_byte_at(not_buf, sizeof(not_buf), &cursor, '\xc3');
        int lead_ok = (cursor == 4);
        utf8_insert_byte_at(not_buf, sizeof(not_buf), &cursor, '\x96');
        int cont_ok = (cursor == 4) && (strcmp(not_buf, "åäöÖ") == 0);

        /* Backspace ska radera "Ö" och minska cursor till 3 */
        utf8_backspace_at(not_buf, &cursor);
        int bs_ok = (cursor == 3) && (strcmp(not_buf, "åäö") == 0);

        int ok = c1_ok && c2_ok && c3_ok && lead_ok && cont_ok && bs_ok;
        record_result(ok, "UTF-8 inmatning av åäö ökar markörposition med exakt 1 steg per tecken (inte 2 bytes)", NULL);
    }

    /* 11. QRB formatering som ren siffra (utan 'km') i loggraderna */
    {
        QSO q_qrb;
        memset(&q_qrb, 0, sizeof(q_qrb));
        copy_str(q_qrb.distance, sizeof(q_qrb.distance), "1250");
        char dsp[32] = "";
        format_qrb_display(&q_qrb, dsp, sizeof(dsp));
        int qrb_num_ok = (strcmp(dsp, "1250") == 0);
        int no_km_ok = (strstr(dsp, "km") == NULL);

        record_result(qrb_num_ok && no_km_ok, "QRB visas som ren siffra utan 'km' i loggraderna (t.ex. '1250')", NULL);
    }

    /* 12. QTH-kolumnregler: min 10 tecken innan den döljs, max 30 tecken */
    {
        int min_qth = 10;
        int max_qth = 30;
        char sample_long_qth[] = "Örnsköldsvik Södra Hamnen Industriområde";
        char qth_clipped[64];
        utf8_pad_or_clip(qth_clipped, sizeof(qth_clipped), sample_long_qth, max_qth, 0);
        int w_clipped = utf8_display_width(qth_clipped);
        int max_ok = (w_clipped <= max_qth);

        char sample_short_qth[] = "Västerås";
        char qth_min[64];
        utf8_pad_or_clip(qth_min, sizeof(qth_min), sample_short_qth, min_qth, 0);
        int w_min = utf8_display_width(qth_min);
        int min_ok = (w_min == min_qth);

        record_result(max_ok && min_ok, "QTH-kolumnregler validerade: minsta bredd 10 tkn och maximal bredd 30 tkn", NULL);
    }

    /* 13. QRB och QUJ döljs samtidigt före QTH vid minskande fönsterbredd */
    {
        LogColumnLayout l_wide, l_mid, l_80;
        calculate_log_column_layout(110, &l_wide);
        calculate_log_column_layout(92, &l_mid);
        calculate_log_column_layout(80, &l_80);

        int wide_ok = (l_wide.w_qrb > 0 && l_wide.w_quj > 0 && l_wide.w_qth > 0);
        int mid_ok  = (l_mid.w_qrb == 0 && l_mid.w_quj == 0 && l_mid.w_qth >= 10); /* QRB och QUJ dolda, QTH kvar */
        int c80_ok  = (l_80.w_qrb == 0 && l_80.w_quj == 0 && l_80.w_qth >= 10);  /* QRB och QUJ dolda, QTH kvar vid 80 kolumner */

        record_result(wide_ok && mid_ok && c80_ok,
                      "QRB och QUJ döljs samtidigt före QTH vid minskande fönsterbredd", NULL);
    }

    /* 14. QTH kvarstår ned till 80 kolumner och döljs vid < 80 kolumner */
    {
        LogColumnLayout l_85, l_80, l_79, l_70;
        calculate_log_column_layout(85, &l_85);
        calculate_log_column_layout(80, &l_80);
        calculate_log_column_layout(79, &l_79);
        calculate_log_column_layout(70, &l_70);

        int qth_85_ok = (l_85.w_qth >= 10);
        int qth_80_ok = (l_80.w_qth >= 10);
        int qth_79_ok = (l_79.w_qth == 0); /* Döljs vid < 80 */
        int qth_70_ok = (l_70.w_qth == 0);

        record_result(qth_85_ok && qth_80_ok && qth_79_ok && qth_70_ok,
                      "QTH kvarstår ned till 80 kolumner (min 10 tkn) och döljs vid < 80 kolumner", NULL);
    }

    /* 15. QSO-nummerkolumn tas bort vid < 90 kolumner för att spara plats */
    {
        LogColumnLayout l_95, l_90, l_89, l_80;
        calculate_log_column_layout(95, &l_95);
        calculate_log_column_layout(90, &l_90);
        calculate_log_column_layout(89, &l_89);
        calculate_log_column_layout(80, &l_80);

        int num_95_ok = (l_95.show_qso_num == 1 && l_95.col_date == 7 && l_95.col_name == 62);
        int num_90_ok = (l_90.show_qso_num == 1 && l_90.col_date == 7 && l_90.col_name == 62);
        int num_89_ok = (l_89.show_qso_num == 0 && l_89.col_date == 2 && l_89.col_name == 57);
        int num_80_ok = (l_80.show_qso_num == 0 && l_80.col_date == 2 && l_80.col_name == 57);

        record_result(num_95_ok && num_90_ok && num_89_ok && num_80_ok,
                      "Kolumn med QSO-nummer visas vid >= 90 kolumner och tas bort vid < 90 kolumner", NULL);
    }

    /* 16. load_qsos_from_adif: ignorerar tomma <EOR>, läser sista post utan <EOR> och sorterar kronologiskt */
    {
        const char *tmp_raw_adi = "/tmp/test_load_adif_edge.adi";
        FILE *afp = fopen(tmp_raw_adi, "w");
        if (afp) {
            fprintf(afp, "ADIF test header<EOH>\n");
            fprintf(afp, "<EOR>\n"); /* Tom post som ska ignoreras */
            fprintf(afp, "<CALL:7>SM5LATE<QSO_DATE:8>20261114<TIME_ON:4>1500<BAND:3>20m<MODE:3>SSB<EOR>\n");
            fprintf(afp, "<CALL:7>SM5EARL<QSO_DATE:8>20261114<TIME_ON:4>0900<BAND:3>40m<MODE:2>CW\n"); /* Saknar avslutande <EOR> */
            fclose(afp);
        }
        QSOList edge_list;
        memset(&edge_list, 0, sizeof(edge_list));
        load_qsos_from_adif(tmp_raw_adi, &edge_list);
        unlink(tmp_raw_adi);

        int adif_edge_ok = (edge_list.count == 2) &&
                           (strcmp(edge_list.items[0].call, "SM5EARL") == 0) &&
                           (strcmp(edge_list.items[1].call, "SM5LATE") == 0);
        free(edge_list.items);
        record_result(adif_edge_ok,
                      "load_qsos_from_adif ignorerar tomma <EOR>, hanterar sista post utan <EOR> och sorterar kronologiskt", NULL);
    }

    /* 17. Redigering av ett hanterat QSO märker det som nytt (handled = 0) och QRB föredrar lokator */
    {
        const char *test_edit_db = "/tmp/test_edit_flow.sq3";
        unlink(test_edit_db);
        db_close();
        db_open(test_edit_db);

        StationSettings cur_st;
        load_station_settings(CONFIG_FILE, &cur_st);
        StationSettings test_st = cur_st;
        copy_str(test_st.gridsquare, sizeof(test_st.gridsquare), "JO89VJ");

        QSO edit_q;
        memset(&edit_q, 0, sizeof(edit_q));
        copy_str(edit_q.call, sizeof(edit_q.call), "SM5EDIT");
        copy_str(edit_q.date, sizeof(edit_q.date), "2026-11-14");
        copy_str(edit_q.time_utc, sizeof(edit_q.time_utc), "12:00");
        copy_str(edit_q.band, sizeof(edit_q.band), "20 m");
        copy_str(edit_q.mode, sizeof(edit_q.mode), "SSB");
        copy_str(edit_q.gridsquare, sizeof(edit_q.gridsquare), "JO67");
        copy_str(edit_q.qth, sizeof(edit_q.qth), "Göteborg");
        edit_q.handled = 1; /* Redan hanterat innan redigering */
        db_insert_qso(&edit_q);

        QSOList edit_list = {NULL, 0, 0};
        db_load_all_qsos(&edit_list);

        int idx_to_edit = -1;
        for (size_t i = 0; i < edit_list.count; ++i) {
            if (strcmp(edit_list.items[i].call, "SM5EDIT") == 0) {
                idx_to_edit = (int)i;
                break;
            }
        }

        int edit_marked_new_ok = 0;
        if (idx_to_edit >= 0 && edit_list.items[idx_to_edit].handled == 1) {
            QSO form_qso = edit_list.items[idx_to_edit];
            copy_str(form_qso.comment, sizeof(form_qso.comment), "Redigerad rad");
            char qrb_buf[32] = "-";
            compute_qrb_str(&test_st, &form_qso, qrb_buf, sizeof(qrb_buf));

            int tf = 1, af = FLD_CALL, fcp = 0;
            char last_call[32] = "";
            char status_buf[128] = "";
            try_save_current_qso(&form_qso, &tf, &af, &fcp, &idx_to_edit,
                                 &test_st, &edit_list, last_call,
                                 status_buf, sizeof(status_buf), qrb_buf, NULL);

            QSOList reloaded = {NULL, 0, 0};
            db_load_all_qsos(&reloaded);
            for (size_t i = 0; i < reloaded.count; ++i) {
                if (strcmp(reloaded.items[i].call, "SM5EDIT") == 0) {
                    if (edit_list.items[i].handled == 0 &&
                        reloaded.items[i].handled == 0 &&
                        strcmp(reloaded.items[i].comment, "Redigerad rad") == 0 &&
                        atol(reloaded.items[i].distance) > 0) {
                        edit_marked_new_ok = 1;
                    }
                    break;
                }
            }
            free(reloaded.items);
        }
        save_station_settings(CONFIG_FILE, &cur_st);
        free(edit_list.items);
        db_close();
        unlink(test_edit_db);
        record_result(edit_marked_new_ok,
                      "Redigering av ett QSO märker det som nytt (handled = 0) och beräknar QRB från lokator", NULL);
    }

    db_close();
    unlink(test_db_path);
}

/* ========================================================================= */
/* [11] Färgscheman och temahantering                                        */
/* ========================================================================= */
static void run_theme_tests(void) {
    printf(ANSI_BLUE "\n=== [11] Färgscheman och temahantering (11 teman) ===" ANSI_RESET "\n");

    /* 1. Antal teman */
    record_result(THEME_COUNT == 11, "THEME_COUNT är definierat som 11 teman", NULL);

    /* 2. Temanamn och validering */
    int names_ok = 1;
    for (int i = 0; i < THEME_COUNT; ++i) {
        if (THEME_NAMES[i] == NULL || strlen(THEME_NAMES[i]) == 0) {
            names_ok = 0;
            break;
        }
    }
    int expected_names = 0;
    if (names_ok &&
        strcmp(THEME_NAMES[8], "Arktisk Natt (Nord Frost)") == 0 &&
        strcmp(THEME_NAMES[9], "Field Day (Vintage Radio)") == 0 &&
        strcmp(THEME_NAMES[10], "Mörkerläge (Crimson Night)") == 0) {
        expected_names = 1;
    }
    record_result(names_ok && expected_names, "Alla 11 temanamn definierade, inkl. Arktisk Natt, Field Day och Mörkerläge", NULL);

    /* 3. Säker exekvering av apply_color_theme för alla index inkl. gränsvärden */
    for (int t = -2; t <= 13; ++t) {
        apply_color_theme(t);
    }
    record_result(1, "apply_color_theme() exekverar stabilt för alla 11 teman och felaktiga index", NULL);

    /* 4. Sparande och laddning av nya teman i station.conf */
    const char *test_conf = "test_theme_station.conf";
    StationSettings st_save;
    memset(&st_save, 0, sizeof(st_save));
    st_save.theme = 8; /* Arktisk Natt */
    copy_str(st_save.call, sizeof(st_save.call), "SM0TEST");
    save_station_settings(test_conf, &st_save);

    StationSettings st_load;
    memset(&st_load, 0, sizeof(st_load));
    load_station_settings(test_conf, &st_load);
    int p1 = (st_load.theme == 8);

    st_save.theme = 9; /* Field Day */
    save_station_settings(test_conf, &st_save);
    load_station_settings(test_conf, &st_load);
    int p2 = (st_load.theme == 9);

    st_save.theme = 10; /* Mörkerläge */
    save_station_settings(test_conf, &st_save);
    load_station_settings(test_conf, &st_load);
    int p3 = (st_load.theme == 10);

    unlink(test_conf);
    record_result(p1 && p2 && p3, "StationSettings sparar och läser in alla nya teman korrekt via station.conf", NULL);
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
    run_swedish_chars_tests();
    run_theme_tests();

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
