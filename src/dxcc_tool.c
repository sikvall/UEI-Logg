/**
 * @file dxcc_tool.c
 * @brief Hjälpverktyg för UEI-Logg för att hantera och uppdatera dxcc.txt.
 *
 * Stöder kommandona:
 *   - update : Laddar ner och uppdaterar dxcc.txt från www.country-files.com (AD1C)
 *   - stat   : Läser dxcc.txt och visar utförlig statistik i en snygg tabell
 *   - lookup : Slår upp DXCC-land för en anropssignal direkt i terminalen
 *   - help   : Visar användarinstruktioner
 */

#define _XOPEN_SOURCE 700

#include "util.h"
#include <ctype.h>
#include <curl/curl.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_CTY_URL "https://www.country-files.com/cty/cty.csv"
#define DEFAULT_DXCC_FILE "dxcc.txt"
#define USER_AGENT "UEI-Logg-dxcc/1.0 (amateur radio log tool; contact: sm5uei@avitec.se)"

/* Svensk översättningstabell som fallback för entiteter vars namn skiljer sig från engelska */
static const struct {
    const char *en;
    const char *sv;
} SWEDISH_FALLBACK[] = {
    {"African Italy", "Afrikanska Italien"},
    {"Aland Islands", "Åland"},
    {"Albania", "Albanien"},
    {"Algeria", "Algeriet"},
    {"American Samoa", "Amerikanska Samoa"},
    {"Antarctica", "Antarktis"},
    {"Armenia", "Armenien"},
    {"Asiatic Russia", "Asiatiska Ryssland"},
    {"Asiatic Turkey", "Asiatiska Turkiet"},
    {"Australia", "Australien"},
    {"Austria", "Österrike"},
    {"Azerbaijan", "Azerbajdzjan"},
    {"Azores", "Azorerna"},
    {"Balearic Islands", "Balearerna"},
    {"Bear Island", "Björnön"},
    {"Belarus", "Vitryssland"},
    {"Belgium", "Belgien"},
    {"Bosnia-Herzegovina", "Bosnien-Hercegovina"},
    {"Brazil", "Brasilien"},
    {"Bulgaria", "Bulgarien"},
    {"Cambodia", "Kambodja"},
    {"Canada", "Kanada"},
    {"Canary Islands", "Kanarieöarna"},
    {"Cayman Islands", "Caymanöarna"},
    {"China", "Kina"},
    {"Corsica", "Korsika"},
    {"Crete", "Kreta"},
    {"Croatia", "Kroatien"},
    {"Cuba", "Kuba"},
    {"Cyprus", "Cypern"},
    {"Czech Republic", "Tjeckien"},
    {"Denmark", "Danmark"},
    {"Dodecanese", "Dodekaneserna"},
    {"Dominican Republic", "Dominikanska republiken"},
    {"East Malaysia", "Östmalaysia"},
    {"Easter Island", "Påskön"},
    {"Egypt", "Egypten"},
    {"Estonia", "Estland"},
    {"European Russia", "Europeiska Ryssland"},
    {"European Turkey", "Europeiska Turkiet"},
    {"Falkland Islands", "Falklandsöarna"},
    {"Faroe Islands", "Färöarna"},
    {"Fed. Rep. of Germany", "Tyskland"},
    {"France", "Frankrike"},
    {"French Polynesia", "Franska Polynesien"},
    {"Galapagos Islands", "Galápagos"},
    {"Georgia", "Georgien"},
    {"Greece", "Grekland"},
    {"Greenland", "Grönland"},
    {"Hong Kong", "Hongkong"},
    {"Hungary", "Ungern"},
    {"Iceland", "Island"},
    {"India", "Indien"},
    {"Indonesia", "Indonesien"},
    {"Iraq", "Irak"},
    {"Ireland", "Irland"},
    {"Italy", "Italien"},
    {"Jordan", "Jordanien"},
    {"Juan Fernandez Islands", "Juan Fernández"},
    {"Juan de Nova & Europa", "Juan de Nova, Europa"},
    {"Kazakhstan", "Kazakstan"},
    {"Kingdom of Eswatini", "Eswatini"},
    {"Kyrgyzstan", "Kirgizistan"},
    {"Latvia", "Lettland"},
    {"Lebanon", "Libanon"},
    {"Libya", "Libyen"},
    {"Lithuania", "Litauen"},
    {"Luxembourg", "Luxemburg"},
    {"Madeira Islands", "Madeira"},
    {"Maldives", "Maldiverna"},
    {"Market Reef", "Märket"},
    {"Mexico", "Mexiko"},
    {"Moldova", "Moldavien"},
    {"Mongolia", "Mongoliet"},
    {"Morocco", "Marocko"},
    {"Mount Athos", "Athos"},
    {"Netherlands", "Nederländerna"},
    {"New Caledonia", "Nya Kaledonien"},
    {"New Zealand", "Nya Zeeland"},
    {"North Macedonia", "Nordmakedonien"},
    {"Northern Ireland", "Nordirland"},
    {"Norway", "Norge"},
    {"Peter 1 Island", "Peter I"},
    {"Philippines", "Filippinerna"},
    {"Poland", "Polen"},
    {"Pratas Island", "Pratas"},
    {"Republic of Korea", "Sydkorea"},
    {"Republic of Kosovo", "Kosovo"},
    {"Reunion Island", "Réunion"},
    {"Romania", "Rumänien"},
    {"Rotuma Island", "Rotuma"},
    {"Sardinia", "Sardinien"},
    {"Saudi Arabia", "Saudiarabien"},
    {"Scotland", "Skottland"},
    {"Serbia", "Serbien"},
    {"Sicily", "Sicilien"},
    {"Slovak Republic", "Slovakien"},
    {"Slovenia", "Slovenien"},
    {"South Africa", "Sydafrika"},
    {"South Georgia Island", "Sydgeorgien"},
    {"South Orkney Islands", "Sydorkneyöarna"},
    {"South Sandwich Islands", "Sydsandwichöarna"},
    {"South Shetland Islands", "Sydshetlandsöarna"},
    {"Sov Mil Order of Malta", "Malteserorden"},
    {"Spain", "Spanien"},
    {"Spratly Islands", "Spratlyöarna"},
    {"Sweden", "Sverige"},
    {"Switzerland", "Schweiz"},
    {"Syria", "Syrien"},
    {"Tajikistan", "Tadzjikistan"},
    {"Tristan da Cunha & Gough Islands", "Tristan da Cunha & Gough"},
    {"Tunisia", "Tunisien"},
    {"Ukraine", "Ukraina"},
    {"United Arab Emirates", "Förenade Arabemiraten"},
    {"United Nations HQ", "FN HQ"},
    {"United States", "USA"},
    {"West Malaysia", "Västmalaysia"},
    {"Western Sahara", "Västsahara"},
    {"Yemen", "Jemen"},
    {NULL, NULL}
};

