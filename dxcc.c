/**
 * @file dxcc.c
 * @brief Implementation av DXCC-uppslagning med prefix- och exakt matchning samt hantering av portabelsuffix.
 */

#include "dxcc.h"
#include "util.h"
#include <ctype.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>

/* Struktur för en DXCC-regel */
typedef struct {
    char key[32];
    char country[64];
} DxccRule;

/* Databas över inlästa DXCC-regler */
typedef struct {
    DxccRule *prefixes;
    size_t prefix_count;
    size_t prefix_cap;

    DxccRule *exacts;
    size_t exact_count;
    size_t exact_cap;
} DxccDatabase;

static DxccDatabase g_dxcc_db;

/**
 * @brief Jämförelsefunktion för qsort som sorterar prefix fallande efter längd (längst prefix matchas först).
 * @param a Pekare till första regeln.
 * @param b Pekare till andra regeln.
 * @return Negativt om a är längre än b, positivt om b är längre än a.
 */
static int cmp_dxcc_prefix_desc(const void *a, const void *b) {
    const DxccRule *pa = (const DxccRule *)a;
    const DxccRule *pb = (const DxccRule *)b;
    size_t la = strlen(pa->key);
    size_t lb = strlen(pb->key);
    if (la != lb) {
        return (lb > la) ? 1 : -1;
    }
    return strcmp(pa->key, pb->key);
}

/**
 * @brief Läser in DXCC-tabellen från dxcc.txt (semikolonseparerat format: PREFIX;LAND eller =SIGNAL;LAND).
 * @param filename Sökvägen till filen.
 */
void load_dxcc_file(const char *filename) {
    free_dxcc_db();

    FILE *fp = fopen(filename, "r");
    if (fp == NULL) return;

    char line[256];
    while (fgets(line, sizeof(line), fp) != NULL) {
        trim_newline(line);
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#') continue;

        char *sep = strchr(p, ';');
        if (sep == NULL) continue;
        *sep = '\0';
        const char *key = p;
        const char *country = sep + 1;

        if (key[0] == '=') {
            key++;
            if (key[0] == '\0') continue;
            if (g_dxcc_db.exact_count >= g_dxcc_db.exact_cap) {
                size_t ncap = (g_dxcc_db.exact_cap == 0) ? 256 : g_dxcc_db.exact_cap * 2;
                DxccRule *narr = realloc(g_dxcc_db.exacts, ncap * sizeof(DxccRule));
                if (!narr) break;
                g_dxcc_db.exacts = narr;
                g_dxcc_db.exact_cap = ncap;
            }
            copy_str(g_dxcc_db.exacts[g_dxcc_db.exact_count].key,
                     sizeof(g_dxcc_db.exacts[0].key), key);
            to_upper_str(g_dxcc_db.exacts[g_dxcc_db.exact_count].key);
            copy_str(g_dxcc_db.exacts[g_dxcc_db.exact_count].country,
                     sizeof(g_dxcc_db.exacts[0].country), country);
            g_dxcc_db.exact_count++;
        } else {
            if (key[0] == '\0') continue;
            if (g_dxcc_db.prefix_count >= g_dxcc_db.prefix_cap) {
                size_t ncap = (g_dxcc_db.prefix_cap == 0) ? 512 : g_dxcc_db.prefix_cap * 2;
                DxccRule *narr = realloc(g_dxcc_db.prefixes, ncap * sizeof(DxccRule));
                if (!narr) break;
                g_dxcc_db.prefixes = narr;
                g_dxcc_db.prefix_cap = ncap;
            }
            copy_str(g_dxcc_db.prefixes[g_dxcc_db.prefix_count].key,
                     sizeof(g_dxcc_db.prefixes[0].key), key);
            to_upper_str(g_dxcc_db.prefixes[g_dxcc_db.prefix_count].key);
            copy_str(g_dxcc_db.prefixes[g_dxcc_db.prefix_count].country,
                     sizeof(g_dxcc_db.prefixes[0].country), country);
            g_dxcc_db.prefix_count++;
        }
    }
    fclose(fp);

    if (g_dxcc_db.prefix_count > 1) {
        qsort(g_dxcc_db.prefixes, g_dxcc_db.prefix_count, sizeof(DxccRule), cmp_dxcc_prefix_desc);
    }
}

/**
 * @brief Frigör dynamiskt minne som allokerats för DXCC-databasen.
 */
void free_dxcc_db(void) {
    free(g_dxcc_db.prefixes);
    free(g_dxcc_db.exacts);
    memset(&g_dxcc_db, 0, sizeof(g_dxcc_db));
}

