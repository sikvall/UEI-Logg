/**
 * @file callbook.c
 * @brief Implementation av asynkron SM Callbook-uppslagning (SSA), lokal cache och hantering av extrasignaler.
 */

#include "callbook.h"
#include "qrb.h"
#include "qrz.h"
#include "util.h"
#include <ctype.h>
#include <curl/curl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <time.h>

/* Cache för SM Callbook (anropssignaler -> förnamn, QTH) */
typedef struct {
    char call[32];
    char name[64];
    char qth[64];
    int found;
} SmcbCacheEntry;

static SmcbCacheEntry g_smcb_cache[128];
static size_t g_smcb_cache_count = 0;
static pthread_mutex_t g_smcb_mutex = PTHREAD_MUTEX_INITIALIZER;
static int g_smcb_blocked = 0;

int is_smcb_blocked(void) {
    pthread_mutex_lock(&g_smcb_mutex);
    int b = g_smcb_blocked;
    pthread_mutex_unlock(&g_smcb_mutex);
    return b;
}

/* Delat tillstånd för asynkront SM Callbook-uppslag i bakgrunden */
typedef struct {
    int active;             /* 1 om en bakgrundstråd körs */
    int req_id;             /* Inkrementellt ID för aktuell förfrågan */
    char req_call[32];      /* Signalen som efterfrågas */

    int has_result;         /* 1 om tråden är klar och resultatet inte hämtats */
    int res_id;             /* req_id för det färdiga resultatet */
    int res_found;
    char res_call[32];
    char res_name[64];
    char res_qth[64];
} SmcbAsyncState;

static SmcbAsyncState g_smcb_async;
static volatile int g_smcb_abort = 0;

/**
 * @brief Kontrollera om anropssignalen är en svensk anropssignal (SA-SM, 7S eller 8S)
 * och extrahera bassignalen utan t.ex. /P, /M eller landsprefix som LA/.
 * @param call_in Inkommande signal.
 * @param base_out Buffer för ren svensk signal (t.ex. "SM5UEI").
 * @param out_sz Storlek på målbuffer.
 * @return 1 om svensk anropssignal, annars 0.
 */
int extract_swedish_base_call(const char *call_in, char *base_out, size_t out_sz) {
    if (call_in == NULL || call_in[0] == '\0') return 0;

    char clean[32];
    copy_str(clean, sizeof(clean), call_in);
    to_upper_str(clean);

    /* Om portabel (t.ex. SM5UEI/P eller LA/SM5UEI), hitta den del som har distriktssiffra */
    char *slash = strchr(clean, '/');
    if (slash != NULL) {
        *slash = '\0';
        const char *part2 = slash + 1;
        int p1_digit = 0, p2_digit = 0;
        for (const char *p = clean; *p; ++p) if (isdigit((unsigned char)*p)) p1_digit = 1;
        for (const char *p = part2; *p; ++p) if (isdigit((unsigned char)*p)) p2_digit = 1;
        if (!p1_digit && p2_digit) {
            copy_str(clean, sizeof(clean), part2);
            char *s2 = strchr(clean, '/');
            if (s2) *s2 = '\0';
        }
    }

    size_t len = strlen(clean);
    if (len < 4 || len > 7) return 0;

    int is_se_prefix = 0;
    if (clean[0] == 'S' && clean[1] >= 'A' && clean[1] <= 'M') {
        is_se_prefix = 1;
    } else if ((clean[0] == '7' || clean[0] == '8') && clean[1] == 'S') {
        is_se_prefix = 1;
    }
    if (!is_se_prefix) return 0;

    if (!isdigit((unsigned char)clean[2])) return 0;
    size_t i = 3;
    while (i < len && isdigit((unsigned char)clean[i])) {
        i++;
    }
    if (i >= len) return 0; /* Måste avslutas med minst en bokstav */
    for (; i < len; ++i) {
        if (!isalpha((unsigned char)clean[i])) return 0;
    }

    if (base_out && out_sz > 0) {
        copy_str(base_out, out_sz, clean);
    }
    return 1;
}