/**
 * @brief Slår upp svenskt namn från den inbäddade fallbacktabellen.
 */
static const char *get_swedish_fallback_name(const char *eng) {
    if (!eng || !eng[0]) return "";
    for (int i = 0; SWEDISH_FALLBACK[i].en != NULL; ++i) {
        if (strcasecmp(SWEDISH_FALLBACK[i].en, eng) == 0) {
            return SWEDISH_FALLBACK[i].sv;
        }
    }
    return eng;
}

/**
 * @brief Struktur för en enskild DXCC-entitet.
 */
typedef struct {
    char primary_pfx[32];
    char name_en[64];
    char name_sv[64];
    int adif;
    char cont[8];
    int cq;
    int itu;
    double lat;
    double lon;
    double tz;
    int pfx_count;
    int exact_count;
} EntityInfo;

/**
 * @brief Enkel regel (prefix eller exakt matchning).
 */
typedef struct {
    char key[32];
    char country[64];
    int is_exact;
} RuleItem;

/**
 * @brief Dynamisk lista av regler.
 */
typedef struct {
    RuleItem *items;
    size_t count;
    size_t cap;
} RuleList;

static void rule_list_init(RuleList *rl) {
    rl->items = NULL;
    rl->count = 0;
    rl->cap = 0;
}

static void rule_list_push(RuleList *rl, const char *key, const char *country, int is_exact) {
    if (rl->count >= rl->cap) {
        size_t ncap = (rl->cap == 0) ? 1024 : rl->cap * 2;
        RuleItem *narr = realloc(rl->items, ncap * sizeof(RuleItem));
        if (!narr) return;
        rl->items = narr;
        rl->cap = ncap;
    }
    copy_str(rl->items[rl->count].key, sizeof(rl->items[0].key), key);
    copy_str(rl->items[rl->count].country, sizeof(rl->items[0].country), country);
    rl->items[rl->count].is_exact = is_exact;
    rl->count++;
}

static void rule_list_free(RuleList *rl) {
    if (rl->items) free(rl->items);
    rl->items = NULL;
    rl->count = 0;
    rl->cap = 0;
}

/**
 * @brief Jämförelsefunktion för qsort för prefixregler:
 * Längre prefix först, därefter alfabetiskt.
 */
static int cmp_rule_prefix_desc(const void *a, const void *b) {
    const RuleItem *ra = (const RuleItem *)a;
    const RuleItem *rb = (const RuleItem *)b;
    /* Snedstrecksprefix (t.ex. 3D2/C) sorteras allra först */
    int a_slash = (strchr(ra->key, '/') != NULL);
    int b_slash = (strchr(rb->key, '/') != NULL);
    if (a_slash != b_slash) {
        return b_slash - a_slash;
    }
    size_t la = strlen(ra->key);
    size_t lb = strlen(rb->key);
    if (la != lb) {
        return (lb > la) ? 1 : -1;
    }
    return strcmp(ra->key, rb->key);
}

/**
 * @brief Jämförelsefunktion för exakta signalundantag (alfabetiskt).
 */
static int cmp_rule_exact_asc(const void *a, const void *b) {
    const RuleItem *ra = (const RuleItem *)a;
    const RuleItem *rb = (const RuleItem *)b;
    return strcmp(ra->key, rb->key);
}

/**
 * @brief Läser in existerande svenskt namnskick från befintlig dxcc.txt.
 */
static void load_existing_swedish_names(const char *path, RuleList *dict) {
    FILE *fp = fopen(path, "r");
    if (!fp) return;
    char line[256];
    while (fgets(line, sizeof(line), fp)) {
        trim_newline(line);
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#') continue;
        char *sep = strchr(p, ';');
        if (!sep) continue;
        *sep = '\0';
        const char *k = p;
        const char *c = sep + 1;
        while (*c == ' ' || *c == '\t') c++;
        if (k[0] == '=') k++;
        if (k[0] && c[0]) {
            rule_list_push(dict, k, c, 0);
        }
    }
    fclose(fp);
}

static const char *find_existing_swedish_name(const RuleList *dict, const char *key) {
    if (!dict || !dict->items) return NULL;
    for (size_t i = 0; i < dict->count; ++i) {
        if (strcasecmp(dict->items[i].key, key) == 0) {
            return dict->items[i].country;
        }
    }
    return NULL;
}

static char *fetch_content(const char *source, size_t *out_size) {
    if (strncmp(source, "http://", 7) == 0 || strncmp(source, "https://", 8) == 0) {
        /* Ladda ner via libcurl */
        CURL *curl = curl_easy_init();
        if (!curl) {
            fprintf(stderr, "Fel: Kunde inte initialisera cURL.\n");
            return NULL;
        }
        CurlBuf mem = {NULL, 0};
        curl_easy_setopt(curl, CURLOPT_URL, source);
        curl_easy_setopt(curl, CURLOPT_USERAGENT, USER_AGENT);
        curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, curl_write_cb);
        curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&mem);
        curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(curl, CURLOPT_TIMEOUT, 20L);

        CURLcode res = curl_easy_perform(curl);
        long http_code = 0;
        curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &http_code);
        curl_easy_cleanup(curl);

        if (res != CURLE_OK) {
            fprintf(stderr, "Nätverksfel vid hämtning av %s:\n  %s\n", source, curl_easy_strerror(res));
            if (mem.data) free(mem.data);
            return NULL;
        }
        if (http_code != 200) {
            fprintf(stderr, "HTTP-fel vid hämtning av %s: Kod %ld\n", source, http_code);
            if (mem.data) free(mem.data);
            return NULL;
        }
        if (out_size) *out_size = mem.len;
        return mem.data;
    } else {
        /* Läs från lokal fil */
        FILE *fp = fopen(source, "rb");
        if (!fp) {
            fprintf(stderr, "Fel: Kunde inte öppna lokal källfil '%s': %s\n", source, strerror(errno));
            return NULL;
        }
        fseek(fp, 0, SEEK_END);
        long sz = ftell(fp);
        fseek(fp, 0, SEEK_SET);
        if (sz < 0) { fclose(fp); return NULL; }
        char *buf = malloc((size_t)sz + 1);
        if (!buf) { fclose(fp); return NULL; }
        size_t rd = fread(buf, 1, (size_t)sz, fp);
        fclose(fp);
        buf[rd] = '\0';
        if (out_size) *out_size = rd;
        return buf;
    }
}

