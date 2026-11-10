/**
 * @file qrz.c
 * @brief Implementation av uppslagning mot QRZ.COM XML Data Service, sessionshantering, lokal cache och bakgrundstrådar.
 */

#include "qrz.h"
#include "qrb.h"
#include "util.h"
#include <ctype.h>
#include <curl/curl.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

#define QRZ_XML_URL "https://xmldata.qrz.com/xml/current/"
#define QRZ_AGENT   "qsolog1.0"

/* Intern cache för QRZ-uppslag */
typedef struct {
    char call[32];
    char name[64];
    char qth[64];
    char grid[16];
    int  found;
} QrzCacheEntry;

static QrzCacheEntry g_qrz_cache[128];
static size_t g_qrz_cache_count = 0;
static pthread_mutex_t g_qrz_mutex = PTHREAD_MUTEX_INITIALIZER;

/* Sparad aktiv sessionsnyckel */
static char g_qrz_session_key[64] = "";

/* Tillstånd för asynkront QRZ-uppslag i bakgrundstråd */
typedef struct {
    int  active;
    int  req_id;
    char req_call[32];
    char req_user[36];
    char req_pass[36];

    int  has_result;
    int  res_id;
    int  res_found;
    char res_call[32];
    char res_name[64];
    char res_qth[64];
    char res_grid[16];
} QrzAsyncState;

static QrzAsyncState g_qrz_async;
static volatile int g_qrz_abort = 0;

/**
 * @brief Avbryter eventuella pågående bakgrundstrådar och städar upp vid programavslut.
 */
void qrz_cleanup(void) {
    g_qrz_abort = 1;
}

/**
 * @brief Nollställer och invaliderar den sparade sessionsnyckeln.
 */
void qrz_invalidate_session(void) {
    pthread_mutex_lock(&g_qrz_mutex);
    g_qrz_session_key[0] = '\0';
    pthread_mutex_unlock(&g_qrz_mutex);
}

/**
 * @brief Konverterar en sträng från ALL-CAPS till prydlig versal/gemen form (Title Case).
 * Hanterar även vanliga svenska tecken (Å, Ä, Ö).
 * @param s Sträng som modifieras på plats.
 */
static void format_title_case(char *s) {
    if (!s || !*s) return;
    int is_first = 1;
    unsigned char *p = (unsigned char *)s;
    while (*p) {
        if (*p == ' ' || *p == '-' || *p == '/' || *p == '.' || *p == ',') {
            is_first = 1;
            p++;
            continue;
        }
        if (is_first) {
            if (*p >= 'a' && *p <= 'z') {
                *p = (unsigned char)toupper(*p);
                p++;
            } else if (*p == 0xC3) {
                if (*(p + 1) == 0xA5) *(p + 1) = 0x85;      /* å -> Å */
                else if (*(p + 1) == 0xA4) *(p + 1) = 0x84; /* ä -> Ä */
                else if (*(p + 1) == 0xB6) *(p + 1) = 0x96; /* ö -> Ö */
                else if (*(p + 1) == 0xA9) *(p + 1) = 0x89; /* é -> É */
                p += 2;
            } else {
                p++;
            }
            is_first = 0;
        } else {
            if (*p >= 'A' && *p <= 'Z') {
                *p = (unsigned char)tolower(*p);
                p++;
            } else if (*p == 0xC3) {
                if (*(p + 1) == 0x85) *(p + 1) = 0xA5;      /* Å -> å */
                else if (*(p + 1) == 0x84) *(p + 1) = 0xA4; /* Ä -> ä */
                else if (*(p + 1) == 0x96) *(p + 1) = 0xB6; /* Ö -> ö */
                else if (*(p + 1) == 0x89) *(p + 1) = 0xA9; /* É -> é */
                else if (*(p + 1) == 0x9C) *(p + 1) = 0xBC; /* Ü -> ü */
                p += 2;
            } else {
                p++;
            }
        }
    }
}

/**
 * @brief Avkodar vanliga XML-entiteter (&amp;, &quot;, &apos;, &lt;, &gt;) på plats.
 * @param str Strängen som modifieras.
 */
