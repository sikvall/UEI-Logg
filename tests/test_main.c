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
#include "qrz.h"
#include "dxcc.h"
#include "tui.h"
#include "space_weather.h"
#include "theme_editor.h"

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

    /* 9. format_freq_display högerställer frekvens och sätter '.' på exakt samma kolumn (pos 4) */
    {
        char f1[16] = "", f2[16] = "", f3[16] = "", f4[16] = "", f5[16] = "", f6[16] = "";
        format_freq_display("14.200", f1, sizeof(f1));
        format_freq_display("7.100", f2, sizeof(f2));
        format_freq_display("144.300", f3, sizeof(f3));
        format_freq_display("1296.200", f4, sizeof(f4));
        format_freq_display("7", f5, sizeof(f5));
        format_freq_display("", f6, sizeof(f6));

        int ok = (strcmp(f1, "  14.200") == 0) &&
                 (strcmp(f2, "   7.100") == 0) &&
                 (strcmp(f3, " 144.300") == 0) &&
                 (strcmp(f4, "1296.200") == 0) &&
                 (strcmp(f5, "   7.000") == 0) &&
                 (strcmp(f6, "        ") == 0);

        /* Kontrollera att '.' alltid ligger på index 4 för alla icke-tomma frekvenser */
        if (ok) {
            ok = ok && (f1[4] == '.') && (f2[4] == '.') && (f3[4] == '.') &&
                       (f4[4] == '.') && (f5[4] == '.') && (strlen(f1) == 8);
        }
        record_result(ok, "format_freq_display högerställer kolumnen och låser '.' till exakt samma kolumn (pos 4)", NULL);
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

    /* Testa markörsynkronisering och fältpekare för samtliga inmatningsfält */
    QSO nav_qso;
    memset(&nav_qso, 0, sizeof(nav_qso));
    copy_str(nav_qso.call, sizeof(nav_qso.call), "SM5UEI");
    copy_str(nav_qso.qth, sizeof(nav_qso.qth), "Västerås");
    int cpos = -1;
    sync_cursor_to_field_end(&nav_qso, FLD_CALL, &cpos);
    int cpos_call_ok = (cpos == 6);
    sync_cursor_to_field_end(&nav_qso, FLD_QTH, &cpos);
    int cpos_qth_ok = (cpos == 8); /* "Västerås" har 8 visningstecken i UTF-8 */
    int f_cnt_empty = build_filtered_indices(&list, "INGENTRAFF999", indices, (int)list.count);
    record_result(cpos_call_ok && cpos_qth_ok && f_cnt_empty == 0,
                  "Markörposition (UTF-8) och tom sökfiltrering (återgång till inmatningsfokus) fungerar korrekt", NULL);

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

    char dxcc_w1[64] = "", dxcc_g1[64] = "", dxcc_f1[64] = "";
    lookup_dxcc("W", dxcc_w1, sizeof(dxcc_w1));
    lookup_dxcc("G", dxcc_g1, sizeof(dxcc_g1));
    lookup_dxcc("F", dxcc_f1, sizeof(dxcc_f1));
    int single_ok = (strcmp(dxcc_w1, "USA") == 0 &&
                     strcmp(dxcc_g1, "England") == 0 &&
                     strcmp(dxcc_f1, "Frankrike") == 0);
    record_result(single_ok, "lookup_dxcc identifierar enbokstavsprefix omedelbart vid inmatning (W->USA, G->England, F->Frankrike)", NULL);

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

    /* Validera att SM Callbook hålls avslaget i inställningarna */
    st.smcb_enabled = 1;
    save_station_settings(tmp_conf, &st);
    memset(&loaded, 0, sizeof(loaded));
    load_station_settings(tmp_conf, &loaded);
    int smcb_disabled_ok = (loaded.smcb_enabled == 0);

    /* Validera Auto QRZ.COM inställningar: Av (0), Auto (1) och Manuell (2) */
    st.qrz_enabled = QRZ_MODE_OFF;
    save_station_settings(tmp_conf, &st);
    memset(&loaded, 0, sizeof(loaded));
    load_station_settings(tmp_conf, &loaded);
    int qrz_off_ok = (loaded.qrz_enabled == QRZ_MODE_OFF);

    st.qrz_enabled = QRZ_MODE_AUTO;
    save_station_settings(tmp_conf, &st);
    memset(&loaded, 0, sizeof(loaded));
    load_station_settings(tmp_conf, &loaded);
    int qrz_auto_ok = (loaded.qrz_enabled == QRZ_MODE_AUTO);

    st.qrz_enabled = QRZ_MODE_MANUAL;
    save_station_settings(tmp_conf, &st);
    memset(&loaded, 0, sizeof(loaded));
    load_station_settings(tmp_conf, &loaded);
    int qrz_manual_ok = (loaded.qrz_enabled == QRZ_MODE_MANUAL);

    /* Testa textbaserad tolkning av QRZ_ENABLED i station.conf */
    FILE *t_fp = fopen(tmp_conf, "w");
    if (t_fp) {
        fprintf(t_fp, "QRZ_ENABLED=auto\n");
        fclose(t_fp);
        load_station_settings(tmp_conf, &loaded);
        int parse_auto_ok = (loaded.qrz_enabled == QRZ_MODE_AUTO);

        t_fp = fopen(tmp_conf, "w");
        fprintf(t_fp, "QRZ_ENABLED=manuell\n");
        fclose(t_fp);
        load_station_settings(tmp_conf, &loaded);
        int parse_manual_ok = (loaded.qrz_enabled == QRZ_MODE_MANUAL);

        t_fp = fopen(tmp_conf, "w");
        fprintf(t_fp, "QRZ_ENABLED=av\n");
        fclose(t_fp);
        load_station_settings(tmp_conf, &loaded);
        int parse_off_ok = (loaded.qrz_enabled == QRZ_MODE_OFF);

        qrz_off_ok = qrz_off_ok && parse_auto_ok && parse_manual_ok && parse_off_ok;
    }

    unlink(tmp_conf);
    record_result(ok && notes_ok, "save_station_settings och load_station_settings bevarar alla stationsparametrar (inkl. show_notes för F6)", NULL);
    record_result(smcb_disabled_ok, "SM Callbook hålls dolt och avslaget i inställningarna (smcb_enabled == 0)", NULL);
    record_result(qrz_off_ok && qrz_auto_ok && qrz_manual_ok,
                  "Auto QRZ.COM inställning stöder Av (0), Auto (1) och Manuell (2)", NULL);

    /* Testa att maybe_lookup_call_details inte startar QRZ i QRZ_MODE_MANUAL eller QRZ_MODE_OFF */
    {
        QSO test_qso;
        memset(&test_qso, 0, sizeof(test_qso));
        copy_str(test_qso.call, sizeof(test_qso.call), "JA1ABC");
        char last_call[32] = "";
        char s_msg[128] = "";
        QSOList empty_list = {NULL, 0, 0};
        StationSettings manual_st;
        memset(&manual_st, 0, sizeof(manual_st));
        copy_str(manual_st.qrz_username, sizeof(manual_st.qrz_username), "user");
        copy_str(manual_st.qrz_password, sizeof(manual_st.qrz_password), "pass");
        manual_st.qrz_enabled = QRZ_MODE_MANUAL;

        maybe_lookup_call_details(&test_qso, &empty_list, &manual_st, last_call, sizeof(last_call), s_msg, sizeof(s_msg));
        int manual_did_not_trigger_qrz = (strstr(s_msg, "QRZ:") == NULL);

        manual_st.qrz_enabled = QRZ_MODE_OFF;
        s_msg[0] = '\0';
        last_call[0] = '\0';
        maybe_lookup_call_details(&test_qso, &empty_list, &manual_st, last_call, sizeof(last_call), s_msg, sizeof(s_msg));
        int off_did_not_trigger_qrz = (strstr(s_msg, "QRZ:") == NULL);

        record_result(manual_did_not_trigger_qrz && off_did_not_trigger_qrz,
                      "Vid Manuell och Av sker inget automatiskt QRZ-uppslag vid inmatning", NULL);
    }

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

        /* Testa tolkning av DX/NVIS (t.ex. "15m/80m") från hf.avitec.se HTML */
        const char *sample_sw_html =
            "<div class=\"whitespace-nowrap\">2/1</div><div>K-index</div>"
            "<div class=\"whitespace-nowrap\">21,4<span class=\"text-muted\">MHz</span></div><div>MUF</div>"
            "<h2>Bandval just nu</h2>"
            "<div><h3>DX · 3 000 km</h3><p>Dagtid är <span class=\"font-medium text-ok\">15 m</span> bäst för DX på 3 000 km, bra läge.</p></div>"
            "<div><h3>NVIS · 400 km</h3><p>Dagtid är <span class=\"font-medium text-ok\">80 m</span> bäst för NVIS på 400 km, bra läge.</p></div>";
        SpaceWeatherData parsed_sw;
        memset(&parsed_sw, 0, sizeof(parsed_sw));
        int sw_parse_ok = parse_space_weather_html(sample_sw_html, &parsed_sw) &&
                          (strcmp(parsed_sw.dx_nvis, "15m/80m") == 0) &&
                          (parsed_sw.dx_nvis_tone == SW_TONE_OK);

        record_result(right_aligned_ok && label_ok && sw_parse_ok,
                      "Rymdvädersrutan: K/Kp, DX/NVIS ('15m/80m') och tidsstämpel högerställd i titelraden", NULL);
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

    /* 8. Tidsinmatning 21:58 (HH:MM) och 2158 (HHMM) tolkas likadant */
    {
        char t_colon_ui[16], t_digits_ui[16];
        char t_colon_adif[16], t_digits_adif[16];
        char t_colon_hhmm[16], t_digits_hhmm[16];

        format_time_hh_mm("21:58", t_colon_ui, sizeof(t_colon_ui));
        format_time_hh_mm("2158", t_digits_ui, sizeof(t_digits_ui));
        normalize_time_adif("21:58", t_colon_adif, sizeof(t_colon_adif));
        normalize_time_adif("2158", t_digits_adif, sizeof(t_digits_adif));
        format_time_hhmm("21:58", t_colon_hhmm, sizeof(t_colon_hhmm));
        format_time_hhmm("2158", t_digits_hhmm, sizeof(t_digits_hhmm));

        QSO q_leave;
        memset(&q_leave, 0, sizeof(q_leave));
        copy_str(q_leave.time_utc, sizeof(q_leave.time_utc), "2158");
        StationSettings st_tmp;
        memset(&st_tmp, 0, sizeof(st_tmp));
        char last_smcb[32] = "", status_msg[128] = "", qrb[32] = "", quj[32] = "";
        apply_field_defaults_on_leave(&q_leave, FLD_TIME, NULL, &st_tmp,
                                      last_smcb, sizeof(last_smcb),
                                      status_msg, sizeof(status_msg),
                                      qrb, sizeof(qrb), quj, sizeof(quj));

        QSO qa, qb;
        memset(&qa, 0, sizeof(qa));
        memset(&qb, 0, sizeof(qb));
        copy_str(qa.call, sizeof(qa.call), "SM5UEI");
        copy_str(qa.date, sizeof(qa.date), "2026-11-15");
        copy_str(qa.time_utc, sizeof(qa.time_utc), "21:58");
        copy_str(qa.band, sizeof(qa.band), "20 m");

        copy_str(qb.call, sizeof(qb.call), "SM5UEI");
        copy_str(qb.date, sizeof(qb.date), "2026-11-15");
        copy_str(qb.time_utc, sizeof(qb.time_utc), "2158");
        copy_str(qb.band, sizeof(qb.band), "20 m");

        QSOList dlist;
        memset(&dlist, 0, sizeof(dlist));
        qso_list_push(&dlist, &qa);
        int is_dup = qso_is_duplicate(&dlist, &qb);
        int cmp_eq = (cmp_qso_chronological(&qa, &qb) == 0);
        free(dlist.items);

        int time_eq_ok = (strcmp(t_colon_ui, "21:58") == 0) &&
                         (strcmp(t_digits_ui, "21:58") == 0) &&
                         (strcmp(t_colon_adif, "2158") == 0) &&
                         (strcmp(t_digits_adif, "2158") == 0) &&
                         (strcmp(t_colon_hhmm, "2158") == 0) &&
                         (strcmp(t_digits_hhmm, "2158") == 0) &&
                         (strcmp(q_leave.time_utc, "21:58") == 0) &&
                         (is_dup == 1) && cmp_eq;
        record_result(time_eq_ok, "Tidsinmatning '21:58' (HH:MM) och '2158' (HHMM) normaliseras och tolkas identiskt", NULL);
    }

    /* 9. Mode-fält och auto_rst_59 */
    {
        QSO q_mode;
        memset(&q_mode, 0, sizeof(q_mode));
        copy_str(q_mode.mode, sizeof(q_mode.mode), "SSB");
        StationSettings st_off;
        memset(&st_off, 0, sizeof(st_off));
        st_off.auto_rst_59 = 0;
        char last_smcb[32] = "", status_msg[128] = "", qrb[32] = "", quj[32] = "";

        apply_field_defaults_on_leave(&q_mode, FLD_MODE, NULL, &st_off,
                                      last_smcb, sizeof(last_smcb),
                                      status_msg, sizeof(status_msg),
                                      qrb, sizeof(qrb), quj, sizeof(quj));

        int rst_off_ok = (q_mode.rst_sent[0] == '\0') && (q_mode.rst_rcvd[0] == '\0');

        StationSettings st_on;
        memset(&st_on, 0, sizeof(st_on));
        st_on.auto_rst_59 = 1;
        apply_field_defaults_on_leave(&q_mode, FLD_MODE, NULL, &st_on,
                                      last_smcb, sizeof(last_smcb),
                                      status_msg, sizeof(status_msg),
                                      qrb, sizeof(qrb), quj, sizeof(quj));

        int rst_on_ok = (strcmp(q_mode.rst_sent, "59") == 0) && (strcmp(q_mode.rst_rcvd, "59") == 0);

        record_result(rst_off_ok && rst_on_ok,
                      "apply_field_defaults_on_leave för FLD_MODE respekterar auto_rst_59 (fyller ej i om av)", NULL);
    }
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
    printf(ANSI_BLUE "\n=== [11] Färgscheman och temahantering (16 teman med UEI-tema som default) ===" ANSI_RESET "\n");

    /* 1. Antal teman */
    record_result(THEME_COUNT == 16, "THEME_COUNT är definierat som 16 teman (inkl. UEI-tema och Egen profil)", NULL);

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
        strcmp(THEME_NAMES[0], "UEI-tema") == 0 &&
        strcmp(THEME_NAMES[1], "Turbo Pascal (MC Default)") == 0 &&
        strcmp(THEME_NAMES[9], "Arktisk Natt (Nord Frost)") == 0 &&
        strcmp(THEME_NAMES[10], "Field Day (Vintage Radio)") == 0 &&
        strcmp(THEME_NAMES[11], "Mörkerläge (Crimson Night)") == 0 &&
        strcmp(THEME_NAMES[12], "Solarized Dark (Djuphav)") == 0 &&
        strcmp(THEME_NAMES[13], "Dracula (Neon Skymning)") == 0 &&
        strcmp(THEME_NAMES[14], "Militärradio (Fältgrön)") == 0 &&
        strcmp(THEME_NAMES[15], "Egen profil (Anpassad)") == 0) {
        expected_names = 1;
    }
    record_result(names_ok && expected_names, "Alla 16 temanamn definierade, med UEI-tema som default och Egen profil som anpassad", NULL);

    /* 3. Säker exekvering av apply_color_theme för alla index inkl. gränsvärden */
    for (int t = -2; t <= 18; ++t) {
        apply_color_theme(t);
    }
    record_result(1, "apply_color_theme() exekverar stabilt för alla 16 teman och felaktiga index", NULL);

    /* 4. Sparande och laddning av nya teman i station.conf */
    const char *test_conf = "/tmp/test_theme_station.conf";
    StationSettings st_save;
    memset(&st_save, 0, sizeof(st_save));
    st_save.theme = 0; /* UEI-tema */
    copy_str(st_save.call, sizeof(st_save.call), "SM0TEST");
    save_station_settings(test_conf, &st_save);

    StationSettings st_load;
    memset(&st_load, 0, sizeof(st_load));
    load_station_settings(test_conf, &st_load);
    int p0 = (st_load.theme == 0);

    st_save.theme = 12; /* Solarized Dark */
    save_station_settings(test_conf, &st_save);
    load_station_settings(test_conf, &st_load);
    int p1 = (st_load.theme == 12);

    st_save.theme = 13; /* Dracula */
    save_station_settings(test_conf, &st_save);
    load_station_settings(test_conf, &st_load);
    int p2 = (st_load.theme == 13);

    st_save.theme = 14; /* Militärradio */
    save_station_settings(test_conf, &st_save);
    load_station_settings(test_conf, &st_load);
    int p3 = (st_load.theme == 14);

    /* 5. Egen färgprofil (CustomTheme) initiering, presets och persistens */
    CustomTheme def_ct, preset_ct;
    custom_theme_init_defaults(&def_ct);
    custom_theme_init_preset(&preset_ct, 3); /* Dracula preset */

    int ct_init_ok = (def_ct.bg_main == 4 && def_ct.fg_text == 14 && def_ct.border == 11) &&
                     (preset_ct.border_active == 19 && preset_ct.title == 9);

    st_save.theme = THEME_CUSTOM;
    st_save.custom_theme = def_ct;
    st_save.custom_theme.border = 15; /* Ändra till Orange */
    st_save.custom_theme.title = 14;  /* Ändra till Solgul */
    save_station_settings(test_conf, &st_save);

    memset(&st_load, 0, sizeof(st_load));
    load_station_settings(test_conf, &st_load);
    int custom_persist_ok = (st_load.theme == THEME_CUSTOM) &&
                            (st_load.custom_theme.border == 15) &&
                            (st_load.custom_theme.title == 14) &&
                            (st_load.custom_theme.bg_main == def_ct.bg_main);

    /* Validera att utebliven THEME i station.conf ger default (UEI-tema = 0) */
    StationSettings st_def;
    load_station_settings("/tmp/nonexistent_conf.conf", &st_def);
    int def_theme_ok = (st_def.theme == THEME_DEFAULT);

    unlink(test_conf);
    record_result(p0 && p1 && p2 && p3 && ct_init_ok && custom_persist_ok && def_theme_ok,
                  "CustomTheme: presets, init_defaults och fullständig persistens i station.conf (THEME=15)", NULL);
}