/**
 * @brief Utför ett HTTP-uppslag mot SM Callbook på ssa.se för en given anropssignal.
 * - Om signalen hittas direkt i medlemsregistret (<span class="block">), fylls
 *   name_out (endast förnamn) och qth_out i och funktionen returnerar 1.
 * - Om signalen är en extrasignal (t.ex. SM0W -> SM0WKA), fylls alt_call_out i
 *   med huvudsignalen och funktionen returnerar 0 så att ett andra uppslag kan göras.
 * @param query_call Signalen som söks.
 * @param name_out Buffer för förnamn.
 * @param name_sz Storlek på namnbuffert.
 * @param qth_out Buffer för QTH/ort.
 * @param qth_sz Storlek på QTH-buffert.
 * @param alt_call_out Buffer för alternativ huvudsignal vid extrasignaler.
 * @param alt_call_sz Storlek på alt_call-buffert.
 * @return 1 om medlemsuppgifter hittades, annars 0.
 */
static int fetch_sm_callbook_once(const char *query_call,
                                  char *name_out, size_t name_sz,
                                  char *qth_out, size_t qth_sz,
                                  char *alt_call_out, size_t alt_call_sz) {
    if (alt_call_out && alt_call_sz > 0) alt_call_out[0] = '\0';

    char ms_str[32], token_b64[64];
    long long ms_now = (long long)time(NULL) * 1000LL;
    snprintf(ms_str, sizeof(ms_str), "%lld", ms_now);
    base64_encode_str(ms_str, token_b64, sizeof(token_b64));

    char url[256];
    snprintf(url, sizeof(url),
             "https://www.ssa.se/ssa/smcb/?call=%s&fnamn=&token=%s&enamn=&ort=",
             query_call, token_b64);

    CURL *curl = curl_easy_init();
    if (curl == NULL) return 0;

    CurlBuf buf = {NULL, 0};
    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_NOSIGNAL, 1L);
    curl_easy_setopt(curl, CURLOPT_REFERER, "https://www.ssa.se/ssa/smcb/");
    curl_easy_setopt(curl, CURLOPT_USERAGENT, "Mozilla/5.0 (X11; Linux x86_64) qsolog/1.0");
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 4L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 3L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &buf);

    CURLcode res = curl_easy_perform(curl);
    long http_code = 0;
    curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || buf.data == NULL) {
        free(buf.data);
        return 0;
    }

    /* Kontrollera om SSA:s webbläsarskydd (wp_js_guard / "Kontrollerar webbläsaren") blockerar anropet */
    if (http_code == 403 ||
        strstr(buf.data, "Kontrollerar webbläsaren") != NULL ||
        strstr(buf.data, "wp_js_guard") != NULL) {
        pthread_mutex_lock(&g_smcb_mutex);
        g_smcb_blocked = 1;
        pthread_mutex_unlock(&g_smcb_mutex);
        free(buf.data);
        return -1;
    }

    pthread_mutex_lock(&g_smcb_mutex);
    g_smcb_blocked = 0;
    pthread_mutex_unlock(&g_smcb_mutex);

    /* Tolka HTML-svaret från SM Callbook */
    int found = 0;

    /* Kontrollera om signalen är markerad som Silent Key (avliden) */
    if (utf8_contains_ci(buf.data, "Silent key") || utf8_contains_ci(buf.data, "Silent-key") ||
        utf8_contains_ci(buf.data, "Silentkey") || utf8_contains_ci(buf.data, "Avliden")) {
        if (name_out && name_sz > 0) copy_str(name_out, name_sz, "-SK-");
        if (qth_out && qth_sz > 0) qth_out[0] = '\0';
        free(buf.data);
        return 1;
    }

    char *block = strstr(buf.data, "<span class=\"block\">");
    if (block != NULL) {
        char *after_strong = strstr(block, "</strong><br>");
        if (after_strong != NULL) {
            char *end_span = strstr(after_strong, "</span>");
            char *name_start = after_strong + strlen("</strong><br>");
            char *name_end = strstr(name_start, "<br>");
            if (name_end != NULL && end_span != NULL && name_end < end_span) {
                size_t nlen = (size_t)(name_end - name_start);
                char extracted_name[64];
                if (nlen >= sizeof(extracted_name)) nlen = sizeof(extracted_name) - 1;
                memcpy(extracted_name, name_start, nlen);
                extracted_name[nlen] = '\0';
                trim_newline(extracted_name);

                /* Ta bara med förnamnet (första namnet), trunkera efternamn/mellannamn */
                char *np = extracted_name;
                while (*np == ' ' || *np == '\t') np++;
                char *space = np;
                while (*space != '\0' && *space != ' ' && *space != '\t') space++;
                *space = '\0';
                char first_name[64];
                copy_str(first_name, sizeof(first_name), np);

                /* Hitta sista <br> före end_span för postnummer + ort */
                char *last_br = name_end;
                char *scan = name_end + 4;
                while (scan < end_span) {
                    char *next_br = strstr(scan, "<br>");
                    if (next_br != NULL && next_br < end_span) {
                        last_br = next_br;
                        scan = next_br + 4;
                    } else {
                        break;
                    }
                }

                char extracted_qth[64] = "";
                if (last_br < end_span) {
                    const char *city_line = last_br + 4;
                    size_t clen = (size_t)(end_span - city_line);
                    char raw_city[64];
                    if (clen >= sizeof(raw_city)) clen = sizeof(raw_city) - 1;
                    memcpy(raw_city, city_line, clen);
                    raw_city[clen] = '\0';
                    trim_newline(raw_city);

                    /* Hoppa förbi svenskt postnummer (t.ex. "819 62 " eller "162 70 ") */
                    const char *cp = raw_city;
                    while (*cp != '\0' && (isdigit((unsigned char)*cp) || *cp == ' ' || *cp == '-')) {
                        cp++;
                    }
                    copy_str(extracted_qth, sizeof(extracted_qth), cp);
                }

                if (first_name[0] != '\0') {
                    if (name_out && name_sz > 0) copy_str(name_out, name_sz, first_name);
                    if (qth_out && qth_sz > 0 && extracted_qth[0] != '\0') {
                        copy_str(qth_out, qth_sz, extracted_qth);
                    }
                    found = 1;
                }
            }
        }
    }

    /* Om inget direkt medlemsblock hittades, kontrollera om svaret är en Extrasignal */
    if (!found && alt_call_out != NULL && alt_call_sz > 0) {
        char td_pattern[48];
        snprintf(td_pattern, sizeof(td_pattern), "<td>%s</td><td>", query_call);
        char *td_match = strstr(buf.data, td_pattern);
        if (td_match != NULL) {
            const char *cstart = td_match + strlen(td_pattern);
            size_t cj = 0;
            while (cstart[cj] != '\0' && cstart[cj] != '<' &&
                   !isspace((unsigned char)cstart[cj]) && cj < alt_call_sz - 1) {
                alt_call_out[cj] = (char)toupper((unsigned char)cstart[cj]);
                cj++;
            }
            alt_call_out[cj] = '\0';
        } else {
            const char *link_pat = "/ssa/smcb/?call=";
            char *lmatch = strstr(buf.data, link_pat);
            if (lmatch != NULL) {
                const char *cstart = lmatch + strlen(link_pat);
                size_t cj = 0;
                while (cstart[cj] != '\0' && cstart[cj] != '\'' && cstart[cj] != '"' &&
                       cstart[cj] != '&' && cstart[cj] != '<' &&
                       !isspace((unsigned char)cstart[cj]) && cj < alt_call_sz - 1) {
                    alt_call_out[cj] = (char)toupper((unsigned char)cstart[cj]);
                    cj++;
                }
                alt_call_out[cj] = '\0';
            }
        }
    }

    free(buf.data);
    return found;
}