static void xml_unescape(char *str) {
    if (!str) return;
    char *p = str;
    char *w = str;
    while (*p) {
        if (*p == '&') {
            if (strncmp(p, "&amp;", 5) == 0) {
                *w++ = '&'; p += 5;
            } else if (strncmp(p, "&quot;", 6) == 0) {
                *w++ = '"'; p += 6;
            } else if (strncmp(p, "&apos;", 6) == 0) {
                *w++ = '\''; p += 6;
            } else if (strncmp(p, "&lt;", 4) == 0) {
                *w++ = '<'; p += 4;
            } else if (strncmp(p, "&gt;", 4) == 0) {
                *w++ = '>'; p += 4;
            } else {
                *w++ = *p++;
            }
        } else {
            *w++ = *p++;
        }
    }
    *w = '\0';
}

/**
 * @brief Extraherar innehållet mellan en XML-tagg, t.ex. <Key>...</Key> eller <fname>...</fname>.
 * @param xml Hela XML-texten.
 * @param tag Taggens namn (utan vinkelparenteser).
 * @param out Målbuffer.
 * @param out_sz Målbufferns storlek.
 * @return 1 om taggen hittades och extraherades, annars 0.
 */
static int extract_xml_tag(const char *xml, const char *tag, char *out, size_t out_sz) {
    if (!xml || !tag || !out || out_sz == 0) return 0;
    out[0] = '\0';

    char open_tag[64], close_tag[64];
    snprintf(open_tag, sizeof(open_tag), "<%s>", tag);
    snprintf(close_tag, sizeof(close_tag), "</%s>", tag);

    const char *start = strcasestr(xml, open_tag);
    if (!start) {
        char open_prefix[64];
        snprintf(open_prefix, sizeof(open_prefix), "<%s ", tag);
        start = strcasestr(xml, open_prefix);
        if (start) {
            start = strchr(start, '>');
            if (start) start++;
        }
    } else {
        start += strlen(open_tag);
    }
    if (!start) return 0;

    const char *end = strcasestr(start, close_tag);
    if (!end) return 0;

    size_t len = (size_t)(end - start);
    if (len >= out_sz) len = out_sz - 1;
    memcpy(out, start, len);
    out[len] = '\0';
    trim_str(out);
    xml_unescape(out);
    return 1;
}

/**
 * @brief Rensar ett namn från QRZ: extraherar första namnet och konverterar versaler vid behov.
 * @param in Rånamn från XML.
 * @param out Målbuffer.
 * @param out_sz Målbufferns storlek.
 */
static void clean_qrz_name(const char *in, char *out, size_t out_sz) {
    if (!in || !*in) {
        if (out && out_sz > 0) *out = '\0';
        return;
    }
    const char *p = in;
    while (*p == ' ' || *p == '\t') p++;

    /* Hämta första ordet (avsluta vid mellanslag, tab eller komma) */
    char tmp[64];
    size_t i = 0;
    while (*p != '\0' && *p != ' ' && *p != '\t' && *p != ',' && i < sizeof(tmp) - 1) {
        tmp[i++] = *p++;
    }
    tmp[i] = '\0';

    /* Ta bort eventuell avslutande punkt (t.ex. vid initialer) */
    while (i > 0 && tmp[i - 1] == '.') {
        tmp[--i] = '\0';
    }

    /* Om namnet enbart består av versaler, formatera med stor begynnelsebokstav */
    int has_lower = 0;
    for (const unsigned char *cp = (const unsigned char *)tmp; *cp; ++cp) {
        if (*cp >= 'a' && *cp <= 'z') { has_lower = 1; break; }
        if (*cp == 0xC3 && (*(cp + 1) == 0xA5 || *(cp + 1) == 0xA4 || *(cp + 1) == 0xB6 || *(cp + 1) == 0xA9)) {
            has_lower = 1; break;
        }
    }
    if (!has_lower) {
        format_title_case(tmp);
    }
    copy_str(out, out_sz, tmp);
}

/**
 * @brief Rensar QTH/ort från QRZ: tar bort postnummerprefix och landsprefix (t.ex. "SE-172 38 ").
 * @param in Råtext från <addr2>.
 * @param out Målbuffer för ren ort.
 * @param out_sz Målbufferns storlek.
 */