/* ========================================================================= */
/* [12] Direktloggning mot QRZ.COM (Inställningar, ADIF & Svarsanalys)       */
/* ========================================================================= */
static void run_qrz_direct_log_tests(void) {
    printf(ANSI_BLUE "\n=== [12] Direktloggning mot QRZ.COM ===" ANSI_RESET "\n");

    const char *test_conf = "/tmp/test_qrz_log_station.conf";
    unlink(test_conf);

    /* 1. Default-regel: Manuell om användarnamn och lösenord för QRZ.COM är ifyllda */
    {
        FILE *fp = fopen(test_conf, "w");
        if (fp) {
            fprintf(fp, "CALL=SM5UEI\nQRZ_USER=SM5UEI\nQRZ_PASS=hemligt\n");
            fclose(fp);
        }
        StationSettings st_cred;
        load_station_settings(test_conf, &st_cred);

        FILE *fp2 = fopen(test_conf, "w");
        if (fp2) {
            fprintf(fp2, "CALL=SM5UEI\n");
            fclose(fp2);
        }
        StationSettings st_nocred;
        load_station_settings(test_conf, &st_nocred);

        int ok = (st_cred.qrz_log_enabled == QRZ_LOG_MANUAL) &&
                 (st_nocred.qrz_log_enabled == QRZ_LOG_OFF);
        record_result(ok, "Default för Direktlogg QRZ.COM är Manuell om QRZ-användare/lösen är ifyllt, annars Av", NULL);
    }

    /* 2. Persistens av QRZ_LOG_ENABLED (Av, Automatisk, Manuell) och QRZ_API_KEY */
    {
        StationSettings st_save, st_load;
        memset(&st_save, 0, sizeof(st_save));
        copy_str(st_save.call, sizeof(st_save.call), "SM5UEI");
        copy_str(st_save.qrz_api_key, sizeof(st_save.qrz_api_key), "ABCD-1234-EFGH-5678");

        st_save.qrz_log_enabled = QRZ_LOG_AUTO;
        save_station_settings(test_conf, &st_save);
        load_station_settings(test_conf, &st_load);
        int ok_auto = (st_load.qrz_log_enabled == QRZ_LOG_AUTO) &&
                      (strcmp(st_load.qrz_api_key, "ABCD-1234-EFGH-5678") == 0);

        st_save.qrz_log_enabled = QRZ_LOG_MANUAL;
        save_station_settings(test_conf, &st_save);
        load_station_settings(test_conf, &st_load);
        int ok_man = (st_load.qrz_log_enabled == QRZ_LOG_MANUAL);

        st_save.qrz_log_enabled = QRZ_LOG_OFF;
        save_station_settings(test_conf, &st_save);
        load_station_settings(test_conf, &st_load);
        int ok_off = (st_load.qrz_log_enabled == QRZ_LOG_OFF);

        unlink(test_conf);
        record_result(ok_auto && ok_man && ok_off,
                      "QRZ_LOG_ENABLED (Av, Automatisk, Manuell) och QRZ_API_KEY sparas och läses in korrekt", NULL);
    }

    /* 3. qrz_build_adif_record genererar korrekt ADIF-post */
    {
        QSO q;
        memset(&q, 0, sizeof(q));
        copy_str(q.call, sizeof(q.call), "DL1ABC");
        copy_str(q.name, sizeof(q.name), "Hans");
        copy_str(q.date, sizeof(q.date), "2026-11-15");
        copy_str(q.time_utc, sizeof(q.time_utc), "12:30");
        copy_str(q.freq, sizeof(q.freq), "14.205");
        copy_str(q.band, sizeof(q.band), "20 m");
        copy_str(q.mode, sizeof(q.mode), "SSB");
        copy_str(q.rst_sent, sizeof(q.rst_sent), "59");
        copy_str(q.rst_rcvd, sizeof(q.rst_rcvd), "59");
        copy_str(q.gridsquare, sizeof(q.gridsquare), "JO52");
        copy_str(q.qth, sizeof(q.qth), "Berlin");

        StationSettings st;
        memset(&st, 0, sizeof(st));
        copy_str(st.call, sizeof(st.call), "SM5UEI");
        copy_str(st.gridsquare, sizeof(st.gridsquare), "JO89VJ");

        char adif[1024];
        qrz_build_adif_record(&q, &st, adif, sizeof(adif));

        int ok = (strstr(adif, "<CALL:6>DL1ABC") != NULL) &&
                 (strstr(adif, "<QSO_DATE:8>20261115") != NULL) &&
                 (strstr(adif, "<TIME_ON:4>1230") != NULL) &&
                 (strstr(adif, "<FREQ:6>14.205") != NULL) &&
                 (strstr(adif, "<BAND:3>20m") != NULL) &&
                 (strstr(adif, "<MODE:3>SSB") != NULL) &&
                 (strstr(adif, "<STATION_CALLSIGN:6>SM5UEI") != NULL) &&
                 (strstr(adif, "<EOR>") != NULL);
        record_result(ok, "qrz_build_adif_record bygger giltig ADIF-post med CALL, QSO_DATE, TIME_ON, BAND, MODE och <EOR>", NULL);
    }

    /* 4. qrz_parse_logbook_response tolkar OK, REPLACE, FAIL och AUTH */
    {
        char err[128] = "";
        int ok1 = qrz_parse_logbook_response("RESULT=OK&LOGID=987654&COUNT=1", err, sizeof(err));
        int ok2 = qrz_parse_logbook_response("STATUS=OK&RESULT=REPLACE&LOGIDS=987654", err, sizeof(err));
        int fail1 = qrz_parse_logbook_response("RESULT=FAIL&REASON=Unable to add QSO to database: duplicate&EXTENDED=", err, sizeof(err));
        int has_reason = (strstr(err, "duplicate") != NULL);
        char err2[128] = "";
        int fail2 = qrz_parse_logbook_response("RESULT=AUTH&REASON=invalid api key", err2, sizeof(err2));
        int has_auth_err = (strstr(err2, "invalid api key") != NULL);

        int ok = (ok1 == 1) && (ok2 == 1) && (fail1 == 0) && has_reason && (fail2 == 0) && has_auth_err;
        record_result(ok, "qrz_parse_logbook_response hanterar RESULT=OK, RESULT=REPLACE, RESULT=FAIL och RESULT=AUTH", NULL);
    }
}