/**
 * @brief Slå upp svensk anropssignal (SA-SM, 7S, 8S) via SM Callbook på ssa.se.
 * Om det första svaret är en extrasignal (t.ex. SM0W -> SM0WKA), görs automatiskt ett
 * andra uppslag på huvudsignalen för att få förnamn och QTH.
 * @param call_in Anropssignal.
 * @param name_out Buffer för namn.
 * @param name_sz Storlek på namnbuffer.
 * @param qth_out Buffer för QTH.
 * @param qth_sz Storlek på QTH-buffer.
 * @return 1 om träff, annars 0.
 */
int lookup_sm_callbook(const char *call_in,
                       char *name_out, size_t name_sz,
                       char *qth_out, size_t qth_sz) {
    char base_call[32];
    if (!extract_swedish_base_call(call_in, base_call, sizeof(base_call))) {
        return 0;
    }

    char alt_call[32] = "";
    int rc = fetch_sm_callbook_once(base_call, name_out, name_sz, qth_out, qth_sz,
                                    alt_call, sizeof(alt_call));
    if (rc == 1) {
        return 1;
    }
    if (rc == -1) {
        return -1;
    }

    if (alt_call[0] != '\0' && strcasecmp(alt_call, base_call) != 0) {
        int rc2 = fetch_sm_callbook_once(alt_call, name_out, name_sz, qth_out, qth_sz,
                                         NULL, 0);
        if (rc2 == 1) {
            return 1;
        }
        if (rc2 == -1) {
            return -1;
        }
    }

    return 0;
}