/**
 * @brief Kontrollerar om ett suffix efter ett snedstreck är ett vanligt modifieringssuffix (t.ex. /P, /M, /QRP, /8).
 * @param s Suffixsträng.
 * @return 1 om modifieringssuffix, annars 0.
 */
static int is_modifier_suffix(const char *s) {
    if (s == NULL || s[0] == '\0') return 1;
    if (strcasecmp(s, "P") == 0 || strcasecmp(s, "M") == 0 ||
        strcasecmp(s, "MM") == 0 || strcasecmp(s, "AM") == 0 ||
        strcasecmp(s, "QRP") == 0 || strcasecmp(s, "A") == 0 ||
        strcasecmp(s, "LH") == 0 || strcasecmp(s, "J") == 0) {
        return 1;
    }
    if (isdigit((unsigned char)s[0]) && s[1] == '\0') {
        return 1;
    }
    return 0;
}

/**
 * @brief Söker efter en exakt signalmatchning i tabellen med DXCC-undantag.
 * @param call Anropssignal i versaler.
 * @return Landets namn om matchning finns, annars NULL.
 */
static const char *match_exact_dxcc(const char *call) {
    for (size_t i = 0; i < g_dxcc_db.exact_count; ++i) {
        if (strcasecmp(g_dxcc_db.exacts[i].key, call) == 0) {
            return g_dxcc_db.exacts[i].country;
        }
    }
    return NULL;
}

/**
 * @brief Matchar en anropssignal mot DXCC-prefixlistan (längst prefix matchas först).
 * @param call Anropssignal i versaler.
 * @return Landets namn om matchning finns, annars NULL.
 */
static const char *match_prefix_dxcc(const char *call) {
    size_t clen = strlen(call);
    for (size_t i = 0; i < g_dxcc_db.prefix_count; ++i) {
        const char *pfx = g_dxcc_db.prefixes[i].key;
        size_t plen = strlen(pfx);
        if (plen <= clen && strncmp(call, pfx, plen) == 0) {
            return g_dxcc_db.prefixes[i].country;
        }
    }
    return NULL;
}

/**
 * @brief Slår upp DXCC-land för en given anropssignal.
 * Hanterar portabelanrop (t.ex. SM5UEI/P, LA/SM5UEI, VP8/SM5UEI) och undantag.
 * @param call_in Inkommande anropssignal.
 * @param out Utdatabuffer för landets namn.
 * @param out_sz Storlek på utdatabuffert.
 */
void lookup_dxcc(const char *call_in, char *out, size_t out_sz) {
    if (out == NULL || out_sz == 0) return;
    out[0] = '\0';
    if (call_in == NULL || call_in[0] == '\0') return;

    char call[32];
    size_t j = 0;
    for (const char *p = call_in; *p != '\0' && j < sizeof(call) - 1; ++p) {
        if (!isspace((unsigned char)*p)) {
            call[j++] = (char)toupper((unsigned char)*p);
        }
    }
    call[j] = '\0';

    if (j < 2) return;

    /* 1. Kontrollera exakt matchning */
    const char *exact = match_exact_dxcc(call);
    if (exact != NULL) {
        copy_str(out, out_sz, exact);
        return;
    }

    /* 2. Hantera anrop med snedstreck */
    char *slash = strchr(call, '/');
    if (slash != NULL) {
        for (size_t i = 0; i < g_dxcc_db.prefix_count; ++i) {
            const char *pfx = g_dxcc_db.prefixes[i].key;
            if (strchr(pfx, '/') != NULL && strncmp(call, pfx, strlen(pfx)) == 0) {
                copy_str(out, out_sz, g_dxcc_db.prefixes[i].country);
                return;
            }
        }

        char part1[32], part2[32];
        size_t len1 = (size_t)(slash - call);
        if (len1 >= sizeof(part1)) len1 = sizeof(part1) - 1;
        memcpy(part1, call, len1);
        part1[len1] = '\0';
        copy_str(part2, sizeof(part2), slash + 1);
        char *second_slash = strchr(part2, '/');
        if (second_slash) *second_slash = '\0';

        if (is_modifier_suffix(part2)) {
            if (strlen(part1) >= 2) {
                const char *c1 = match_exact_dxcc(part1);
                if (!c1) c1 = match_prefix_dxcc(part1);
                if (c1) copy_str(out, out_sz, c1);
            }
            return;
        }

        const char *cand = (strlen(part1) <= strlen(part2) && strlen(part1) >= 1) ? part1 : part2;
        const char *cc = match_prefix_dxcc(cand);
        if (cc != NULL) {
            copy_str(out, out_sz, cc);
            return;
        }
    }

    /* 3. Normal prefixmatchning */
    const char *pfx_country = match_prefix_dxcc(call);
    if (pfx_country != NULL) {
        copy_str(out, out_sz, pfx_country);
    }
}