/**
 * @brief Rensar ett prefix eller signal från zoner och attribut, t.ex. (14), [28], {41.9/12.4}, ~1.0~.
 */
static void clean_token(char *token) {
    char *src = token;
    char *dst = token;
    while (*src) {
        if (*src == '(') {
            while (*src && *src != ')') src++;
            if (*src == ')') src++;
        } else if (*src == '[') {
            while (*src && *src != ']') src++;
            if (*src == ']') src++;
        } else if (*src == '{') {
            while (*src && *src != '}') src++;
            if (*src == '}') src++;
        } else if (*src == '~') {
            while (*src && *src != '~') src++;
            if (*src == '~') src++;
        } else {
            *dst++ = *src++;
        }
    }
    *dst = '\0';
}

/**
 * @brief Implementering av kommandot "update":
 * Hämtar cty.csv, tolkar regler, bevarar svenskt namnskick och skriver dxcc.txt.
 */
static int cmd_update(const char *source_url) {
    const char *url = (source_url && source_url[0]) ? source_url : DEFAULT_CTY_URL;
    printf("DXCC-uppdatering startar...\n");
    printf("Källa: %s\n", url);

    /* 1. Läs in existerande dxcc.txt för att bevara svenska landsnamn */
    RuleList existing_dict;
    rule_list_init(&existing_dict);
    load_existing_swedish_names(DEFAULT_DXCC_FILE, &existing_dict);
    printf("Inläst existerande namnminne: %zu nycklar från %s\n", existing_dict.count, DEFAULT_DXCC_FILE);

    /* 2. Hämta cty-data från källa */
    size_t raw_sz = 0;
    char *raw = fetch_content(url, &raw_sz);
    if (!raw) {
        rule_list_free(&existing_dict);
        return 1;
    }
    printf("Hämtade %zu bytes rådata.\n", raw_sz);

    /* 3. Tolka cty.csv rader */
    RuleList prefixes;
    RuleList exacts;
    rule_list_init(&prefixes);
    rule_list_init(&exacts);

    EntityInfo entities[512];
    int entity_count = 0;

    char *saveptr = NULL;
    char *line = strtok_r(raw, "\r\n", &saveptr);
    while (line != NULL) {
        if (line[0] == '\0') {
            line = strtok_r(NULL, "\r\n", &saveptr);
            continue;
        }

        /* cty.csv format:
         * 0: primärprefix (t.ex. "3D2/c" el. "*4U1V")
         * 1: engelskt namn (t.ex. "Conway Reef")
         * 2: adif dxcc-kod (t.ex. "489")
         * 3: kontinent (t.ex. "OC")
         * 4: cq-zon
         * 5: itu-zon
         * 6: latitud
         * 7: longitud
         * 8: utc-offset
         * 9: lista av alias/prefix separerade med mellanslag och avslutas med ';'
         */
        char *f[10];
        char *p = line;
        for (int i = 0; i < 9; ++i) {
            f[i] = p;
            char *comma = strchr(p, ',');
            if (!comma) {
                f[i] = NULL;
                break;
            }
            *comma = '\0';
            p = comma + 1;
        }
        f[9] = p;

        if (f[0] && f[1] && f[2] && f[3] && f[9] && entity_count < 512) {
            char prim[32];
            copy_str(prim, sizeof(prim), f[0]);
            /* Ta bort eventuell inledande asterisk */
            char *prim_clean = prim;
            if (prim_clean[0] == '*') prim_clean++;

            const char *eng_name = f[1];
            int adif = atoi(f[2]);
            const char *cont = f[3];
            int cq = atoi(f[4] ? f[4] : "0");
            int itu = atoi(f[5] ? f[5] : "0");
            double lat = f[6] ? atof(f[6]) : 0.0;
            double lon = f[7] ? atof(f[7]) : 0.0;
            double tz  = f[8] ? atof(f[8]) : 0.0;

            /* Bestäm svenskt namn:
             * 1. Kolla om primärprefix finns i befintlig dxcc.txt
             * 2. Kolla om något alias finns i befintlig dxcc.txt
             * 3. Kolla i inbäddad fallbacktabell
             * 4. Använd engelskt namn
             */
            const char *swe_name = find_existing_swedish_name(&existing_dict, prim_clean);
            char aliases_buf[4096];
            copy_str(aliases_buf, sizeof(aliases_buf), f[9]);
            /* Ta bort avslutande semikolon */
            char *sc = strrchr(aliases_buf, ';');
            if (sc) *sc = '\0';

            if (!swe_name) {
                char tmp_aliases[4096];
                copy_str(tmp_aliases, sizeof(tmp_aliases), aliases_buf);
                char *asave = NULL;
                char *tok = strtok_r(tmp_aliases, " \t", &asave);
                while (tok) {
                    char clean_tok[64];
                    copy_str(clean_tok, sizeof(clean_tok), tok);
                    clean_token(clean_tok);
                    char *chk = clean_tok;
                    if (chk[0] == '=') chk++;
                    swe_name = find_existing_swedish_name(&existing_dict, chk);
                    if (swe_name) break;
                    tok = strtok_r(NULL, " \t", &asave);
                }
            }

            if (!swe_name) {
                swe_name = get_swedish_fallback_name(eng_name);
            }

            /* Registrera entitetsinfo */
            EntityInfo *ent = &entities[entity_count++];
            copy_str(ent->primary_pfx, sizeof(ent->primary_pfx), prim_clean);
            to_upper_str(ent->primary_pfx);
            copy_str(ent->name_en, sizeof(ent->name_en), eng_name);
            copy_str(ent->name_sv, sizeof(ent->name_sv), swe_name);
            ent->adif = adif;
            copy_str(ent->cont, sizeof(ent->cont), cont);
            ent->cq = cq;
            ent->itu = itu;
            ent->lat = lat;
            ent->lon = lon;
            ent->tz = tz;
            ent->pfx_count = 0;
            ent->exact_count = 0;

            /* Om primärprefixet innehåller snedstreck (t.ex. 3D2/C, HK0/A), spara som prefix */
            if (strchr(ent->primary_pfx, '/') != NULL) {
                rule_list_push(&prefixes, ent->primary_pfx, swe_name, 0);
                ent->pfx_count++;
            }

            /* Tolka alla alias i lista */
            char *asave = NULL;
            char *tok = strtok_r(aliases_buf, " \t", &asave);
            while (tok) {
                char clean_tok[64];
                copy_str(clean_tok, sizeof(clean_tok), tok);
                clean_token(clean_tok);
                if (clean_tok[0] != '\0' && strcmp(clean_tok, "=VERSION") != 0) {
                    if (clean_tok[0] == '=') {
                        to_upper_str(clean_tok + 1);
                        rule_list_push(&exacts, clean_tok + 1, swe_name, 1);
                        ent->exact_count++;
                    } else {
                        to_upper_str(clean_tok);
                        rule_list_push(&prefixes, clean_tok, swe_name, 0);
                        ent->pfx_count++;
                    }
                }
                tok = strtok_r(NULL, " \t", &asave);
            }
        }

        line = strtok_r(NULL, "\r\n", &saveptr);
    }

    free(raw);
    rule_list_free(&existing_dict);

    printf("Hittade %d DXCC-entiteter.\n", entity_count);
    printf("Extraherade %zu prefixregler och %zu signalundantag.\n", prefixes.count, exacts.count);

    /* Sortera regler */
    if (prefixes.count > 1) {
        qsort(prefixes.items, prefixes.count, sizeof(RuleItem), cmp_rule_prefix_desc);
    }
    if (exacts.count > 1) {
        qsort(exacts.items, exacts.count, sizeof(RuleItem), cmp_rule_exact_asc);
    }

    /* 4. Skapa backup av befintlig dxcc.txt */
    struct stat st;
    if (stat(DEFAULT_DXCC_FILE, &st) == 0) {
        char bak_name[64];
        snprintf(bak_name, sizeof(bak_name), "%s.bak", DEFAULT_DXCC_FILE);
        unlink(bak_name);
        if (rename(DEFAULT_DXCC_FILE, bak_name) == 0) {
            printf("Skapade backup: %s -> %s\n", DEFAULT_DXCC_FILE, bak_name);
        }
    }

    /* 5. Skriv ny dxcc.txt */
    FILE *fp = fopen(DEFAULT_DXCC_FILE, "w");
    if (!fp) {
        fprintf(stderr, "Fel: Kunde inte öppna '%s' för skrivning: %s\n", DEFAULT_DXCC_FILE, strerror(errno));
        rule_list_free(&prefixes);
        rule_list_free(&exacts);
        return 1;
    }

    time_t now = time(NULL);
    struct tm tm_utc;
    gmtime_r(&now, &tm_utc);
    char time_str[64];
    strftime(time_str, sizeof(time_str), "%Y-%m-%d %H:%M:%S UTC", &tm_utc);

    fprintf(fp, "# DXCC-prefixdatabas för UEI-Logg\n");
    fprintf(fp, "# Uppdaterad: %s\n", time_str);
    fprintf(fp, "# Källa: Country Files (AD1C) - %s\n", url);
    fprintf(fp, "# Entiteter: %d | Prefix: %zu | Undantag: %zu | Totalt antal regler: %zu\n",
            entity_count, prefixes.count, exacts.count, prefixes.count + exacts.count);
    fprintf(fp, "#\n");
    fprintf(fp, "# Format:\n");
    fprintf(fp, "#   PREFIX;DXCC-land\n");
    fprintf(fp, "#   =EXAKT_SIGNAL;DXCC-land\n");
    fprintf(fp, "#\n");
    fprintf(fp, "# Entitetskatalog med geodata och zoner (används av 'dxcc stat'):\n");
    for (int i = 0; i < entity_count; ++i) {
        fprintf(fp, "# @E:%d:%s:%d:%d:%.2f:%.2f:%.1f:%s:%s:%s\n",
                entities[i].adif,
                entities[i].cont,
                entities[i].cq,
                entities[i].itu,
                entities[i].lat,
                entities[i].lon,
                entities[i].tz,
                entities[i].primary_pfx,
                entities[i].name_en,
                entities[i].name_sv);
    }
    fprintf(fp, "#\n");
    fprintf(fp, "# --- PREFIXREGLER ---\n");

    /* Skriv deduplicerade prefixregler */
    size_t written_pfx = 0;
    for (size_t i = 0; i < prefixes.count; ++i) {
        if (i > 0 && strcmp(prefixes.items[i].key, prefixes.items[i - 1].key) == 0 &&
            strcmp(prefixes.items[i].country, prefixes.items[i - 1].country) == 0) {
            continue;
        }
        fprintf(fp, "%s;%s\n", prefixes.items[i].key, prefixes.items[i].country);
        written_pfx++;
    }

    fprintf(fp, "#\n# --- EXAKTA SIGNALUNDANTAG ---\n");
    size_t written_exact = 0;
    for (size_t i = 0; i < exacts.count; ++i) {
        if (i > 0 && strcmp(exacts.items[i].key, exacts.items[i - 1].key) == 0 &&
            strcmp(exacts.items[i].country, exacts.items[i - 1].country) == 0) {
            continue;
        }
        fprintf(fp, "=%s;%s\n", exacts.items[i].key, exacts.items[i].country);
        written_exact++;
    }

    fclose(fp);
    rule_list_free(&prefixes);
    rule_list_free(&exacts);

    printf("\n\033[1;32m✓ DXCC-uppdatering slutförd framgångsrikt!\033[0m\n");
    printf("  Sparad fil:        %s\n", DEFAULT_DXCC_FILE);
    printf("  DXCC-entiteter:    %d\n", entity_count);
    printf("  Prefixregler:      %zu\n", written_pfx);
    printf("  Signalundantag:    %zu\n", written_exact);
    printf("  Totalt antal rader:%zu\n", written_pfx + written_exact);
    printf("  Tidsstämpel:       %s\n", time_str);

    return 0;
}