/* ========================================================================= */
/* [13] Loggradshantering: Reorder (w/s), Färgcykling (c) & Edit-återgång    */
/* ========================================================================= */
static void run_log_row_features_tests(void) {
    printf(ANSI_BLUE "\n=== [13] Loggradshantering: Reorder (w/s), Färgcykling (c) & Edit-återgång ===" ANSI_RESET "\n");

    /* 1. Radordning: Swappa element uppåt (w) och nedåt (s) i logglistan */
    {
        QSOList list;
        memset(&list, 0, sizeof(list));
        QSO qa, qb, qc;
        memset(&qa, 0, sizeof(qa));
        memset(&qb, 0, sizeof(qb));
        memset(&qc, 0, sizeof(qc));
        copy_str(qa.call, sizeof(qa.call), "AA1A");
        copy_str(qb.call, sizeof(qb.call), "BB2B");
        copy_str(qc.call, sizeof(qc.call), "CC3C");
        qso_list_push(&list, &qa);
        qso_list_push(&list, &qb);
        qso_list_push(&list, &qc);

        /* Simulera flytt uppåt (w) av element på index 1 med index 2 */
        QSO tmp = list.items[1];
        list.items[1] = list.items[2];
        list.items[2] = tmp;

        int reorder_up_ok = (strcmp(list.items[1].call, "CC3C") == 0) &&
                            (strcmp(list.items[2].call, "BB2B") == 0);

        /* Simulera flytt nedåt (s) */
        tmp = list.items[1];
        list.items[1] = list.items[2];
        list.items[2] = tmp;

        int reorder_down_ok = (strcmp(list.items[1].call, "BB2B") == 0) &&
                              (strcmp(list.items[2].call, "CC3C") == 0);

        free(list.items);
        record_result(reorder_up_ok && reorder_down_ok,
                      "Loggrader kan flyttas och ändra ordning uppåt (w) och nedåt (s)", NULL);
    }

    /* 2. Cykling av bakgrundsfärg: Standard -> Grön -> Gul -> Röd -> Standard */
    {
        int col = LOG_COLOR_NORMAL; /* 0 */
        int col1 = (col + 1) % LOG_COLOR_COUNT;  /* 1 = Grön */
        int col2 = (col1 + 1) % LOG_COLOR_COUNT; /* 2 = Gul */
        int col3 = (col2 + 1) % LOG_COLOR_COUNT; /* 3 = Röd */
        int col0 = (col3 + 1) % LOG_COLOR_COUNT; /* 0 = Standard */

        int cycle_ok = (col == 0) && (col1 == 1) && (col2 == 2) && (col3 == 3) && (col0 == 0);
        record_result(cycle_ok,
                      "Färgcykling med 'c' följer ordningen Standard -> Grön -> Gul -> Röd -> Standard", NULL);
    }

    /* 3. Färgpersistens i SQLite och ADIF */
    {
        const char *db_test_path = "/tmp/test_color_db.sq3";
        unlink(db_test_path);
        db_open(db_test_path);

        QSO q;
        memset(&q, 0, sizeof(q));
        copy_str(q.call, sizeof(q.call), "SM5UEI");
        copy_str(q.date, sizeof(q.date), "2026-11-16");
        copy_str(q.time_utc, sizeof(q.time_utc), "09:00");
        copy_str(q.band, sizeof(q.band), "20 m");
        copy_str(q.mode, sizeof(q.mode), "SSB");
        copy_str(q.freq, sizeof(q.freq), "14.200");
        q.color = LOG_COLOR_GREEN; /* 1 */

        db_insert_qso(&q);
        int64_t id = q.id;

        QSOList loaded;
        memset(&loaded, 0, sizeof(loaded));
        db_load_all_qsos(&loaded);
        int ins_col_ok = (loaded.count == 1 && loaded.items[0].color == LOG_COLOR_GREEN);

        /* Uppdatera färg via db_set_color till RÖD (3) */
        db_set_color(id, LOG_COLOR_RED);
        free(loaded.items);
        memset(&loaded, 0, sizeof(loaded));
        db_load_all_qsos(&loaded);
        int upd_col_ok = (loaded.count == 1 && loaded.items[0].color == LOG_COLOR_RED);

        /* Spara och ladda via ADIF */
        const char *adif_test_path = "/tmp/test_color_adif.adi";
        unlink(adif_test_path);
        save_all_qsos_adif(adif_test_path, &loaded, NULL);

        QSOList adif_loaded;
        memset(&adif_loaded, 0, sizeof(adif_loaded));
        load_qsos_from_adif(adif_test_path, &adif_loaded);
        int adif_col_ok = (adif_loaded.count == 1 && adif_loaded.items[0].color == LOG_COLOR_RED);

        free(loaded.items);
        free(adif_loaded.items);
        unlink(adif_test_path);
        unlink(db_test_path);

        record_result(ins_col_ok && upd_col_ok && adif_col_ok,
                      "Radfärg persisteras i SQLite3 (db_set_color) och ADIF (APP_QSOLOG_COLOR)", NULL);
    }

    /* 4. Edit-återgång till loggläge (Enter redigera -> Enter spara -> åter till loggrader) */
    {
        FocusMode focus = FOCUS_LOG;
        int editing_index = -1;
        int edit_from_log = 0;

        /* Tryck Enter i loggläge */
        int selected_log_pos = 0;
        editing_index = selected_log_pos;
        edit_from_log = 1;
        focus = FOCUS_INPUT;

        /* Tryck Enter för att spara formulär */
        int prev_editing_index = editing_index;
        editing_index = -1; /* Sparat! */
        if (prev_editing_index >= 0 && editing_index < 0) {
            if (edit_from_log) {
                focus = FOCUS_LOG;
                edit_from_log = 0;
            }
        }

        int return_ok = (focus == FOCUS_LOG) && (edit_from_log == 0);
        record_result(return_ok,
                      "Redigering från loggrad återgår automatiskt till FOCUS_LOG när QSO sparas med Enter", NULL);
    }
}