/**
 * @brief Sök i den lokala minnescachen efter ett tidigare SMCB-resultat.
 * @param call_in Signalen.
 * @param name_out Buffer för namn.
 * @param name_sz Storlek på namnbuffer.
 * @param qth_out Buffer för QTH.
 * @param qth_sz Storlek på QTH-buffer.
 * @param was_cached Sätts till 1 om signalen fanns i cachen.
 * @return 1 om signalen fanns och hade giltigt namn/QTH, annars 0.
 */
int lookup_smcb_cache(const char *call_in, char *name_out, size_t name_sz,
                      char *qth_out, size_t qth_sz, int *was_cached) {
    if (was_cached) *was_cached = 0;
    pthread_mutex_lock(&g_smcb_mutex);
    for (size_t i = 0; i < g_smcb_cache_count; ++i) {
        if (strcasecmp(g_smcb_cache[i].call, call_in) == 0) {
            if (was_cached) *was_cached = 1;
            int found = g_smcb_cache[i].found;
            if (found) {
                if (name_out && name_sz > 0) copy_str(name_out, name_sz, g_smcb_cache[i].name);
                if (qth_out && qth_sz > 0) copy_str(qth_out, qth_sz, g_smcb_cache[i].qth);
            }
            pthread_mutex_unlock(&g_smcb_mutex);
            return found;
        }
    }
    pthread_mutex_unlock(&g_smcb_mutex);
    return 0;
}

/**
 * @brief Sparar eller uppdaterar en post i SMCB-minnescachen.
 * @param call_in Signalen.
 * @param name Förnamn.
 * @param qth QTH/ort.
 * @param found 1 om giltig medlem, 0 om ingen träff.
 */
void store_smcb_cache(const char *call_in, const char *name, const char *qth, int found) {
    pthread_mutex_lock(&g_smcb_mutex);
    for (size_t i = 0; i < g_smcb_cache_count; ++i) {
        if (strcasecmp(g_smcb_cache[i].call, call_in) == 0) {
            g_smcb_cache[i].found = found;
            copy_str(g_smcb_cache[i].name, sizeof(g_smcb_cache[i].name), name ? name : "");
            copy_str(g_smcb_cache[i].qth, sizeof(g_smcb_cache[i].qth), qth ? qth : "");
            pthread_mutex_unlock(&g_smcb_mutex);
            return;
        }
    }
    if (g_smcb_cache_count < sizeof(g_smcb_cache) / sizeof(g_smcb_cache[0])) {
        copy_str(g_smcb_cache[g_smcb_cache_count].call, sizeof(g_smcb_cache[0].call), call_in);
        copy_str(g_smcb_cache[g_smcb_cache_count].name, sizeof(g_smcb_cache[0].name), name ? name : "");
        copy_str(g_smcb_cache[g_smcb_cache_count].qth, sizeof(g_smcb_cache[0].qth), qth ? qth : "");
        g_smcb_cache[g_smcb_cache_count].found = found;
        g_smcb_cache_count++;
    }
    pthread_mutex_unlock(&g_smcb_mutex);
}