/**
 * @brief Hjälpfunktion för att formatera tusental i tal (t.ex. 4 819).
 */
static void format_thousands(size_t val, char *buf, size_t buf_sz) {
    char tmp[32];
    snprintf(tmp, sizeof(tmp), "%zu", val);
    int len = (int)strlen(tmp);
    int out_idx = 0;
    for (int i = 0; i < len; ++i) {
        if (i > 0 && (len - i) % 3 == 0 && out_idx + 1 < (int)buf_sz) {
            buf[out_idx++] = ' ';
        }
        if (out_idx + 1 < (int)buf_sz) {
            buf[out_idx++] = tmp[i];
        }
    }
    buf[out_idx] = '\0';
}

/**
 * @brief Implementering av kommandot "stat":
 * Läser dxcc.txt och presenterar en snygg översiktstabell.
 */
static int cmd_stat(const char *target_file) {
    const char *file_path = (target_file && target_file[0]) ? target_file : DEFAULT_DXCC_FILE;

    struct stat st;
    if (stat(file_path, &st) != 0) {
        fprintf(stderr, "Fel: Kunde inte läsa '%s': %s\n", file_path, strerror(errno));
        fprintf(stderr, "Tips: Kör 'dxcc update' för att ladda ner och generera dxcc.txt.\n");
        return 1;
    }

    FILE *fp = fopen(file_path, "r");
    if (!fp) {
        fprintf(stderr, "Fel: Kunde inte öppna '%s': %s\n", file_path, strerror(errno));
        return 1;
    }

    char update_time[64] = "Okänd";
    char source_info[128] = "Okänd";

    typedef struct {
        char name[64];
        char cont[8];
        int pfx_count;
        int exact_count;
    } CountryStat;

    CountryStat *cstats = NULL;
    size_t cstat_count = 0;
    size_t cstat_cap = 0;

    size_t total_pfx = 0;
    size_t total_exact = 0;
    size_t total_lines = 0;
    size_t min_pfx_len = 999;
    size_t max_pfx_len = 0;
    size_t sum_pfx_len = 0;

    /* Kontinenter: EU, AS, NA, SA, AF, OC, AN */
    struct {
        const char *code;
        const char *name;
        int entities;
        int pfx;
        int exact;
    } continents[] = {
        {"EU", "Europa", 0, 0, 0},
        {"AS", "Asien", 0, 0, 0},
        {"NA", "Nordamerika", 0, 0, 0},
        {"SA", "Sydamerika", 0, 0, 0},
        {"AF", "Afrika", 0, 0, 0},
        {"OC", "Oceanien", 0, 0, 0},
        {"AN", "Antarktis", 0, 0, 0},
        {NULL, NULL, 0, 0, 0}
    };

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        total_lines++;
        trim_newline(line);
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0') continue;

        if (strncmp(p, "# Uppdaterad:", 13) == 0) {
            copy_str(update_time, sizeof(update_time), p + 13);
            trim_str(update_time);
            continue;
        }
        if (strncmp(p, "# Källa:", 8) == 0) {
            copy_str(source_info, sizeof(source_info), p + 8);
            trim_str(source_info);
            continue;
        }
        if (strncmp(p, "# @E:", 5) == 0) {
            /* @E:adif:cont:cq:itu:lat:lon:tz:prim:eng:sv */
            char *tokens[10];
            char *cur = p + 5;
            for (int i = 0; i < 9; ++i) {
                tokens[i] = cur;
                char *col = strchr(cur, ':');
                if (!col) break;
                *col = '\0';
                cur = col + 1;
            }
            tokens[9] = cur;
            const char *cont = tokens[1];
            const char *swe  = tokens[9] ? tokens[9] : tokens[8];
            if (cont && swe) {
                for (int c = 0; continents[c].code != NULL; ++c) {
                    if (strcasecmp(continents[c].code, cont) == 0) {
                        continents[c].entities++;
                        break;
                    }
                }
                /* Registrera land i cstats */
                int found = -1;
                for (size_t i = 0; i < cstat_count; ++i) {
                    if (strcmp(cstats[i].name, swe) == 0) {
                        found = (int)i;
                        break;
                    }
                }
                if (found < 0) {
                    if (cstat_count >= cstat_cap) {
                        size_t nc = (cstat_cap == 0) ? 512 : cstat_cap * 2;
                        CountryStat *tmp = realloc(cstats, nc * sizeof(CountryStat));
                        if (tmp) {
                            cstats = tmp;
                            cstat_cap = nc;
                        }
                    }
                    if (cstat_count < cstat_cap) {
                        copy_str(cstats[cstat_count].name, sizeof(cstats[0].name), swe);
                        copy_str(cstats[cstat_count].cont, sizeof(cstats[0].cont), cont);
                        cstats[cstat_count].pfx_count = 0;
                        cstats[cstat_count].exact_count = 0;
                        cstat_count++;
                    }
                }
            }
            continue;
        }

        if (*p == '#') continue;

        char *sep = strchr(p, ';');
        if (!sep) continue;
        *sep = '\0';
        const char *k = p;
        const char *country = sep + 1;
        while (*country == ' ' || *country == '\t') country++;

        int is_exact = (k[0] == '=');
        if (is_exact) k++;

        /* Sök eller lägg till land i cstats */
        int c_idx = -1;
        for (size_t i = 0; i < cstat_count; ++i) {
            if (strcmp(cstats[i].name, country) == 0) {
                c_idx = (int)i;
                break;
            }
        }
        if (c_idx < 0) {
            if (cstat_count >= cstat_cap) {
                size_t nc = (cstat_cap == 0) ? 512 : cstat_cap * 2;
                CountryStat *tmp = realloc(cstats, nc * sizeof(CountryStat));
                if (tmp) {
                    cstats = tmp;
                    cstat_cap = nc;
                }
            }
            if (cstat_count < cstat_cap) {
                copy_str(cstats[cstat_count].name, sizeof(cstats[0].name), country);
                cstats[cstat_count].cont[0] = '\0';
                cstats[cstat_count].pfx_count = 0;
                cstats[cstat_count].exact_count = 0;
                c_idx = (int)cstat_count++;
            }
        }

        if (is_exact) {
            total_exact++;
            if (c_idx >= 0) cstats[c_idx].exact_count++;
        } else {
            total_pfx++;
            size_t plen = strlen(k);
            if (plen < min_pfx_len) min_pfx_len = plen;
            if (plen > max_pfx_len) max_pfx_len = plen;
            sum_pfx_len += plen;
            if (c_idx >= 0) cstats[c_idx].pfx_count++;
        }
    }
    fclose(fp);

    /* Summera kontinenter från länder om entitetskatalog saknades */
    int has_catalog = 0;
    for (int c = 0; continents[c].code != NULL; ++c) {
        if (continents[c].entities > 0) has_catalog = 1;
    }
    if (has_catalog) {
        for (size_t i = 0; i < cstat_count; ++i) {
            if (cstats[i].cont[0] != '\0') {
                for (int c = 0; continents[c].code != NULL; ++c) {
                    if (strcasecmp(continents[c].code, cstats[i].cont) == 0) {
                        continents[c].pfx += cstats[i].pfx_count;
                        continents[c].exact += cstats[i].exact_count;
                        break;
                    }
                }
            }
        }
    }

    char str_sz[32];
    if (st.st_size >= 1024 * 1024) {
        snprintf(str_sz, sizeof(str_sz), "%.1f MB", (double)st.st_size / (1024.0 * 1024.0));
    } else {
        snprintf(str_sz, sizeof(str_sz), "%.1f KB", (double)st.st_size / 1024.0);
    }

    char time_buf[64];
    struct tm tm_file;
    localtime_r(&st.st_mtime, &tm_file);
    strftime(time_buf, sizeof(time_buf), "%Y-%m-%d %H:%M:%S", &tm_file);

    char num_pfx[32], num_exact[32], num_tot[32], num_lines[32];
    format_thousands(total_pfx, num_pfx, sizeof(num_pfx));
    format_thousands(total_exact, num_exact, sizeof(num_exact));
    format_thousands(total_pfx + total_exact, num_tot, sizeof(num_tot));
    format_thousands(total_lines, num_lines, sizeof(num_lines));

    /* RITA TABELL 1: Allmän databasöversikt */
    printf("\n\033[1;36m┌──────────────────────────────────────────────────────────────────────────────┐\033[0m\n");
    printf("\033[1;36m│\033[0m \033[1;37m                       DXCC DATABASSTATISTIK                                \033[0m\033[1;36m│\033[0m\n");
    printf("\033[1;36m├──────────────────────────────────────────────────────────────────────────────┤\033[0m\n");
    printf("\033[1;36m│\033[0m  Filnamn:            %-56s\033[1;36m│\033[0m\n", file_path);
    char sz_line[128];
    snprintf(sz_line, sizeof(sz_line), "%s (%s rader)", str_sz, num_lines);
    printf("\033[1;36m│\033[0m  Filstorlek:         %-56s\033[1;36m│\033[0m\n", sz_line);
    printf("\033[1;36m│\033[0m  Senast ändrad:      %-56s\033[1;36m│\033[0m\n", time_buf);
    if (strcmp(update_time, "Okänd") != 0) {
        printf("\033[1;36m│\033[0m  Databas uppdaterad: %-56s\033[1;36m│\033[0m\n", update_time);
    }
    printf("\033[1;36m│\033[0m  Datakälla:          %-56s\033[1;36m│\033[0m\n", source_info);
    printf("\033[1;36m├──────────────────────────────────────────────────────────────────────────────┤\033[0m\n");
    printf("\033[1;36m│\033[0m  \033[1mAktiva DXCC-entiteter:\033[0m      \033[1;32m%5zu\033[0m                                             \033[1;36m│\033[0m\n", cstat_count);
    printf("\033[1;36m│\033[0m  \033[1mPrefixregler:\033[0m               \033[1;33m%7s\033[0m                                           \033[1;36m│\033[0m\n", num_pfx);
    printf("\033[1;36m│\033[0m  \033[1mExakta signalundantag:\033[0m      \033[1;33m%7s\033[0m                                           \033[1;36m│\033[0m\n", num_exact);
    printf("\033[1;36m│\033[0m  \033[1mTotalt antal regler:\033[0m        \033[1;32m%7s\033[0m                                           \033[1;36m│\033[0m\n", num_tot);
    if (total_pfx > 0) {
        double avg = (double)sum_pfx_len / (double)total_pfx;
        char pfx_stat_str[64];
        snprintf(pfx_stat_str, sizeof(pfx_stat_str), "%zu min / %zu max (snitt %.1f tecken)", min_pfx_len, max_pfx_len, avg);
        printf("\033[1;36m│\033[0m  Prefixlängder:              %-56s\033[1;36m│\033[0m\n", pfx_stat_str);
    }
    printf("\033[1;36m└──────────────────────────────────────────────────────────────────────────────┘\033[0m\n");

    /* RITA TABELL 2: Fördelning per kontinent om geodata finns */
    if (has_catalog) {
        printf("\n\033[1;36m┌──────────────────────────────────┬──────────┬──────────┬───────────┬─────────┐\033[0m\n");
        printf("\033[1;36m│\033[0m \033[1mKontinent                        \033[0m\033[1;36m│\033[0m \033[1m  DXCC   \033[0m\033[1;36m│\033[0m \033[1m  Prefix \033[0m\033[1;36m│\033[0m \033[1m Undantag \033[0m\033[1;36m│\033[0m \033[1m  Totalt \033[0m\033[1;36m│\033[0m\n");
        printf("\033[1;36m├──────────────────────────────────┼──────────┼──────────┼───────────┼─────────┤\033[0m\n");
        size_t sum_c_ent = 0, sum_c_pfx = 0, sum_c_ex = 0;
        for (int c = 0; continents[c].code != NULL; ++c) {
            char pfx_fmt[16], ex_fmt[16], tot_fmt[16];
            format_thousands((size_t)continents[c].pfx, pfx_fmt, sizeof(pfx_fmt));
            format_thousands((size_t)continents[c].exact, ex_fmt, sizeof(ex_fmt));
            format_thousands((size_t)(continents[c].pfx + continents[c].exact), tot_fmt, sizeof(tot_fmt));
            char title[64];
            snprintf(title, sizeof(title), "%s (%s)", continents[c].name, continents[c].code);
            int tw = utf8_display_width(title);
            int pad = 32 - tw;
            if (pad < 0) pad = 0;
            printf("\033[1;36m│\033[0m %s%*s \033[1;36m│\033[0m   %4d   \033[1;36m│\033[0m  %7s \033[1;36m│\033[0m   %7s \033[1;36m│\033[0m %7s \033[1;36m│\033[0m\n",
                   title, pad, "",
                   continents[c].entities, pfx_fmt, ex_fmt, tot_fmt);
            sum_c_ent += continents[c].entities;
            sum_c_pfx += continents[c].pfx;
            sum_c_ex  += continents[c].exact;
        }
        printf("\033[1;36m├──────────────────────────────────┼──────────┼──────────┼───────────┼─────────┤\033[0m\n");
        char stot_pfx[16], stot_ex[16], stot_all[16];
        format_thousands(sum_c_pfx, stot_pfx, sizeof(stot_pfx));
        format_thousands(sum_c_ex, stot_ex, sizeof(stot_ex));
        format_thousands(sum_c_pfx + sum_c_ex, stot_all, sizeof(stot_all));
        printf("\033[1;36m│\033[0m \033[1mTotalt                            \033[0m\033[1;36m│\033[0m \033[1;32m  %4zu   \033[0m\033[1;36m│\033[0m \033[1;32m %7s \033[0m\033[1;36m│\033[0m \033[1;32m  %7s \033[0m\033[1;36m│\033[0m \033[1;32m%7s \033[0m\033[1;36m│\033[0m\n",
               sum_c_ent, stot_pfx, stot_ex, stot_all);
        printf("\033[1;36m└──────────────────────────────────┴──────────┴──────────┴───────────┴─────────┘\033[0m\n");
    }

    /* RITA TABELL 3: Topp 10 länder med flest regler */
    if (cstat_count > 0) {
        /* Sortera efter totalt antal regler (prefix + undantag) fallande */
        for (size_t i = 0; i < cstat_count - 1; ++i) {
            for (size_t j = i + 1; j < cstat_count; ++j) {
                int tot_i = cstats[i].pfx_count + cstats[i].exact_count;
                int tot_j = cstats[j].pfx_count + cstats[j].exact_count;
                if (tot_j > tot_i) {
                    CountryStat tmp = cstats[i];
                    cstats[i] = cstats[j];
                    cstats[j] = tmp;
                }
            }
        }

        printf("\n\033[1;36m┌──────────────────────────────────────────┬──────────┬───────────┬────────────┐\033[0m\n");
        printf("\033[1;36m│\033[0m \033[1mDXCC-entitet (Topp 10 flest regler)      \033[0m\033[1;36m│\033[0m \033[1m  Prefix \033[0m\033[1;36m│\033[0m \033[1m Undantag  \033[0m\033[1;36m│\033[0m \033[1m   Totalt  \033[0m\033[1;36m│\033[0m\n");
        printf("\033[1;36m├──────────────────────────────────────────┼──────────┼───────────┼────────────┤\033[0m\n");
        size_t limit = cstat_count < 10 ? cstat_count : 10;
        for (size_t i = 0; i < limit; ++i) {
            char spfx[16], sex[16], stot[16];
            format_thousands((size_t)cstats[i].pfx_count, spfx, sizeof(spfx));
            format_thousands((size_t)cstats[i].exact_count, sex, sizeof(sex));
            format_thousands((size_t)(cstats[i].pfx_count + cstats[i].exact_count), stot, sizeof(stot));

            int nw = utf8_display_width(cstats[i].name);
            int pad = 40 - nw;
            if (pad < 0) pad = 0;
            printf("\033[1;36m│\033[0m %s%*s \033[1;36m│\033[0m  %7s \033[1;36m│\033[0m   %7s \033[1;36m│\033[0m  %8s  \033[1;36m│\033[0m\n",
                   cstats[i].name, pad, "", spfx, sex, stot);
        }
        printf("\033[1;36m└──────────────────────────────────────────┴──────────┴───────────┴────────────┘\033[0m\n\n");
    }

    if (cstats) free(cstats);
    return 0;
}