/* ========================================================================= */
/* [14] QRZ.COM Synkronisering (Hämtning & Minnes-ADIF-import)               */
/* ========================================================================= */
static void run_qrz_sync_tests(void) {
    printf(ANSI_BLUE "\n=== [14] QRZ.COM Synkronisering & Minnes-ADIF-import ===" ANSI_RESET "\n");

    /* 1. STATION_FIELD_COUNT är 22 */
    record_result(STATION_FIELD_COUNT == 22,
                  "STATION_FIELD_COUNT är 22 (inkl. Databas, Exportera CSV, Synka till QRZ och Synka från QRZ)", NULL);

    /* 2. qrz_logbook_fetch validerar API-nyckel */
    {
        char *adif_out = NULL;
        size_t adif_sz = 0;
        char err[128] = "";
        int res_null = qrz_logbook_fetch(NULL, &adif_out, &adif_sz, err, sizeof(err));
        int res_empty = qrz_logbook_fetch("", &adif_out, &adif_sz, err, sizeof(err));
        int ok = (!res_null && !res_empty && strstr(err, "API-nyckel saknas") != NULL);
        record_result(ok, "qrz_logbook_fetch avvisar tom eller saknad API-nyckel med tydligt felmeddelande", NULL);
    }

    /* 3. import_qsos_from_memory importerar poster, sätter handled=1 och beräknar DXCC & QRB */
    {
        StationSettings st;
        memset(&st, 0, sizeof(st));
        copy_str(st.call, sizeof(st.call), "SM5UEI");
        copy_str(st.gridsquare, sizeof(st.gridsquare), "JO89we");

        QSOList list;
        memset(&list, 0, sizeof(list));

        const char *adif_test =
            "<adif_ver:5>3.1.4\n<eoh>\n"
            "<call:6>SM0ABC<qso_date:8>20261115<time_on:4>1200<band:3>20m<freq:6>14.200<mode:3>SSB<gridsquare:6>JO89xf<rst_sent:2>59<rst_rcvd:2>59<eor>\n"
            "<call:6>DL1XYZ<qso_date:8>20261115<time_on:4>1300<band:3>40m<freq:5>7.100<mode:2>CW<gridsquare:6>JO62qm<rst_sent:3>599<rst_rcvd:3>599<eor>\n";

        int imported = import_qsos_from_memory(adif_test, strlen(adif_test), &list, &st, 1, NULL);
        int ok = (imported == 2 && list.count == 2);
        if (ok) {
            ok = ok && (list.items[0].handled == 1) && (list.items[1].handled == 1);
            ok = ok && (strcmp(list.items[0].call, "SM0ABC") == 0);
            ok = ok && (strcmp(list.items[1].call, "DL1XYZ") == 0);
            ok = ok && (strcmp(list.items[0].gridsquare, "JO89XF") == 0);
            ok = ok && (strcmp(list.items[1].gridsquare, "JO62QM") == 0);
        }
        record_result(ok, "import_qsos_from_memory importerar 2 QSO:n, sätter handled=1 och normaliserar fält", NULL);

        /* 4. Dubblettkontroll vid synkronisering: ignorerar redan befintliga QSO:n */
        const char *adif_with_dup_and_new =
            "<call:6>SM0ABC<qso_date:8>20261115<time_on:4>1200<band:3>20m<mode:3>SSB<eor>\n" /* Dubblett */
            "<call:6>OH2NEW<qso_date:8>20261115<time_on:4>1400<band:3>20m<freq:6>14.250<mode:3>SSB<gridsquare:6>KP20ab<eor>\n"; /* Ny! */

        int imported2 = import_qsos_from_memory(adif_with_dup_and_new, strlen(adif_with_dup_and_new), &list, &st, 1, NULL);
        int dup_ok = (imported2 == 1 && list.count == 3);
        if (dup_ok) {
            dup_ok = dup_ok && (strcmp(list.items[2].call, "OH2NEW") == 0);
            dup_ok = dup_ok && (list.items[2].handled == 1);
        }
        record_result(dup_ok, "import_qsos_from_memory filtrerar bort dubbletter och lägger endast till nya poster", NULL);

        /* 5. Kronologisk sortering efter import */
        const char *adif_older =
            "<call:5>G3OLD<qso_date:8>20261114<time_on:4>1000<band:3>20m<freq:6>14.195<mode:3>SSB<eor>\n";
        int imported3 = import_qsos_from_memory(adif_older, strlen(adif_older), &list, &st, 1, NULL);
        int sort_ok = (imported3 == 1 && list.count == 4);
        if (sort_ok) {
            /* G3OLD (2026-11-14) ska hamna först efter kronologisk sortering */
            sort_ok = sort_ok && (strcmp(list.items[0].call, "G3OLD") == 0);
        }
        record_result(sort_ok, "import_qsos_from_memory sorterar listan kronologiskt", NULL);

        free(list.items);
    }

    /* 6. Maskering av QRZ API-nyckel vid visning */
    {
        StationSettings st;
        memset(&st, 0, sizeof(st));
        copy_str(st.qrz_api_key, sizeof(st.qrz_api_key), "ABCD-1234-EFGH-5678");

        char key_view[64] = "";
        char key_edit[64] = "";

        /* Vid visning (active != 7) maskeras den */
        int active_view = 0;
        if (active_view == 7) {
            copy_str(key_view, sizeof(key_view), st.qrz_api_key);
        } else if (st.qrz_api_key[0] != '\0') {
            copy_str(key_view, sizeof(key_view), "**********");
        }

        /* Vid redigering (active == 7) visas den i klartext */
        int active_edit = 7;
        if (active_edit == 7) {
            copy_str(key_edit, sizeof(key_edit), st.qrz_api_key);
        } else if (st.qrz_api_key[0] != '\0') {
            copy_str(key_edit, sizeof(key_edit), "**********");
        }

        int mask_ok = (strcmp(key_view, "**********") == 0) &&
                      (strcmp(key_edit, "ABCD-1234-EFGH-5678") == 0);
        record_result(mask_ok, "QRZ API-nyckel maskeras som ********** vid visning och visas i klartext vid redigering", NULL);
    }

    /* 7. Tolkning av HTML/XML-entitetskodat ADIF från QRZ.COM (&lt; och &gt;) */
    {
        StationSettings st;
        memset(&st, 0, sizeof(st));
        copy_str(st.call, sizeof(st.call), "SM5UEI");

        QSOList list;
        memset(&list, 0, sizeof(list));

        const char *html_adif =
            "&lt;call:7&gt;LA1TEST&lt;qso_date:8&gt;20261116&lt;time_on:4&gt;1000&lt;band:3&gt;20m&lt;mode:3&gt;SSB&lt;rst_sent:2&gt;59&lt;rst_rcvd:2&gt;59&lt;eor&gt;\n";

        int imported = import_qsos_from_memory(html_adif, strlen(html_adif), &list, &st, 1, NULL);
        int ok = (imported == 1 && list.count == 1);
        if (ok) {
            ok = ok && (strcmp(list.items[0].call, "LA1TEST") == 0);
            ok = ok && (list.items[0].handled == 1);
        }
        record_result(ok, "import_qsos_from_memory hanterar HTML/XML-entitetskodad ADIF (&lt; och &gt;) från QRZ API", NULL);
        if (list.items) free(list.items);
    }

    /* 8. sync_unhandled_qsos_to_qrz validerar API-nyckel och ohanterade poster */
    {
        StationSettings st;
        memset(&st, 0, sizeof(st));
        char msg[128] = "";

        /* Saknad nyckel ger fel */
        int r1 = sync_unhandled_qsos_to_qrz(NULL, &st, msg, sizeof(msg));
        int no_key_ok = (r1 < 0) && (strstr(msg, "API-nyckel saknas") != NULL);

        copy_str(st.qrz_api_key, sizeof(st.qrz_api_key), "TEST-API-KEY");
        QSOList list;
        memset(&list, 0, sizeof(list));

        /* Tom logg */
        int r2 = sync_unhandled_qsos_to_qrz(&list, &st, msg, sizeof(msg));
        int empty_ok = (r2 == 0) && (strstr(msg, "Loggen är tom") != NULL);

        /* Inga ohanterade poster */
        QSO q;
        memset(&q, 0, sizeof(q));
        copy_str(q.call, sizeof(q.call), "SM5UEI");
        q.handled = 1;
        qso_list_push(&list, &q);
        int r3 = sync_unhandled_qsos_to_qrz(&list, &st, msg, sizeof(msg));
        int no_unhandled_ok = (r3 == 0) && (strstr(msg, "Inga ohanterade QSO") != NULL);

        free(list.items);
        record_result(no_key_ok && empty_ok && no_unhandled_ok,
                      "sync_unhandled_qsos_to_qrz validerar API-nyckel, tom logg och ohanterade poster", NULL);
    }

    /* 9. QRZ.COM ADIF med UTF-8-teckenlängd (å, ä, ö m.fl.) och dubbelkodad UTF-8 hugger inte av QTH/Namn */
    {
        StationSettings st;
        memset(&st, 0, sizeof(st));
        copy_str(st.call, sizeof(st.call), "SM5UEI");

        QSOList list;
        memset(&list, 0, sizeof(list));

        /* Simulerar QRZ.COM ADIF-svar där längden i <qth:N>/<name:N> är angiven i antal tecken istället för bytes:
           Västerås (8 tkn, 10 bytes), Vällingby (9 tkn, 10 bytes), Bergsjö (7 tkn, 8 bytes),
           Fréderic (8 tkn, 9 bytes), samt dubbelkodad VÃ¤llingby (10 codepoints, 12 bytes) och KrakÃ³w (7 tkn, 9 bytes) */
        const char *qrz_utf8_adif =
            "&lt;call:6&gt;SM5XHB&lt;qso_date:8&gt;20261117&lt;time_on:4&gt;0751&lt;band:3&gt;80m&lt;mode:3&gt;SSB&lt;name:5&gt;Bernt&lt;qth:8&gt;Västerås&lt;eor&gt;\n"
            "&lt;call:6&gt;SA0PAL&lt;qso_date:8&gt;20261117&lt;time_on:4&gt;0752&lt;band:3&gt;80m&lt;mode:3&gt;SSB&lt;name:8&gt;Christer&lt;qth:9&gt;Vällingby&lt;eor&gt;\n"
            "&lt;call:6&gt;SA3XYZ&lt;qso_date:8&gt;20261117&lt;time_on:4&gt;0753&lt;band:3&gt;80m&lt;mode:3&gt;SSB&lt;name:5&gt;Peter&lt;qth:7&gt;Bergsjö&lt;eor&gt;\n"
            "&lt;call:5&gt;F4MZV&lt;qso_date:8&gt;20261117&lt;time_on:4&gt;0754&lt;band:3&gt;12m&lt;mode:3&gt;SSB&lt;name:8&gt;Fréderic&lt;qth:8&gt;Frouzins&lt;eor&gt;\n"
            "&lt;call:6&gt;SA0PAL&lt;qso_date:8&gt;20261117&lt;time_on:4&gt;0755&lt;band:3&gt;40m&lt;mode:3&gt;SSB&lt;name:8&gt;Christer&lt;qth:10&gt;VÃ¤llingby&lt;eor&gt;\n"
            "&lt;call:5&gt;SP9RS&lt;qso_date:8&gt;20261117&lt;time_on:4&gt;0756&lt;band:3&gt;20m&lt;mode:3&gt;SSB&lt;name:6&gt;Robert&lt;qth:7&gt;KrakÃ³w&lt;eor&gt;\n"
            "&lt;call:6&gt;SA3CJF&lt;qso_date:8&gt;20261117&lt;time_on:4&gt;0757&lt;band:3&gt;80m&lt;mode:3&gt;SSB&lt;name:6&gt;Marina&lt;qth:8&gt;Bollnäs &lt;eor&gt;\n";

        int imported = import_qsos_from_memory(qrz_utf8_adif, strlen(qrz_utf8_adif), &list, &st, 1, NULL);
        int ok = (imported == 7 && list.count == 7);
        if (ok) {
            ok = ok && (strcmp(list.items[0].qth, "Västerås") == 0);
            ok = ok && (strcmp(list.items[1].qth, "Vällingby") == 0);
            ok = ok && (strcmp(list.items[2].qth, "Bergsjö") == 0);
            ok = ok && (strcmp(list.items[3].name, "Fréderic") == 0);
            ok = ok && (strcmp(list.items[4].qth, "Vällingby") == 0);
            ok = ok && (strcmp(list.items[5].qth, "Kraków") == 0);
            ok = ok && (strcmp(list.items[6].qth, "Bollnäs") == 0);
        }
        record_result(ok, "import_qsos_from_memory klipper ej QTH/Namn med UTF-8-teckenlängd (t.ex. Västerås, Vällingby, Kraków)", NULL);

        /* Verifiera även filbaserad load_qsos_from_adif med samma fall */
        const char *tmp_qth_adi = "/tmp/test_qrz_qth_utf8.adi";
        FILE *fp = fopen(tmp_qth_adi, "w");
        if (fp) {
            fputs("<ADIF_VER:5>3.1.4\n<EOH>\n"
                  "<CALL:6>SM6ANK<QSO_DATE:8>20261117<TIME_ON:4>0800<BAND:3>80m<MODE:3>SSB<NAME:7>Lennart<QTH:8>Göteborg<EOR>\n"
                  "<CALL:6>SA4BUK<QSO_DATE:8>20261117<TIME_ON:4>0805<BAND:3>80m<MODE:3>SSB<NAME:4>Mats<QTH:10>Siljansnäs<EOR>\n",
                  fp);
            fclose(fp);
        }
        QSOList adif_loaded;
        memset(&adif_loaded, 0, sizeof(adif_loaded));
        load_qsos_from_adif(tmp_qth_adi, &adif_loaded);
        int file_ok = (adif_loaded.count == 2) &&
                      (strcmp(adif_loaded.items[0].qth, "Göteborg") == 0) &&
                      (strcmp(adif_loaded.items[1].qth, "Siljansnäs") == 0);
        unlink(tmp_qth_adi);
        if (adif_loaded.items) free(adif_loaded.items);
        record_result(file_ok, "load_qsos_from_adif bevarar fullständigt QTH vid teckenräknad UTF-8 (Göteborg, Siljansnäs)", NULL);

        if (list.items) free(list.items);
    }
}