typedef struct {
    char call[32];
    int req_id;
} SmcbThreadArg;

/**
 * @brief Trådfunktion för asynkron bakgrundsslagning mot SM Callbook.
 * @param arg Dynamiskt allokerad SmcbThreadArg-struktur.
 * @return NULL.
 */
static void *smcb_worker_thread(void *arg) {
    SmcbThreadArg *targ = (SmcbThreadArg *)arg;
    char query_call[32];
    copy_str(query_call, sizeof(query_call), targ->call);
    int req_id = targ->req_id;
    free(targ);

    char name[64] = "", qth[64] = "";
    int found = lookup_sm_callbook(query_call, name, sizeof(name), qth, sizeof(qth));

    if (g_smcb_abort) return NULL;

    store_smcb_cache(query_call, name, qth, found);

    pthread_mutex_lock(&g_smcb_mutex);
    if (!g_smcb_abort && req_id == g_smcb_async.req_id) {
        g_smcb_async.has_result = 1;
        g_smcb_async.res_id = req_id;
        g_smcb_async.res_found = found;
        copy_str(g_smcb_async.res_call, sizeof(g_smcb_async.res_call), query_call);
        copy_str(g_smcb_async.res_name, sizeof(g_smcb_async.res_name), name);
        copy_str(g_smcb_async.res_qth, sizeof(g_smcb_async.res_qth), qth);
    }
    g_smcb_async.active = 0;
    pthread_mutex_unlock(&g_smcb_mutex);

    return NULL;
}

/**
 * @brief Startar en asynkron bakgrundstråd för att slå upp signalen i SM Callbook utan att låsa TUI.
 * @param base_call Bassignalen.
 */
void start_smcb_background_lookup(const char *base_call) {
    if (base_call == NULL || base_call[0] == '\0') return;

    pthread_mutex_lock(&g_smcb_mutex);
    if (g_smcb_async.active && strcasecmp(g_smcb_async.req_call, base_call) == 0) {
        pthread_mutex_unlock(&g_smcb_mutex);
        return;
    }

    g_smcb_async.req_id++;
    copy_str(g_smcb_async.req_call, sizeof(g_smcb_async.req_call), base_call);
    g_smcb_async.active = 1;
    g_smcb_async.has_result = 0;

    SmcbThreadArg *targ = malloc(sizeof(SmcbThreadArg));
    if (!targ) {
        g_smcb_async.active = 0;
        pthread_mutex_unlock(&g_smcb_mutex);
        return;
    }
    copy_str(targ->call, sizeof(targ->call), base_call);
    targ->req_id = g_smcb_async.req_id;

    pthread_t tid;
    if (pthread_create(&tid, NULL, smcb_worker_thread, targ) == 0) {
        pthread_detach(tid);
    } else {
        free(targ);
        g_smcb_async.active = 0;
    }
    pthread_mutex_unlock(&g_smcb_mutex);
}

/**
 * @brief Kontrollerar om bakgrundssökningen i SM Callbook har blivit klar och applicerar resultatet på formuläret.
 * @param qso Aktuellt QSO.
 * @param st Stationsinställningar.
 * @param active_field Aktuellt aktivt inmatningsfält.
 * @param cursor_pos Pekare till markörposition i fältet.
 * @param status_msg Buffer för statusmeddelande.
 * @param status_sz Storlek på statusbuffert.
 * @param current_qrb Buffer för QRB-sträng.
 * @param qrb_sz Storlek på QRB-buffert.
 */