/**
 * @brief Implementering av kommandot "lookup":
 * Slår upp en signal i dxcc.txt och visar information.
 */
static int cmd_lookup(const char *target_file, const char *call_in) {
    if (!call_in || !call_in[0]) {
        fprintf(stderr, "Användning: dxcc lookup <signal>\n");
        return 1;
    }

    const char *file_path = (target_file && target_file[0]) ? target_file : DEFAULT_DXCC_FILE;
    FILE *fp = fopen(file_path, "r");
    if (!fp) {
        fprintf(stderr, "Fel: Kunde inte öppna '%s': %s\n", file_path, strerror(errno));
        return 1;
    }

    char call[32];
    size_t clen = 0;
    for (const char *p = call_in; *p && clen < sizeof(call) - 1; ++p) {
        if (!isspace((unsigned char)*p)) call[clen++] = (char)toupper((unsigned char)*p);
    }
    call[clen] = '\0';

    RuleList rules;
    rule_list_init(&rules);

    char line[512];
    while (fgets(line, sizeof(line), fp)) {
        trim_newline(line);
        char *p = line;
        while (*p == ' ' || *p == '\t') p++;
        if (*p == '\0' || *p == '#') continue;
        char *sep = strchr(p, ';');
        if (!sep) continue;
        *sep = '\0';
        const char *k = p;
        const char *c = sep + 1;
        while (*c == ' ' || *c == '\t') c++;
        int is_exact = (k[0] == '=');
        if (is_exact) k++;
        rule_list_push(&rules, k, c, is_exact);
    }
    fclose(fp);

    /* 1. Sök exakt */
    const char *hit_country = NULL;
    const char *hit_rule = NULL;
    int hit_exact = 0;

    for (size_t i = 0; i < rules.count; ++i) {
        if (rules.items[i].is_exact && strcasecmp(rules.items[i].key, call) == 0) {
            hit_country = rules.items[i].country;
            hit_rule = rules.items[i].key;
            hit_exact = 1;
            break;
        }
    }

    /* 2. Sök prefix (längsta matchande prefix) */
    if (!hit_country) {
        size_t best_len = 0;
        for (size_t i = 0; i < rules.count; ++i) {
            if (!rules.items[i].is_exact) {
                size_t plen = strlen(rules.items[i].key);
                if (plen <= clen && strncmp(call, rules.items[i].key, plen) == 0) {
                    if (plen > best_len) {
                        best_len = plen;
                        hit_country = rules.items[i].country;
                        hit_rule = rules.items[i].key;
                    }
                }
            }
        }
    }

    if (hit_country) {
        printf("\n\033[1;36m┌────────────────────────────────────────────────────────┐\033[0m\n");
        printf("\033[1;36m│\033[0m \033[1;37m                   DXCC-UPPSLAG                        \033[0m\033[1;36m│\033[0m\n");
        printf("\033[1;36m├────────────────────────────────────────────────────────┤\033[0m\n");
        printf("\033[1;36m│\033[0m  Signal:        \033[1;32m%-38s\033[0m\033[1;36m│\033[0m\n", call);
        printf("\033[1;36m│\033[0m  DXCC-land:     \033[1;33m%-38s\033[0m\033[1;36m│\033[0m\n", hit_country);
        char type_str[64];
        snprintf(type_str, sizeof(type_str), "%s (%s)", hit_exact ? "Exakt signalundantag" : "Prefixmatch", hit_rule);
        printf("\033[1;36m│\033[0m  Matchning:     %-38s\033[1;36m│\033[0m\n", type_str);
        printf("\033[1;36m└────────────────────────────────────────────────────────┘\033[0m\n\n");
    } else {
        printf("Ingen DXCC-matchning hittades för signalen '%s'.\n", call);
    }

    rule_list_free(&rules);
    return hit_country ? 0 : 2;
}