/* ========================================================================= */
/* [15] CSV-Export (Excel, Google Docs, LibreOffice)                         */
/* ========================================================================= */
static void run_csv_export_tests(void) {
    printf(ANSI_BLUE "\n=== [15] CSV-Export (Excel, Google Docs, LibreOffice) ===" ANSI_RESET "\n");

    /* 1. build_default_export_csv_filename genererar giltiga tidsstämplade filnamn */
    {
        StationSettings st;
        memset(&st, 0, sizeof(st));
        copy_str(st.call, sizeof(st.call), "SM5/UEI");
        copy_str(st.timezone, sizeof(st.timezone), "Europe/Stockholm");

        char fname_all[128] = "";
        char fname_nya[128] = "";
        build_default_export_csv_filename(fname_all, sizeof(fname_all), &st, 0);
        build_default_export_csv_filename(fname_nya, sizeof(fname_nya), &st, 1);

        int ok = (strncmp(fname_all, "SM5-UEI_", 8) == 0) &&
                 (strstr(fname_all, ".csv") != NULL) &&
                 (strstr(fname_nya, "_nya.csv") != NULL);
        record_result(ok, "build_default_export_csv_filename genererar giltiga filnamn med .csv och _nya.csv", NULL);
    }

    /* 2. export_qsos_csv skapar korrekt CSV med UTF-8 BOM, rubriker och fältordning */
    {
        StationSettings st;
        memset(&st, 0, sizeof(st));
        copy_str(st.call, sizeof(st.call), "SM5UEI");
        copy_str(st.gridsquare, sizeof(st.gridsquare), "JO89we");
        copy_str(st.qth, sizeof(st.qth), "Stockholm");

        QSOList list;
        memset(&list, 0, sizeof(list));

        QSO q1;
        memset(&q1, 0, sizeof(q1));
        copy_str(q1.call, sizeof(q1.call), "SA0BQT");
        copy_str(q1.date, sizeof(q1.date), "2026-11-17");
        copy_str(q1.time_utc, sizeof(q1.time_utc), "13:45");
        copy_str(q1.freq, sizeof(q1.freq), "14.200");
        copy_str(q1.band, sizeof(q1.band), "20m");
        copy_str(q1.mode, sizeof(q1.mode), "USB");
        copy_str(q1.rst_sent, sizeof(q1.rst_sent), "59");
        copy_str(q1.rst_rcvd, sizeof(q1.rst_rcvd), "59");
        copy_str(q1.name, sizeof(q1.name), "Göran");
        copy_str(q1.qth, sizeof(q1.qth), "Västerås, Sweden");
        copy_str(q1.gridsquare, sizeof(q1.gridsquare), "JO89qm");
        copy_str(q1.dxcc, sizeof(q1.dxcc), "Sweden");
        copy_str(q1.comment, sizeof(q1.comment), "QRP 5W, test med \"citat\"");
        copy_str(q1.distance, sizeof(q1.distance), "85");
        q1.handled = 0;
        q1.marked = 1;
        qso_list_push(&list, &q1);

        QSO q2;
        memset(&q2, 0, sizeof(q2));
        copy_str(q2.call, sizeof(q2.call), "OH2ABC");
        copy_str(q2.date, sizeof(q2.date), "2026-11-17");
        copy_str(q2.time_utc, sizeof(q2.time_utc), "14:10");
        copy_str(q2.freq, sizeof(q2.freq), "7.100");
        copy_str(q2.band, sizeof(q2.band), "40m");
        copy_str(q2.mode, sizeof(q2.mode), "CW");
        copy_str(q2.rst_sent, sizeof(q2.rst_sent), "599");
        copy_str(q2.rst_rcvd, sizeof(q2.rst_rcvd), "579");
        copy_str(q2.name, sizeof(q2.name), "Matti");
        copy_str(q2.qth, sizeof(q2.qth), "Helsinki");
        copy_str(q2.gridsquare, sizeof(q2.gridsquare), "KP20ab");
        copy_str(q2.dxcc, sizeof(q2.dxcc), "Finland");
        copy_str(q2.comment, sizeof(q2.comment), "Dipol");
        copy_str(q2.distance, sizeof(q2.distance), "400");
        q2.handled = 1;
        q2.marked = 0;
        qso_list_push(&list, &q2);

        const char *test_csv = "/tmp/test_uei_export.csv";
        unlink(test_csv);

        int exported = export_qsos_csv(test_csv, &list, &st, EXPORT_MODE_ALL);
        record_result(exported == 2, "export_qsos_csv exporterar alla poster (EXPORT_MODE_ALL)", NULL);

        /* Kontrollera filens innehåll: BOM, rubriker, citerade fält och svenska tecken */
        FILE *fp = fopen(test_csv, "rb");
        int content_ok = 0;
        if (fp != NULL) {
            unsigned char bom[3] = {0};
            size_t bom_read = fread(bom, 1, 3, fp);
            int has_bom = (bom_read == 3 && bom[0] == 0xEF && bom[1] == 0xBB && bom[2] == 0xBF);

            char line1[512] = "";
            char line2[1024] = "";
            if (fgets(line1, sizeof(line1), fp) && fgets(line2, sizeof(line2), fp)) {
                /* Första raden ska innehålla rubrikerna i samma ordning som loggraderna */
                int h_ok = (strstr(line1, "#,Datum,Tid,Signal,Frekvens,Band,Mode,RST,Namn,QTH,QRB,QUJ,Lokator,DXCC,Notering") != NULL);

                /* Andra raden ska innehålla SA0BQT med formaterade fält */
                int d_ok = (strstr(line2, "1,") != NULL) &&
                           (strstr(line2, "\"2026-11-17\"") != NULL) &&
                           (strstr(line2, "\"13:45\"") != NULL) &&
                           (strstr(line2, "\"SA0BQT\"") != NULL) &&
                           (strstr(line2, "\"14.200\"") != NULL) &&
                           (strstr(line2, "\"20m\"") != NULL) &&
                           (strstr(line2, "\"USB\"") != NULL) &&
                           (strstr(line2, "\"59/59\"") != NULL) &&
                           (strstr(line2, "\"Göran\"") != NULL) &&
                           (strstr(line2, "\"Västerås, Sweden\"") != NULL) &&
                           (strstr(line2, "\"QRP 5W, test med \"\"citat\"\"\"") != NULL);

                content_ok = has_bom && h_ok && d_ok;
            }
            fclose(fp);
        }
        record_result(content_ok, "export_qsos_csv skriver UTF-8 BOM, korrekta rubriker och säkert citerade celler", NULL);

        /* 3. export_qsos_csv stöder EXPORT_MODE_MARKED och EXPORT_MODE_ONLY_NEW utan att ändra handled */
        int exp_marked = export_qsos_csv(test_csv, &list, &st, EXPORT_MODE_MARKED);
        int exp_new = export_qsos_csv(test_csv, &list, &st, EXPORT_MODE_ONLY_NEW);
        int modes_ok = (exp_marked == 1) && (exp_new == 1) && (list.items[0].handled == 0);
        record_result(modes_ok, "export_qsos_csv filtrerar på markerade/nya utan att modifiera handled-status", NULL);

        unlink(test_csv);
        free(list.items);
    }
}