static void clean_qrz_qth(const char *in, char *out, size_t out_sz) {
    if (!in || !*in) {
        if (out && out_sz > 0) *out = '\0';
        return;
    }
    const char *p = in;
    while (*p == ' ' || *p == '\t') p++;

    /* Kontrollera eventuellt landsprefix (t.ex. "SE-", "D-", "FI-") följt av siffror */
    const char *q = p;
    int letters = 0;
    while (isalpha((unsigned char)*q)) {
        letters++;
        q++;
    }
    if (letters >= 1 && letters <= 3 && (*q == '-' || *q == ' ')) {
        const char *after_prefix = q + 1;
        while (*after_prefix == ' ') after_prefix++;
        if (isdigit((unsigned char)*after_prefix)) {
            p = after_prefix;
        }
    }

    /* Hoppa förbi postnummersiffror, bindestreck och mellanslag */
    while (*p != '\0' && (isdigit((unsigned char)*p) || *p == ' ' || *p == '-')) {
        p++;
    }

    char tmp[64];
    copy_str(tmp, sizeof(tmp), p);
    trim_str(tmp);

    int has_lower = 0;
    for (const unsigned char *cp = (const unsigned char *)tmp; *cp; ++cp) {
        if (*cp >= 'a' && *cp <= 'z') { has_lower = 1; break; }
        if (*cp == 0xC3 && (*(cp + 1) == 0xA5 || *(cp + 1) == 0xA4 || *(cp + 1) == 0xB6 || *(cp + 1) == 0xA9)) {
            has_lower = 1; break;
        }
    }
    if (!has_lower) {
        format_title_case(tmp);
    }
    copy_str(out, out_sz, tmp);
}

/**
 * @brief Slår upp i intern minnescache för QRZ.
 * @param call_in Signalen som efterfrågas.
 * @param name_out Buffer för namn.
 * @param name_sz Storlek på namnbuffert.
 * @param qth_out Buffer för QTH.
 * @param qth_sz Storlek på QTH-buffert.
 * @param grid_out Buffer för lokator.
 * @param grid_sz Storlek på lokatorbuffert.
 * @param was_cached Sätts till 1 om signalen fanns i cachen (oavsett träff eller ej).
 * @return 1 om träff, 0 om ingen träff eller ej cachad.
 */
int lookup_qrz_cache(const char *call_in,
                     char *name_out, size_t name_sz,
                     char *qth_out, size_t qth_sz,
                     char *grid_out, size_t grid_sz,
                     int *was_cached) {
    if (was_cached) *was_cached = 0;
    if (call_in == NULL || call_in[0] == '\0') return 0;

    pthread_mutex_lock(&g_qrz_mutex);
    for (size_t i = 0; i < g_qrz_cache_count; ++i) {
        if (strcasecmp(g_qrz_cache[i].call, call_in) == 0) {
            if (was_cached) *was_cached = 1;
            int found = g_qrz_cache[i].found;
            if (found) {
                if (name_out && name_sz > 0) copy_str(name_out, name_sz, g_qrz_cache[i].name);
                if (qth_out && qth_sz > 0)   copy_str(qth_out, qth_sz, g_qrz_cache[i].qth);
                if (grid_out && grid_sz > 0)  copy_str(grid_out, grid_sz, g_qrz_cache[i].grid);
            }
            pthread_mutex_unlock(&g_qrz_mutex);
            return found;
        }
    }
    pthread_mutex_unlock(&g_qrz_mutex);
    return 0;
}

/**
 * @brief Sparar resultat i QRZ-cachen.
 * @param call_in Signalen.
 * @param name Namn.
 * @param qth QTH.
 * @param grid Lokator.
 * @param found 1 vid träff, 0 vid ingen träff.
 */