/**
 * @brief Visar hjälptext för kommandot.
 */
static void show_help(const char *prog_name) {
    printf("\033[1;37mUEI-Logg DXCC-verktyg (dxcc) version 1.0\033[0m\n");
    printf("Hantering, uppdatering och statistikanalys av DXCC-databasen (%s)\n\n", DEFAULT_DXCC_FILE);
    printf("\033[1mAnvändning:\033[0m\n");
    printf("  %s help                         Visa denna hjälptext\n", prog_name);
    printf("  %s version                      Visa versionsinformation (version 1.0)\n", prog_name);
    printf("  %s update [källa / URL / fil]   Ladda ner och uppdatera %s från nätet\n", prog_name, DEFAULT_DXCC_FILE);
    printf("  %s stat [filnamn]               Visa utförlig statistik och tabeller för %s\n", prog_name, DEFAULT_DXCC_FILE);
    printf("  %s lookup <signal>              Slå upp DXCC-land för en anropssignal\n\n", prog_name);
    printf("\033[1mExempel:\033[0m\n");
    printf("  %s help                         Visa hjälptext och tillgängliga kommandon\n", prog_name);
    printf("  %s version                      Visa version (version 1.0)\n", prog_name);
    printf("  %s update                       Uppdatera med senaste data från Country Files (AD1C)\n", prog_name);
    printf("  %s stat                         Presentera tabeller över länder, prefix och zoner\n", prog_name);
    printf("  %s lookup SM5UEI                Visa DXCC-tillhörighet för SM5UEI\n\n", prog_name);
    printf("\033[1mStandardkälla för uppdatering:\033[0m\n");
    printf("  %s\n", DEFAULT_CTY_URL);
}