void poll_smcb_background_result(QSO *qso, const StationSettings *st,
                                 int active_field, int *cursor_pos,
                                 char *status_msg, size_t status_sz,
                                 char *current_qrb, size_t qrb_sz) {
    int has_result = 0;
    int res_found = 0;
    char res_call[32] = "", res_name[64] = "", res_qth[64] = "";

    pthread_mutex_lock(&g_smcb_mutex);
    if (g_smcb_async.has_result) {
        has_result = 1;
        res_found = g_smcb_async.res_found;
        copy_str(res_call, sizeof(res_call), g_smcb_async.res_call);
        copy_str(res_name, sizeof(res_name), g_smcb_async.res_name);
        copy_str(res_qth, sizeof(res_qth), g_smcb_async.res_qth);
        g_smcb_async.has_result = 0;
    }
    pthread_mutex_unlock(&g_smcb_mutex);

    if (!has_result) return;

    /* Kontrollera att det aktuella QSO:t fortfarande har samma signal */
    char base_call[32];
    if (qso->call[0] == '\0' || !extract_swedish_base_call(qso->call, base_call, sizeof(base_call))) {
        return;
    }
    if (strcasecmp(base_call, res_call) != 0) {
        return; /* Signalen har ändrats under tiden, ignorera resultatet */
    }

    if (res_found) {
        if (strcmp(res_name, "-SK-") == 0) {
            copy_str(qso->name, sizeof(qso->name), "-SK-");
            qso->qth[0] = '\0';
            copy_str(qso->comment, sizeof(qso->comment), "SILENT KEY");
            if (current_qrb && qrb_sz > 0) copy_str(current_qrb, qrb_sz, "-");
            if (active_field == FLD_NAME && cursor_pos) *cursor_pos = 0;
            snprintf(status_msg, status_sz, "SMCB: SILENT KEY (-SK-)");
            return;
        }

        int updated = 0;
        if (qso->name[0] == '\0' && res_name[0] != '\0') {
            copy_str(qso->name, sizeof(qso->name), res_name);
            if (active_field == FLD_NAME && cursor_pos) {
                *cursor_pos = utf8_display_width(qso->name);
            }
            updated = 1;
        }
        if (qso->qth[0] == '\0' && res_qth[0] != '\0') {
            copy_str(qso->qth, sizeof(qso->qth), res_qth);
            if (active_field == FLD_QTH && cursor_pos) {
                *cursor_pos = utf8_display_width(qso->qth);
            }
            updated = 1;
            if (current_qrb && qrb_sz > 0) {
                compute_qrb_str(st, qso, current_qrb, qrb_sz);
            }
        }
        if (updated || status_msg[0] == '\0' || strstr(status_msg, "SMCB: Slår upp")) {
            snprintf(status_msg, status_sz, "SMCB: %s, %s", res_name, res_qth);
        }
    } else if (res_found == -1) {
        if (st && st->qrz_enabled && st->qrz_username[0] != '\0') {
            start_qrz_background_lookup(st->qrz_username, st->qrz_password, qso->call);
            snprintf(status_msg, status_sz, "SMCB blockerat -> QRZ: Slår upp %s...", qso->call);
        } else if (status_msg[0] == '\0' || strstr(status_msg, "SMCB: Slår upp")) {
            snprintf(status_msg, status_sz, "SMCB: Blockerat av SSA webbläsarkontroll (använd QRZ.COM)");
        }
    } else {
        if (st && st->qrz_enabled && st->qrz_username[0] != '\0') {
            start_qrz_background_lookup(st->qrz_username, st->qrz_password, qso->call);
            snprintf(status_msg, status_sz, "SMCB: Ingen träff -> QRZ: Slår upp %s...", qso->call);
        } else if (status_msg[0] == '\0' || strstr(status_msg, "SMCB: Slår upp")) {
            snprintf(status_msg, status_sz, "SMCB: Ingen träff för %s", res_call);
        }
    }
}

/**
 * @brief Avbryter pågående bakgrundsuppslag och förbereder för säker programavslutning.
 */
void callbook_cleanup(void) {
    g_smcb_abort = 1;
}