void store_qrz_cache(const char *call_in, const char *name, const char *qth, const char *grid, int found) {
    if (call_in == NULL || call_in[0] == '\0') return;

    pthread_mutex_lock(&g_qrz_mutex);
    for (size_t i = 0; i < g_qrz_cache_count; ++i) {
        if (strcasecmp(g_qrz_cache[i].call, call_in) == 0) {
            g_qrz_cache[i].found = found;
            copy_str(g_qrz_cache[i].name, sizeof(g_qrz_cache[i].name), name ? name : "");
            copy_str(g_qrz_cache[i].qth,  sizeof(g_qrz_cache[i].qth),  qth ? qth : "");
            copy_str(g_qrz_cache[i].grid, sizeof(g_qrz_cache[i].grid), grid ? grid : "");
            pthread_mutex_unlock(&g_qrz_mutex);
            return;
        }
    }

    if (g_qrz_cache_count < sizeof(g_qrz_cache) / sizeof(g_qrz_cache[0])) {
        size_t idx = g_qrz_cache_count++;
        copy_str(g_qrz_cache[idx].call, sizeof(g_qrz_cache[idx].call), call_in);
        to_upper_str(g_qrz_cache[idx].call);
        g_qrz_cache[idx].found = found;
        copy_str(g_qrz_cache[idx].name, sizeof(g_qrz_cache[idx].name), name ? name : "");
        copy_str(g_qrz_cache[idx].qth,  sizeof(g_qrz_cache[idx].qth),  qth ? qth : "");
        copy_str(g_qrz_cache[idx].grid, sizeof(g_qrz_cache[idx].grid), grid ? grid : "");
    }
    pthread_mutex_unlock(&g_qrz_mutex);
}

/**
 * @brief Loggar in mot QRZ.COM och hämtar en ny sessionsnyckel.
 * @param username Användarnamn.
 * @param password Lösenord.
 * @param err_msg Buffer för felmeddelande.
 * @param err_sz Storlek på felmeddelandebuffert.
 * @return 1 vid lyckad inloggning, 0 vid fel.
 */
static int qrz_login_locked(const char *username, const char *password, char *err_msg, size_t err_sz) {
    if (username == NULL || username[0] == '\0' || password == NULL || password[0] == '\0') {
        if (err_msg && err_sz > 0) copy_str(err_msg, err_sz, "Användarnamn eller lösenord saknas.");
        return 0;
    }

    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    char *enc_user = curl_easy_escape(curl, username, 0);
    char *enc_pass = curl_easy_escape(curl, password, 0);

    char url[512];
    snprintf(url, sizeof(url), "%s?username=%s;password=%s;agent=%s",
             QRZ_XML_URL, enc_user ? enc_user : username, enc_pass ? enc_pass : password, QRZ_AGENT);

    if (enc_user) curl_free(enc_user);
    if (enc_pass) curl_free(enc_pass);

    CurlBuf resp = {malloc(1), 0};
    if (resp.data) resp.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&resp);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, QRZ_AGENT);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 6L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 4L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || resp.data == NULL) {
        if (resp.data) free(resp.data);
        if (err_msg && err_sz > 0) copy_str(err_msg, err_sz, "Nätverksfel vid inloggning till QRZ.");
        return 0;
    }

    char key[64] = "";
    char err[128] = "";
    extract_xml_tag(resp.data, "Key", key, sizeof(key));
    extract_xml_tag(resp.data, "Error", err, sizeof(err));
    free(resp.data);

    if (key[0] != '\0') {
        copy_str(g_qrz_session_key, sizeof(g_qrz_session_key), key);
        return 1;
    }

    if (err[0] != '\0' && err_msg && err_sz > 0) {
        copy_str(err_msg, err_sz, err);
    }
    return 0;
}

/**
 * @brief Gör en sökning mot QRZ.COM med en given sessionsnyckel.
 * @param session_key Aktiv sessionsnyckel.
 * @param call Signalen som efterfrågas.
 * @param name_out Buffer för namn.
 * @param name_sz Storlek på namnbuffert.
 * @param qth_out Buffer för QTH.
 * @param qth_sz Storlek på QTH-buffert.
 * @param grid_out Buffer för lokator.
 * @param grid_sz Storlek på lokatorbuffert.
 * @return 1 vid träff, 0 vid ingen träff/hittades ej, -2 vid sessionsfel (utgången sessionsnyckel).
 */