int main(int argc, char **argv) {
    curl_global_init(CURL_GLOBAL_DEFAULT);

    int ret = 0;
    if (argc < 2) {
        show_help(argv[0]);
        ret = 0;
    } else if (strcmp(argv[1], "help") == 0 || strcmp(argv[1], "--help") == 0 || strcmp(argv[1], "-h") == 0) {
        show_help(argv[0]);
        ret = 0;
    } else if (strcmp(argv[1], "version") == 0 || strcmp(argv[1], "--version") == 0 || strcmp(argv[1], "-v") == 0) {
        printf("dxcc version 1.0\n");
        ret = 0;
    } else if (strcmp(argv[1], "update") == 0 || strcmp(argv[1], "--update") == 0) {
        const char *src = (argc >= 3) ? argv[2] : NULL;
        ret = cmd_update(src);
    } else if (strcmp(argv[1], "stat") == 0 || strcmp(argv[1], "stats") == 0 || strcmp(argv[1], "--stat") == 0) {
        const char *f = (argc >= 3) ? argv[2] : NULL;
        ret = cmd_stat(f);
    } else if (strcmp(argv[1], "lookup") == 0) {
        if (argc < 3) {
            fprintf(stderr, "Ange signal att slå upp, t.ex.: %s lookup SM5UEI\n", argv[0]);
            ret = 1;
        } else {
            ret = cmd_lookup(DEFAULT_DXCC_FILE, argv[2]);
        }
    } else {
        fprintf(stderr, "Okänt kommando: '%s'\n\n", argv[1]);
        show_help(argv[0]);
        ret = 1;
    }

    curl_global_cleanup();
    return ret;
}