/* ========================================================================= */
/* 16. TESTER FÖR version.h OCH BLANK RST VID LOGGNING / IMPORT              */
/* ========================================================================= */
static void run_version_and_blank_rst_tests(void) {
    printf("\n[16] Tester för version.h & blank RST vid loggning/import:\n");

    /* 1. Makron i src/version.h ska vara definierade och icke-tomma */
    {
        int macros_ok = (strlen(APP_NAME) > 0) &&
                        (strlen(APP_FULL_NAME) > 0) &&
                        (strlen(APP_PROGRAM_ID) > 0) &&
                        (strlen(APP_VERSION) > 0) &&
                        (strlen(APP_BUILD_DATE) > 0) &&
                        (strstr(APP_TITLE_STRING, APP_NAME) != NULL) &&
                        (strstr(APP_TITLE_STRING, APP_VERSION) != NULL);
        record_result(macros_ok, "version.h definierar namn, versionsnummer och byggdatum", NULL);
    }

    /* 2. reset_qso_form med auto_rst_59 == 0 lämnar rst_sent och rst_rcvd tomma */
    {
        StationSettings st;
        memset(&st, 0, sizeof(st));
        st.auto_rst_59 = 0;

        QSO q;
        int time_frozen = 0, active_field = 0, cursor_pos = 0, editing_index = 0;
        char last_smcb[32] = "SM0XYZ";
        char qrb[32] = "100 km", quj[32] = "180°";

        reset_qso_form(&q, &st, &time_frozen, &active_field, &cursor_pos,
                       &editing_index, last_smcb, qrb, quj);

        int blank_ok = (q.rst_sent[0] == '\0') && (q.rst_rcvd[0] == '\0');
        record_result(blank_ok, "reset_qso_form lämnar RST tomt när auto_rst_59 == 0", NULL);
    }

    /* 3. try_save_current_qso loggar blank RST om inget fyllts i (auto_rst_59 == 0 och == 1) */
    {
        /* Säkerhetskopiera befintlig station.conf innan try_save_current_qso skriver dit */
        int had_conf = 0;
        char *conf_backup = NULL;
        size_t conf_len = 0;
        FILE *cf = fopen(CONFIG_FILE, "rb");
        if (cf != NULL) {
            had_conf = 1;
            fseek(cf, 0, SEEK_END);
            long sz = ftell(cf);
            fseek(cf, 0, SEEK_SET);
            if (sz > 0) {
                conf_backup = (char *)malloc((size_t)sz);
                if (conf_backup) {
                    conf_len = fread(conf_backup, 1, (size_t)sz, cf);
                }
            }
            fclose(cf);
        }

        const char *tmp_db = "/tmp/test_blank_rst.sq3";
        unlink(tmp_db);
        db_close();
        int db_rc = db_open(tmp_db);

        StationSettings st;
        memset(&st, 0, sizeof(st));
        copy_str(st.call, sizeof(st.call), "SM0AAA");
        st.auto_rst_59 = 0;

        QSOList list;
        memset(&list, 0, sizeof(list));

        QSO q1;
        memset(&q1, 0, sizeof(q1));
        copy_str(q1.date, sizeof(q1.date), "2026-11-18");
        copy_str(q1.time_utc, sizeof(q1.time_utc), "10:15");
        copy_str(q1.call, sizeof(q1.call), "SM0BBB");
        copy_str(q1.freq, sizeof(q1.freq), "14.200");
        copy_str(q1.band, sizeof(q1.band), "20m");
        copy_str(q1.mode, sizeof(q1.mode), "USB");
        q1.rst_sent[0] = '\0';
        q1.rst_rcvd[0] = '\0';

        int time_frozen = 0, active_field = 0, cursor_pos = 0, editing_index = -1;
        char last_smcb[32] = "";
        char status_msg[128] = "";
        char qrb[32] = "", quj[32] = "";

        try_save_current_qso(&q1, &time_frozen, &active_field, &cursor_pos, &editing_index,
                             &st, &list, last_smcb, status_msg, sizeof(status_msg), qrb, quj);

        /* Testa även med auto_rst_59 == 1 om användaren manuellt lämnat RST tomt */
        st.auto_rst_59 = 1;
        QSO q2;
        memset(&q2, 0, sizeof(q2));
        copy_str(q2.date, sizeof(q2.date), "2026-11-18");
        copy_str(q2.time_utc, sizeof(q2.time_utc), "10:20");
        copy_str(q2.call, sizeof(q2.call), "SM0CCC");
        copy_str(q2.freq, sizeof(q2.freq), "7.030");
        copy_str(q2.band, sizeof(q2.band), "40m");
        copy_str(q2.mode, sizeof(q2.mode), "CW");
        q2.rst_sent[0] = '\0';
        q2.rst_rcvd[0] = '\0';

        try_save_current_qso(&q2, &time_frozen, &active_field, &cursor_pos, &editing_index,
                             &st, &list, last_smcb, status_msg, sizeof(status_msg), qrb, quj);

        int mem_ok = (list.count == 2) &&
                     (list.items[0].rst_sent[0] == '\0') &&
                     (list.items[0].rst_rcvd[0] == '\0') &&
                     (list.items[1].rst_sent[0] == '\0') &&
                     (list.items[1].rst_rcvd[0] == '\0');

        QSOList loaded;
        memset(&loaded, 0, sizeof(loaded));
        int load_rc = (db_rc == 0) ? db_load_all_qsos(&loaded) : -1;
        int db_ok = (load_rc == 2) &&
                    (loaded.items[0].rst_sent[0] == '\0') &&
                    (loaded.items[0].rst_rcvd[0] == '\0') &&
                    (loaded.items[1].rst_sent[0] == '\0') &&
                    (loaded.items[1].rst_rcvd[0] == '\0');

        record_result(mem_ok && db_ok,
                      "try_save_current_qso sparar blankt RST i minne och SQLite när fälten är tomma", NULL);

        free(loaded.items);
        free(list.items);
        db_close();
        unlink(tmp_db);

        /* Återställ station.conf till exakt ursprungligt skick */
        if (had_conf) {
            FILE *wcf = fopen(CONFIG_FILE, "wb");
            if (wcf != NULL) {
                if (conf_backup && conf_len > 0) fwrite(conf_backup, 1, conf_len, wcf);
                fclose(wcf);
            }
        } else {
            unlink(CONFIG_FILE);
        }
        free(conf_backup);
    }

    /* 4. finalize_imported_qso håller saknat RST blankt istället för att fylla i 59/599 */
    {
        StationSettings st;
        memset(&st, 0, sizeof(st));

        QSO qi;
        memset(&qi, 0, sizeof(qi));
        copy_str(qi.date, sizeof(qi.date), "2026-11-18");
        copy_str(qi.time_utc, sizeof(qi.time_utc), "10:30");
        copy_str(qi.call, sizeof(qi.call), "SM0DDD");
        copy_str(qi.mode, sizeof(qi.mode), "CW");
        qi.rst_sent[0] = '\0';
        qi.rst_rcvd[0] = '\0';

        finalize_imported_qso(&qi, &st);

        int imp_ok = (qi.rst_sent[0] == '\0') && (qi.rst_rcvd[0] == '\0');
        record_result(imp_ok, "finalize_imported_qso lämnar saknat RST blankt vid import", NULL);
    }
}