static int qrz_query_call(const char *session_key, const char *call,
                          char *name_out, size_t name_sz,
                          char *qth_out, size_t qth_sz,
                          char *grid_out, size_t grid_sz) {
    if (session_key == NULL || session_key[0] == '\0' || call == NULL || call[0] == '\0') {
        return 0;
    }

    CURL *curl = curl_easy_init();
    if (!curl) return 0;

    char *enc_call = curl_easy_escape(curl, call, 0);
    char url[512];
    snprintf(url, sizeof(url), "%s?s=%s;callsign=%s", QRZ_XML_URL, session_key, enc_call ? enc_call : call);
    if (enc_call) curl_free(enc_call);

    CurlBuf resp = {malloc(1), 0};
    if (resp.data) resp.data[0] = '\0';

    curl_easy_setopt(curl, CURLOPT_URL, url);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&resp);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, QRZ_AGENT);
    curl_easy_setopt(curl, CURLOPT_TIMEOUT, 6L);
    curl_easy_setopt(curl, CURLOPT_CONNECTTIMEOUT, 4L);
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

    CURLcode res = curl_easy_perform(curl);
    curl_easy_cleanup(curl);

    if (res != CURLE_OK || resp.data == NULL) {
        if (resp.data) free(resp.data);
        return 0;
    }

    char err[128] = "";
    extract_xml_tag(resp.data, "Error", err, sizeof(err));
    if (err[0] != '\0') {
        if (strcasestr(err, "Session Timeout") || strcasestr(err, "Invalid session key")) {
            free(resp.data);
            return -2; /* Sessionen har gått ut */
        }
        /* Annat fel, t.ex. "Not found" */
        free(resp.data);
        return 0;
    }

    char raw_fname[64] = "", raw_name[64] = "", raw_addr2[64] = "", raw_grid[16] = "";
    extract_xml_tag(resp.data, "fname", raw_fname, sizeof(raw_fname));
    extract_xml_tag(resp.data, "name", raw_name, sizeof(raw_name));
    extract_xml_tag(resp.data, "addr2", raw_addr2, sizeof(raw_addr2));
    extract_xml_tag(resp.data, "grid", raw_grid, sizeof(raw_grid));
    free(resp.data);

    /* Välj förnamn i första hand; om fname saknas ta name */
    char clean_name_buf[64] = "";
    if (raw_fname[0] != '\0') {
        clean_qrz_name(raw_fname, clean_name_buf, sizeof(clean_name_buf));
    } else if (raw_name[0] != '\0') {
        clean_qrz_name(raw_name, clean_name_buf, sizeof(clean_name_buf));
    }

    char clean_qth_buf[64] = "";
    if (raw_addr2[0] != '\0') {
        clean_qrz_qth(raw_addr2, clean_qth_buf, sizeof(clean_qth_buf));
    }

    char clean_grid_buf[16] = "";
    if (raw_grid[0] != '\0') {
        copy_str(clean_grid_buf, sizeof(clean_grid_buf), raw_grid);
        trim_str(clean_grid_buf);
        to_upper_str(clean_grid_buf);
        if (!is_valid_locator(clean_grid_buf)) {
            clean_grid_buf[0] = '\0';
        }
    }

    if (clean_name_buf[0] != '\0' || clean_qth_buf[0] != '\0' || clean_grid_buf[0] != '\0') {
        if (name_out && name_sz > 0) copy_str(name_out, name_sz, clean_name_buf);
        if (qth_out && qth_sz > 0)   copy_str(qth_out, qth_sz, clean_qth_buf);
        if (grid_out && grid_sz > 0)  copy_str(grid_out, grid_sz, clean_grid_buf);
        return 1;
    }

    return 0;
}

/**
 * @brief Slår upp anropssignal synkront via QRZ.COM XML Data Service.
 * Kontrollerar cachen, återanvänder aktiv session och loggar automatiskt in på nytt vid sessionsavbrott.
 * @param username QRZ.COM användarnamn.
 * @param password QRZ.COM lösenord.
 * @param call_in Signalen.
 * @param name_out Buffer för namn.
 * @param name_sz Storlek på namnbuffert.
 * @param qth_out Buffer för QTH.
 * @param qth_sz Storlek på QTH-buffert.
 * @param grid_out Buffer för lokator.
 * @param grid_sz Storlek på lokatorbuffert.
 * @return 1 vid träff, 0 vid ingen träff eller fel.
 */
int qrz_lookup(const char *username, const char *password, const char *call_in,
               char *name_out, size_t name_sz,
               char *qth_out, size_t qth_sz,
               char *grid_out, size_t grid_sz) {
    if (call_in == NULL || call_in[0] == '\0') return 0;

    char clean_call[32];
    copy_str(clean_call, sizeof(clean_call), call_in);
    trim_str(clean_call);
    to_upper_str(clean_call);

    /* 1. Sök i intern cache */
    int was_cached = 0;
    if (lookup_qrz_cache(clean_call, name_out, name_sz, qth_out, qth_sz, grid_out, grid_sz, &was_cached)) {
        return 1;
    }
    if (was_cached) return 0;

    if (username == NULL || username[0] == '\0' || password == NULL || password[0] == '\0') {
        return 0;
    }

    pthread_mutex_lock(&g_qrz_mutex);

    /* 2. Logga in om sessionsnyckel saknas */
    if (g_qrz_session_key[0] == '\0') {
        char err[128] = "";
        if (!qrz_login_locked(username, password, err, sizeof(err))) {
            pthread_mutex_unlock(&g_qrz_mutex);
            return 0;
        }
    }

    char active_key[64];
    copy_str(active_key, sizeof(active_key), g_qrz_session_key);
    pthread_mutex_unlock(&g_qrz_mutex);

    /* 3. Gör sökningen */
    char res_name[64] = "", res_qth[64] = "", res_grid[16] = "";
    int status = qrz_query_call(active_key, clean_call, res_name, sizeof(res_name),
                                res_qth, sizeof(res_qth), res_grid, sizeof(res_grid));

    /* Om sessionen hade gått ut (-2), logga in på nytt och försök en gång till */
    if (status == -2) {
        pthread_mutex_lock(&g_qrz_mutex);
        g_qrz_session_key[0] = '\0';
        char err[128] = "";
        int login_ok = qrz_login_locked(username, password, err, sizeof(err));
        if (login_ok) {
            copy_str(active_key, sizeof(active_key), g_qrz_session_key);
        }
        pthread_mutex_unlock(&g_qrz_mutex);

        if (login_ok) {
            status = qrz_query_call(active_key, clean_call, res_name, sizeof(res_name),
                                    res_qth, sizeof(res_qth), res_grid, sizeof(res_grid));
        } else {
            status = 0;
        }
    }

    int found = (status == 1);
    store_qrz_cache(clean_call, res_name, res_qth, res_grid, found);

    if (found) {
        if (name_out && name_sz > 0) copy_str(name_out, name_sz, res_name);
        if (qth_out && qth_sz > 0)   copy_str(qth_out, qth_sz, res_qth);
        if (grid_out && grid_sz > 0)  copy_str(grid_out, grid_sz, res_grid);
        return 1;
    }

    return 0;
}

/**
 * @brief Trådfunktion för bakgrundssökning mot QRZ.COM.
 */
static void *qrz_worker_thread(void *arg) {
    (void)arg;
    char user[36], pass[36], call[32];
    int req_id = 0;

    pthread_mutex_lock(&g_qrz_mutex);
    copy_str(user, sizeof(user), g_qrz_async.req_user);
    copy_str(pass, sizeof(pass), g_qrz_async.req_pass);
    copy_str(call, sizeof(call), g_qrz_async.req_call);
    req_id = g_qrz_async.req_id;
    pthread_mutex_unlock(&g_qrz_mutex);

    char name[64] = "", qth[64] = "", grid[16] = "";
    int hit = qrz_lookup(user, pass, call, name, sizeof(name), qth, sizeof(qth), grid, sizeof(grid));

    if (g_qrz_abort) return NULL;

    pthread_mutex_lock(&g_qrz_mutex);
    if (g_qrz_async.req_id == req_id) {
        g_qrz_async.res_id = req_id;
        g_qrz_async.res_found = hit;
        copy_str(g_qrz_async.res_call, sizeof(g_qrz_async.res_call), call);
        copy_str(g_qrz_async.res_name, sizeof(g_qrz_async.res_name), name);
        copy_str(g_qrz_async.res_qth,  sizeof(g_qrz_async.res_qth),  qth);
        copy_str(g_qrz_async.res_grid, sizeof(g_qrz_async.res_grid), grid);
        g_qrz_async.has_result = 1;
        g_qrz_async.active = 0;
    }
    pthread_mutex_unlock(&g_qrz_mutex);
    return NULL;
}