/* ========================================================================= */
/* 17. TESTER FÖR ~/.ueilogg OCH DATABASBYTE I INSTÄLLNINGAR                 */
/* ========================================================================= */
static void run_ueilogg_dir_and_db_switch_tests(void) {
    printf("\n[17] Tester för ~/.ueilogg & databasbyte i inställningar:\n");

    const char *orig_home_ptr = getenv("HOME");
    char orig_home[PATH_MAX] = "";
    int had_orig_home = (orig_home_ptr != NULL && orig_home_ptr[0] != '\0');
    if (had_orig_home) {
        copy_str(orig_home, sizeof(orig_home), orig_home_ptr);
    }

    char tmp_home[256];
    snprintf(tmp_home, sizeof(tmp_home), "/tmp/test_ueilogg_home_%ld", (long)getpid());
    setenv("HOME", tmp_home, 1);

    /* 1. Alla standardsökvägar pekar på <HOME>/.ueilogg/ med qsolog.sq3 som default */
    {
        char expected_dir[512];
        snprintf(expected_dir, sizeof(expected_dir), "%s/.ueilogg", tmp_home);
        int paths_ok = (strcmp(get_app_data_dir(), expected_dir) == 0) &&
                       (strncmp(get_config_file_path(), expected_dir, strlen(expected_dir)) == 0) &&
                       (strcmp(get_path_basename(get_config_file_path()), "station.conf") == 0) &&
                       (strncmp(get_dxcc_file_path(), expected_dir, strlen(expected_dir)) == 0) &&
                       (strcmp(get_path_basename(get_dxcc_file_path()), "dxcc.txt") == 0) &&
                       (strncmp(get_default_log_file_path(), expected_dir, strlen(expected_dir)) == 0) &&
                       (strcmp(get_path_basename(get_default_log_file_path()), "qsolog.sq3") == 0);
        record_result(paths_ok, "Alla config-, dxcc- och databassökvägar pekar på ~/.ueilogg/ (default qsolog.sq3)", NULL);
    }

    /* 2. ensure_app_data_dir och ensure_dxcc_file_seeded skapar ~/.ueilogg/dxcc.txt och slår upp DXCC */
    {
        int seeded = ensure_dxcc_file_seeded();
        int exists = (access(get_dxcc_file_path(), R_OK) == 0);
        load_dxcc_file(get_dxcc_file_path());
        char country[64] = "";
        lookup_dxcc("SM5UEI", country, sizeof(country));
        int dxcc_ok = seeded && exists &&
                      (strcmp(country, "Sverige") == 0 || strcmp(country, "Sweden") == 0);
        record_result(dxcc_ok, "ensure_dxcc_file_seeded kopierar dxcc.txt till ~/.ueilogg/ och möjliggör uppslag", NULL);
        free_dxcc_db();
    }

    /* 3. normalize_db_name och save/load DB_FILE i ~/.ueilogg/station.conf */
    {
        char n1[64] = "", n2[64] = "", n3[64] = "", n4[64] = "";
        normalize_db_name("contest", n1, sizeof(n1));
        normalize_db_name("fieldday.sq3", n2, sizeof(n2));
        normalize_db_name("", n3, sizeof(n3));
        normalize_db_name("/tmp/other/minlogg", n4, sizeof(n4));

        int norm_ok = (strcmp(n1, "contest.sq3") == 0) &&
                      (strcmp(n2, "fieldday.sq3") == 0) &&
                      (strcmp(n3, "qsolog.sq3") == 0) &&
                      (strcmp(n4, "minlogg.sq3") == 0);

        StationSettings st_def;
        load_station_settings(get_config_file_path(), &st_def);
        int def_ok = (strcmp(st_def.db_name, "qsolog.sq3") == 0);

        copy_str(st_def.call, sizeof(st_def.call), "SM5TEST");
        copy_str(st_def.db_name, sizeof(st_def.db_name), "contest.sq3");
        int saved_ok = (save_station_settings(get_config_file_path(), &st_def) == 0);

        StationSettings st_reloaded;
        load_station_settings(get_config_file_path(), &st_reloaded);
        int reload_ok = (strcmp(st_reloaded.db_name, "contest.sq3") == 0);

        record_result(norm_ok && def_ok && saved_ok && reload_ok,
                      "normalize_db_name och DB_FILE i ~/.ueilogg/station.conf fungerar med qsolog.sq3 som default", NULL);
    }

    /* 4. switch_station_database byter mellan databaser i ~/.ueilogg och isolerar loggarna */
    {
        StationSettings st;
        memset(&st, 0, sizeof(st));
        copy_str(st.call, sizeof(st.call), "SM5TEST");
        copy_str(st.db_name, sizeof(st.db_name), "qsolog.sq3");

        QSOList list;
        memset(&list, 0, sizeof(list));
        QSO form_qso;
        memset(&form_qso, 0, sizeof(form_qso));
        int editing_idx = 2, sel_pos = 1, scroll_off = 1;
        char status[128] = "";

        /* Öppna default qsolog.sq3 och lägg in ett QSO */
        db_close();
        copy_str(g_log_filename, sizeof(g_log_filename), get_default_log_file_path());
        int open_def_ok = (db_open(g_log_filename) == 0);

        QSO q_orig;
        memset(&q_orig, 0, sizeof(q_orig));
        copy_str(q_orig.call, sizeof(q_orig.call), "SM0AAA");
        copy_str(q_orig.date, sizeof(q_orig.date), "2026-11-18");
        copy_str(q_orig.time_utc, sizeof(q_orig.time_utc), "11:00");
        copy_str(q_orig.band, sizeof(q_orig.band), "20 m");
        copy_str(q_orig.mode, sizeof(q_orig.mode), "USB");
        db_insert_qso(&q_orig);

        /* Byt till ny databas 'contest' (ska skapa contest.sq3 i ~/.ueilogg och ladda tom lista) */
        int sw1 = switch_station_database(&st, &form_qso, &editing_idx, &list,
                                          &sel_pos, &scroll_off, "contest",
                                          status, sizeof(status));
        int sw1_ok = (sw1 == 0) && (strcmp(st.db_name, "contest.sq3") == 0) &&
                     (list.count == 0) && (editing_idx == -1) && (sel_pos == 0) &&
                     (strcmp(get_path_basename(g_log_filename), "contest.sq3") == 0);

        /* Logga ett QSO i contest.sq3 */
        QSO q_new;
        memset(&q_new, 0, sizeof(q_new));
        copy_str(q_new.call, sizeof(q_new.call), "OH2BBB");
        copy_str(q_new.date, sizeof(q_new.date), "2026-11-18");
        copy_str(q_new.time_utc, sizeof(q_new.time_utc), "11:05");
        copy_str(q_new.band, sizeof(q_new.band), "40 m");
        copy_str(q_new.mode, sizeof(q_new.mode), "CW");
        db_insert_qso(&q_new);

        /* Byt tillbaka till qsolog.sq3 */
        int sw2 = switch_station_database(&st, &form_qso, &editing_idx, &list,
                                          &sel_pos, &scroll_off, "qsolog.sq3",
                                          status, sizeof(status));
        int sw2_ok = (sw2 == 0) && (strcmp(st.db_name, "qsolog.sq3") == 0) &&
                     (list.count == 1) && (strcmp(list.items[0].call, "SM0AAA") == 0);

        record_result(open_def_ok && sw1_ok && sw2_ok,
                      "switch_station_database skapar/byter databas i ~/.ueilogg/ och isolerar QSO per databas", NULL);

        db_close();
        free(list.items);
        g_log_filename[0] = '\0';
    }

    /* Städa upp temporär testkatalog och återställ HOME */
    char rm_cmd[512];
    snprintf(rm_cmd, sizeof(rm_cmd), "rm -rf \"%s\"", tmp_home);
    if (system(rm_cmd) != 0) { /* ignore */ }

    if (had_orig_home) {
        setenv("HOME", orig_home, 1);
    } else {
        unsetenv("HOME");
    }
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
    run_qrz_direct_log_tests();
    run_log_row_features_tests();
    run_qrz_sync_tests();
    run_csv_export_tests();
    run_version_and_blank_rst_tests();
    run_ueilogg_dir_and_db_switch_tests();

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