/**
 * @brief Startar en asynkron bakgrundssökning mot QRZ.COM.
 * @param username QRZ-användarnamn.
 * @param password QRZ-lösenord.
 * @param call_in Signalen som efterfrågas.
 */
void start_qrz_background_lookup(const char *username, const char *password, const char *call_in) {
    if (username == NULL || username[0] == '\0' || password == NULL || password[0] == '\0') return;
    if (call_in == NULL || call_in[0] == '\0') return;

    pthread_mutex_lock(&g_qrz_mutex);
    g_qrz_async.req_id++;
    copy_str(g_qrz_async.req_user, sizeof(g_qrz_async.req_user), username);
    copy_str(g_qrz_async.req_pass, sizeof(g_qrz_async.req_pass), password);
    copy_str(g_qrz_async.req_call, sizeof(g_qrz_async.req_call), call_in);
    to_upper_str(g_qrz_async.req_call);
    g_qrz_async.has_result = 0;
    g_qrz_async.active = 1;

    pthread_t tid;
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
    if (pthread_create(&tid, &attr, qrz_worker_thread, NULL) != 0) {
        g_qrz_async.active = 0;
    }
    pthread_attr_destroy(&attr);
    pthread_mutex_unlock(&g_qrz_mutex);
}

/**
 * @brief Kontrollerar om QRZ-bakgrundsuppslagningen blivit klar och uppdaterar formuläret vid träff.
 * @param qso Aktuellt QSO.
 * @param st Stationsinställningar.
 * @param active_field Aktuellt inmatningsfält.
 * @param cursor_pos Pekare till markörposition.
 * @param status_msg Buffer för statusmeddelande.
 * @param status_sz Storlek på statusbuffert.
 * @param current_qrb Buffer för QRB-sträng.
 * @param qrb_sz Storlek på QRB-buffert.
 */
void poll_qrz_background_result(QSO *qso, const StationSettings *st,
                                int active_field, int *cursor_pos,
                                char *status_msg, size_t status_sz,
                                char *current_qrb, size_t qrb_sz) {
    int has_result = 0;
    int res_found = 0;
    char res_call[32] = "", res_name[64] = "", res_qth[64] = "", res_grid[16] = "";

    pthread_mutex_lock(&g_qrz_mutex);
    if (g_qrz_async.has_result) {
        has_result = 1;
        res_found = g_qrz_async.res_found;
        copy_str(res_call, sizeof(res_call), g_qrz_async.res_call);
        copy_str(res_name, sizeof(res_name), g_qrz_async.res_name);
        copy_str(res_qth,  sizeof(res_qth),  g_qrz_async.res_qth);
        copy_str(res_grid, sizeof(res_grid), g_qrz_async.res_grid);
        g_qrz_async.has_result = 0;
    }
    pthread_mutex_unlock(&g_qrz_mutex);

    if (!has_result) return;

    /* Kontrollera att det aktuella QSO:t fortfarande har samma signal */
    if (qso->call[0] == '\0' || strcasecmp(qso->call, res_call) != 0) {
        return;
    }

    if (res_found) {
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
        }
        if (qso->gridsquare[0] == '\0' && res_grid[0] != '\0') {
            copy_str(qso->gridsquare, sizeof(qso->gridsquare), res_grid);
            if (active_field == FLD_GRID && cursor_pos) {
                *cursor_pos = utf8_display_width(qso->gridsquare);
            }
            updated = 1;
        }

        if (current_qrb && qrb_sz > 0) {
            compute_qrb_str(st, qso, current_qrb, qrb_sz);
        }

        if (updated || status_msg[0] == '\0' || strstr(status_msg, "QRZ")) {
            if (res_grid[0] != '\0') {
                snprintf(status_msg, status_sz, "QRZ: %s, %s (%s)", res_name, res_qth, res_grid);
            } else {
                snprintf(status_msg, status_sz, "QRZ: %s, %s", res_name, res_qth);
            }
        }
    } else {
        if (status_msg[0] == '\0' || strstr(status_msg, "QRZ") || strstr(status_msg, "SMCB")) {
            snprintf(status_msg, status_sz, "QRZ.COM: Ingen träff för %s", res_call);
        }
    }
}
