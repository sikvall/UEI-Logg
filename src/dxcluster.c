/**
 * @file dxcluster.c
 * @brief Implementation av telnet-klient och spothantering för DXSpider (W3LPL).
 */

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include "dxcluster.h"
#include "util.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <unistd.h>
#include <pthread.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <fcntl.h>
#include <poll.h>
#include <errno.h>
#include <signal.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif

static pthread_mutex_t g_cluster_mutex = PTHREAD_MUTEX_INITIALIZER;
static pthread_t       g_cluster_thread;
static int             g_cluster_thread_started = 0;
static volatile int    g_cluster_running = 0;

static ClusterState    g_cluster_state = CLUSTER_DISCONNECTED;
static char            g_cluster_host[64] = DEFAULT_CLUSTER_HOST;
static int             g_cluster_port = DEFAULT_CLUSTER_PORT;
static char            g_my_call[32] = "";
static char            g_my_grid[16] = "";
static char            g_my_dxcc[64] = "";

static DxSpot          g_spots[MAX_CLUSTER_SPOTS];
static int             g_spot_count = 0;

static char            g_outgoing_cmd[256] = "";
static int             g_has_outgoing_cmd = 0;

int dxcluster_format_login_commands(const char *call, const char *grid, const char *dxcc, char *out_buf, size_t out_sz) {
    if (!out_buf || out_sz == 0) return 0;
    out_buf[0] = '\0';

    if (call && call[0] != '\0') {
        char clean_call[32];
        copy_str(clean_call, sizeof(clean_call), call);
        trim_str(clean_call);
        to_upper_str(clean_call);
        if (clean_call[0] != '\0') {
            snprintf(out_buf, out_sz, "%s\r\n", clean_call);
        }
    }

    if (grid && grid[0] != '\0') {
        char clean_grid[16];
        copy_str(clean_grid, sizeof(clean_grid), grid);
        trim_str(clean_grid);
        to_upper_str(clean_grid);
        if (clean_grid[0] != '\0') {
            char gcmd[64];
            snprintf(gcmd, sizeof(gcmd), "set/qra %s\r\n", clean_grid);
            strncat(out_buf, gcmd, out_sz - strlen(out_buf) - 1);
        }
    }

    if (dxcc && dxcc[0] != '\0') {
        char clean_dxcc[64];
        copy_str(clean_dxcc, sizeof(clean_dxcc), dxcc);
        trim_str(clean_dxcc);
        if (clean_dxcc[0] != '\0') {
            char dcmd[128];
            snprintf(dcmd, sizeof(dcmd), "set/dxcc %s\r\n", clean_dxcc);
            strncat(out_buf, dcmd, out_sz - strlen(out_buf) - 1);
        }
    }

    strncat(out_buf, "sh/dx 30\r\n", out_sz - strlen(out_buf) - 1);
    return (int)strlen(out_buf);
}

/* Jämförelsefunktion för att sortera spots i frekvensordning med lägsta frekvensen upptill */
static int compare_spots_freq_asc(const void *a, const void *b) {
    const DxSpot *sa = (const DxSpot *)a;
    const DxSpot *sb = (const DxSpot *)b;
    if (sa->freq_khz < sb->freq_khz) return -1;
    if (sa->freq_khz > sb->freq_khz) return 1;
    return strcmp(sa->call, sb->call);
}

void dxcluster_init(void) {
    signal(SIGPIPE, SIG_IGN);
    pthread_mutex_lock(&g_cluster_mutex);
    g_spot_count = 0;
    g_cluster_state = CLUSTER_DISCONNECTED;
    g_has_outgoing_cmd = 0;
    pthread_mutex_unlock(&g_cluster_mutex);
}

void dxcluster_clear_spots(void) {
    pthread_mutex_lock(&g_cluster_mutex);
    g_spot_count = 0;
    pthread_mutex_unlock(&g_cluster_mutex);
}

int dxcluster_is_connected(void) {
    pthread_mutex_lock(&g_cluster_mutex);
    int conn = (g_cluster_state == CLUSTER_CONNECTED);
    pthread_mutex_unlock(&g_cluster_mutex);
    return conn;
}

void dxcluster_get_status(char *buf, size_t sz) {
    if (!buf || sz == 0) return;
    pthread_mutex_lock(&g_cluster_mutex);
    switch (g_cluster_state) {
        case CLUSTER_DISCONNECTED:
            copy_str(buf, sz, "Ej ansluten");
            break;
        case CLUSTER_CONNECTING:
            snprintf(buf, sz, "Ansluter till %s:%d...", g_cluster_host, g_cluster_port);
            break;
        case CLUSTER_LOGGING_IN:
            snprintf(buf, sz, "Loggar in som %s...", g_my_call[0] ? g_my_call : "NOCALL");
            break;
        case CLUSTER_CONNECTED:
            snprintf(buf, sz, "Ansluten (%s)", g_cluster_host);
            break;
        case CLUSTER_ERROR:
            copy_str(buf, sz, "Anslutningsfel");
            break;
    }
    pthread_mutex_unlock(&g_cluster_mutex);
}

void dxcluster_add_spot_direct(const char *call, double freq_khz, const char *spotter, const char *comment, const char *time_str) {
    if (!call || call[0] == '\0' || freq_khz <= 0.0) return;

    pthread_mutex_lock(&g_cluster_mutex);

    char clean_call[32];
    copy_str(clean_call, sizeof(clean_call), call);
    trim_str(clean_call);
    to_upper_str(clean_call);

    /* Kontrollera om signalen redan finns i listan: uppdatera i så fall */
    int existing_idx = -1;
    for (int i = 0; i < g_spot_count; ++i) {
        if (strcmp(g_spots[i].call, clean_call) == 0) {
            existing_idx = i;
            break;
        }
    }

    int target_idx = existing_idx;
    if (target_idx < 0) {
        if (g_spot_count < MAX_CLUSTER_SPOTS) {
            target_idx = g_spot_count++;
        } else {
            /* Ersätt den äldsta mottagna posten */
            int oldest_idx = 0;
            time_t oldest_time = g_spots[0].received_at;
            for (int i = 1; i < g_spot_count; ++i) {
                if (g_spots[i].received_at < oldest_time) {
                    oldest_time = g_spots[i].received_at;
                    oldest_idx = i;
                }
            }
            target_idx = oldest_idx;
        }
    }

    DxSpot *s = &g_spots[target_idx];
    copy_str(s->call, sizeof(s->call), clean_call);
    s->freq_khz = freq_khz;

    const char *guessed_band = guess_band_from_mhz(freq_khz / 1000.0);
    if (guessed_band && guessed_band[0] != '\0') {
        copy_str(s->band, sizeof(s->band), guessed_band);
    } else {
        snprintf(s->band, sizeof(s->band), "%.0f kHz", freq_khz);
    }

    copy_str(s->spotter, sizeof(s->spotter), spotter ? spotter : "");
    copy_str(s->comment, sizeof(s->comment), comment ? comment : "");
    copy_str(s->time_str, sizeof(s->time_str), time_str ? time_str : "");
    s->received_at = time(NULL);

    pthread_mutex_unlock(&g_cluster_mutex);
}

int dxcluster_get_spots(DxSpot *out_spots, int max_out, const char *band_filter, int *out_count) {
    if (!out_spots || max_out <= 0 || !out_count) return -1;

    pthread_mutex_lock(&g_cluster_mutex);

    int filter_band_idx = -1;
    if (band_filter && band_filter[0] != '\0') {
        filter_band_idx = find_band_index(band_filter);
    }

    int match_count = 0;
    for (int i = 0; i < g_spot_count && match_count < max_out; ++i) {
        if (filter_band_idx >= 0) {
            int spot_band_idx = find_band_index(g_spots[i].band);
            if (spot_band_idx != filter_band_idx) {
                continue;
            }
        }
        out_spots[match_count++] = g_spots[i];
    }

    pthread_mutex_unlock(&g_cluster_mutex);

    /* Sortera i frekvensordning: lägsta frekvensen upptill */
    if (match_count > 1) {
        qsort(out_spots, (size_t)match_count, sizeof(DxSpot), compare_spots_freq_asc);
    }

    *out_count = match_count;
    return 0;
}

int dxcluster_send_spot(double freq_khz, const char *call, const char *comment) {
    if (!call || call[0] == '\0' || freq_khz <= 0.0) return -1;

    char clean_call[32];
    copy_str(clean_call, sizeof(clean_call), call);
    trim_str(clean_call);
    to_upper_str(clean_call);

    /* Sanera så anropssignalen inte innehåller blanksteg eller radbrytningar */
    char *sp_p = strpbrk(clean_call, " \t\r\n");
    if (sp_p) *sp_p = '\0';
    if (clean_call[0] == '\0') return -1;

    char clean_comm[64] = "";
    if (comment && comment[0] != '\0') {
        copy_str(clean_comm, sizeof(clean_comm), comment);
        trim_str(clean_comm);
        for (char *cp = clean_comm; *cp; ++cp) {
            if (*cp == '\r' || *cp == '\n') *cp = ' ';
        }
    }
    if (clean_comm[0] == '\0') {
        copy_str(clean_comm, sizeof(clean_comm), "UEI-Logg");
    }

    pthread_mutex_lock(&g_cluster_mutex);
    if (g_cluster_state != CLUSTER_CONNECTED) {
        pthread_mutex_unlock(&g_cluster_mutex);
        return -1;
    }

    snprintf(g_outgoing_cmd, sizeof(g_outgoing_cmd), "dx %.1f %s %s\r\n", freq_khz, clean_call, clean_comm);
    g_has_outgoing_cmd = 1;
    pthread_mutex_unlock(&g_cluster_mutex);

    /* Spegla spotten lokalt direkt */
    char t_buf[16] = "";
    time_t now = time(NULL);
    struct tm tm_utc;
    if (gmtime_r(&now, &tm_utc)) {
        snprintf(t_buf, sizeof(t_buf), "%02d%02dZ", tm_utc.tm_hour, tm_utc.tm_min);
    }
    dxcluster_add_spot_direct(clean_call, freq_khz, g_my_call[0] ? g_my_call : "ME", clean_comm, t_buf);

    return 0;
}

int dxcluster_parse_line(const char *line, DxSpot *spot) {
    if (!line || !spot) return 0;

    memset(spot, 0, sizeof(*spot));

    const char *p = line;
    while (*p && isspace((unsigned char)*p)) p++;
    if (!*p) return 0;

    /* Typ 1: Standard real-time spot: "DX de <spotter>: <freq> <dxcall> <comment> <time>Z" */
    if (strncasecmp(p, "DX de ", 6) == 0) {
        p += 6;
        while (*p && isspace((unsigned char)*p)) p++;

        const char *colon = strchr(p, ':');
        if (!colon) return 0;

        char spotter[32] = "";
        size_t sp_len = (size_t)(colon - p);
        if (sp_len >= sizeof(spotter)) sp_len = sizeof(spotter) - 1;
        memcpy(spotter, p, sp_len);
        spotter[sp_len] = '\0';
        trim_str(spotter);

        p = colon + 1;
        while (*p && isspace((unsigned char)*p)) p++;

        char *end_freq = NULL;
        double freq_val = strtod(p, &end_freq);
        if (freq_val <= 0.0 || !end_freq || end_freq == p) return 0;
        p = end_freq;

        while (*p && isspace((unsigned char)*p)) p++;
        if (!*p) return 0;

        char dx_call[32] = "";
        size_t c_idx = 0;
        while (*p && !isspace((unsigned char)*p) && c_idx < sizeof(dx_call) - 1) {
            dx_call[c_idx++] = (char)toupper((unsigned char)*p);
            p++;
        }
        dx_call[c_idx] = '\0';
        if (c_idx < 3) return 0;

        while (*p && isspace((unsigned char)*p)) p++;

        char rest[128];
        copy_str(rest, sizeof(rest), p);
        trim_str(rest);

        /* Sök efter UTC-tid (t.ex. "2145Z") i slutet */
        char time_str[16] = "";
        size_t rlen = strlen(rest);
        if (rlen >= 5) {
            for (int i = (int)rlen - 5; i >= 0; --i) {
                if (isdigit((unsigned char)rest[i]) &&
                    isdigit((unsigned char)rest[i+1]) &&
                    isdigit((unsigned char)rest[i+2]) &&
                    isdigit((unsigned char)rest[i+3]) &&
                    (rest[i+4] == 'Z' || rest[i+4] == 'z')) {
                    snprintf(time_str, sizeof(time_str), "%c%c%c%cZ",
                             rest[i], rest[i+1], rest[i+2], rest[i+3]);
                    /* Klipp bort tiden och eventuell lokator efter den ur kommentaren */
                    rest[i] = '\0';
                    trim_str(rest);
                    break;
                }
            }
        }

        spot->freq_khz = freq_val;
        copy_str(spot->call, sizeof(spot->call), dx_call);
        copy_str(spot->spotter, sizeof(spot->spotter), spotter);
        copy_str(spot->comment, sizeof(spot->comment), rest);
        copy_str(spot->time_str, sizeof(spot->time_str), time_str);

        const char *b = guess_band_from_mhz(freq_val / 1000.0);
        if (b && b[0] != '\0') {
            copy_str(spot->band, sizeof(spot->band), b);
        } else {
            snprintf(spot->band, sizeof(spot->band), "%.0f kHz", freq_val);
        }
        spot->received_at = time(NULL);
        return 1;
    }

    /* Typ 2: sh/dx output (t.ex. "  14025.1  OH2BU        25-Nov-2026 2145Z CW CQ <W3LPL>") */
    if (isdigit((unsigned char)*p)) {
        char *end_freq = NULL;
        double freq_val = strtod(p, &end_freq);
        if (freq_val > 100.0 && end_freq && end_freq != p) {
            p = end_freq;
            while (*p && isspace((unsigned char)*p)) p++;

            char dx_call[32] = "";
            size_t c_idx = 0;
            while (*p && !isspace((unsigned char)*p) && c_idx < sizeof(dx_call) - 1) {
                dx_call[c_idx++] = (char)toupper((unsigned char)*p);
                p++;
            }
            dx_call[c_idx] = '\0';

            /* Validera att anropssignalen innehåller minst en siffra eller bokstav */
            int has_alpha = 0, has_digit = 0;
            for (size_t i = 0; i < c_idx; ++i) {
                if (isalpha((unsigned char)dx_call[i])) has_alpha = 1;
                if (isdigit((unsigned char)dx_call[i])) has_digit = 1;
            }

            if (c_idx >= 3 && has_alpha && has_digit) {
                while (*p && isspace((unsigned char)*p)) p++;

                char rest[128];
                copy_str(rest, sizeof(rest), p);
                trim_str(rest);

                char time_str[16] = "";
                size_t rlen = strlen(rest);
                if (rlen >= 5) {
                    for (int i = 0; i <= (int)rlen - 5; ++i) {
                        if (isdigit((unsigned char)rest[i]) &&
                            isdigit((unsigned char)rest[i+1]) &&
                            isdigit((unsigned char)rest[i+2]) &&
                            isdigit((unsigned char)rest[i+3]) &&
                            (rest[i+4] == 'Z' || rest[i+4] == 'z')) {
                            snprintf(time_str, sizeof(time_str), "%c%c%c%cZ",
                                     rest[i], rest[i+1], rest[i+2], rest[i+3]);
                            break;
                        }
                    }
                }

                spot->freq_khz = freq_val;
                copy_str(spot->call, sizeof(spot->call), dx_call);
                copy_str(spot->comment, sizeof(spot->comment), rest);
                copy_str(spot->time_str, sizeof(spot->time_str), time_str);

                const char *b = guess_band_from_mhz(freq_val / 1000.0);
                if (b && b[0] != '\0') {
                    copy_str(spot->band, sizeof(spot->band), b);
                } else {
                    snprintf(spot->band, sizeof(spot->band), "%.0f kHz", freq_val);
                }
                spot->received_at = time(NULL);
                return 1;
            }
        }
    }

    return 0;
}

/* Hantera Telnet IAC (0xFF) förhandlingar */
static int handle_telnet_iac(int sock, unsigned char *buf, int len, char *clean_out, int max_clean) {
    int clean_len = 0;
    int i = 0;

    while (i < len) {
        if (buf[i] == 255) { /* IAC */
            if (i + 1 < len) {
                unsigned char cmd = buf[i + 1];
                if (cmd == 251 || cmd == 252 || cmd == 253 || cmd == 254) {
                    /* IAC DO/DONT/WILL/WONT */
                    if (i + 2 < len) {
                        unsigned char opt = buf[i + 2];
                        unsigned char resp[3];
                        resp[0] = 255;
                        if (cmd == 253) {      /* DO -> WONT */
                            resp[1] = 252;
                            resp[2] = opt;
                            send(sock, resp, 3, 0);
                        } else if (cmd == 251) { /* WILL -> DONT */
                            resp[1] = 254;
                            resp[2] = opt;
                            send(sock, resp, 3, 0);
                        }
                        i += 3;
                        continue;
                    }
                }
                i += 2;
                continue;
            }
            i++;
            continue;
        }

        if (clean_len < max_clean - 1) {
            clean_out[clean_len++] = (char)buf[i];
        }
        i++;
    }
    clean_out[clean_len] = '\0';
    return clean_len;
}

/* Bakgrundstråd för telnet-anslutning mot W3LPL */
static void *dxcluster_worker_thread(void *arg) {
    (void)arg;

    while (g_cluster_running) {
        pthread_mutex_lock(&g_cluster_mutex);
        g_cluster_state = CLUSTER_CONNECTING;
        char host[64];
        copy_str(host, sizeof(host), g_cluster_host[0] ? g_cluster_host : DEFAULT_CLUSTER_HOST);
        int port = (g_cluster_port > 0) ? g_cluster_port : DEFAULT_CLUSTER_PORT;
        char call[32];
        copy_str(call, sizeof(call), g_my_call[0] ? g_my_call : "SM5UEI");
        pthread_mutex_unlock(&g_cluster_mutex);

        struct addrinfo hints, *res = NULL;
        memset(&hints, 0, sizeof(hints));
        hints.ai_family = AF_UNSPEC;
        hints.ai_socktype = SOCK_STREAM;

        char port_str[16];
        snprintf(port_str, sizeof(port_str), "%d", port);

        int rc = getaddrinfo(host, port_str, &hints, &res);
        if (rc != 0 && port != FALLBACK_CLUSTER_PORT) {
            /* Försök fallback-port */
            snprintf(port_str, sizeof(port_str), "%d", FALLBACK_CLUSTER_PORT);
            rc = getaddrinfo(host, port_str, &hints, &res);
        }

        if (rc != 0 || !res) {
            pthread_mutex_lock(&g_cluster_mutex);
            g_cluster_state = CLUSTER_ERROR;
            pthread_mutex_unlock(&g_cluster_mutex);

            /* Vänta 10 sekunder före nytt försök om tråden fortfarande körs */
            for (int w = 0; w < 20 && g_cluster_running; ++w) {
                usleep(500000);
            }
            continue;
        }

        int sock = -1;
        for (struct addrinfo *rp = res; rp != NULL; rp = rp->ai_next) {
            sock = socket(rp->ai_family, rp->ai_socktype, rp->ai_protocol);
            if (sock < 0) continue;

            /* Sätt icke-blockerande med timeout för anslutning */
            int flags = fcntl(sock, F_GETFL, 0);
            fcntl(sock, F_SETFL, flags | O_NONBLOCK);

            int cres = connect(sock, rp->ai_addr, rp->ai_addrlen);
            if (cres < 0 && errno == EINPROGRESS) {
                struct pollfd pfd;
                pfd.fd = sock;
                pfd.events = POLLOUT;
                int pr = poll(&pfd, 1, 5000);
                if (pr > 0 && (pfd.revents & POLLOUT)) {
                    int err = 0;
                    socklen_t elen = sizeof(err);
                    getsockopt(sock, SOL_SOCKET, SO_ERROR, &err, &elen);
                    if (err == 0) {
                        /* Framgångsrik anslutning */
                        fcntl(sock, F_SETFL, flags);
                        break;
                    }
                }
                close(sock);
                sock = -1;
            } else if (cres == 0) {
                fcntl(sock, F_SETFL, flags);
                break;
            } else {
                close(sock);
                sock = -1;
            }
        }
        freeaddrinfo(res);

        if (sock < 0) {
            pthread_mutex_lock(&g_cluster_mutex);
            g_cluster_state = CLUSTER_ERROR;
            pthread_mutex_unlock(&g_cluster_mutex);

            for (int w = 0; w < 50 && g_cluster_running; ++w) {
                usleep(100000);
            }
            continue;
        }

        int keepalive = 1;
        setsockopt(sock, SOL_SOCKET, SO_KEEPALIVE, &keepalive, sizeof(keepalive));

        pthread_mutex_lock(&g_cluster_mutex);
        g_cluster_state = CLUSTER_LOGGING_IN;
        pthread_mutex_unlock(&g_cluster_mutex);

        /* Huvudloop för ansluten session */
        char in_buf[1024];
        char line_acc[1024] = "";
        size_t acc_len = 0;
        int logged_in = 0;
        time_t connect_time = time(NULL);
        time_t last_recv_time = time(NULL);

        while (g_cluster_running) {
            /* Skicka utgående spot om en sådan köats (utanför mutex) */
            char cmd_to_send[256] = "";
            int has_cmd = 0;
            pthread_mutex_lock(&g_cluster_mutex);
            if (g_has_outgoing_cmd) {
                copy_str(cmd_to_send, sizeof(cmd_to_send), g_outgoing_cmd);
                has_cmd = 1;
                g_has_outgoing_cmd = 0;
            }
            pthread_mutex_unlock(&g_cluster_mutex);

            if (has_cmd && sock >= 0) {
                send(sock, cmd_to_send, strlen(cmd_to_send), MSG_DONTWAIT | MSG_NOSIGNAL);
            }

            struct pollfd pfd;
            pfd.fd = sock;
            pfd.events = POLLIN;
            int pr = poll(&pfd, 1, 250);

            if (!g_cluster_running) break;

            /* Om inloggningsprompt inte upptäckts inom 2 sekunder: skicka inloggningssekvensen */
            if (!logged_in && (time(NULL) - connect_time >= 2)) {
                char cmds[512];
                dxcluster_format_login_commands(call, g_my_grid, g_my_dxcc, cmds, sizeof(cmds));
                send(sock, cmds, strlen(cmds), MSG_DONTWAIT | MSG_NOSIGNAL);
                logged_in = 1;
                pthread_mutex_lock(&g_cluster_mutex);
                g_cluster_state = CLUSTER_CONNECTED;
                pthread_mutex_unlock(&g_cluster_mutex);
                last_recv_time = time(NULL);
            }

            if (pr > 0 && (pfd.revents & POLLIN)) {
                ssize_t n = recv(sock, in_buf, sizeof(in_buf) - 1, 0);
                if (n <= 0) {
                    /* Koppling bruten */
                    break;
                }
                last_recv_time = time(NULL);

                char clean_buf[1024];
                int c_len = handle_telnet_iac(sock, (unsigned char *)in_buf, (int)n, clean_buf, sizeof(clean_buf));

                /* Kontrollera inloggningsprompt */
                if (!logged_in) {
                    if (strcasestr(clean_buf, "login:") ||
                        strcasestr(clean_buf, "call:") ||
                        strcasestr(clean_buf, "callsign:") ||
                        strcasestr(clean_buf, "enter your call")) {
                        char cmds[512];
                        dxcluster_format_login_commands(call, g_my_grid, g_my_dxcc, cmds, sizeof(cmds));
                        send(sock, cmds, strlen(cmds), MSG_DONTWAIT | MSG_NOSIGNAL);
                        logged_in = 1;
                        pthread_mutex_lock(&g_cluster_mutex);
                        g_cluster_state = CLUSTER_CONNECTED;
                        pthread_mutex_unlock(&g_cluster_mutex);
                    }
                }

                /* Ackumulera och dela upp i rader */
                for (int ci = 0; ci < c_len; ++ci) {
                    char ch = clean_buf[ci];
                    if (ch == '\r') continue;
                    if (ch == '\n') {
                        line_acc[acc_len] = '\0';
                        if (acc_len > 0) {
                            DxSpot sp;
                            if (dxcluster_parse_line(line_acc, &sp)) {
                                dxcluster_add_spot_direct(sp.call, sp.freq_khz, sp.spotter, sp.comment, sp.time_str);
                            }
                        }
                        acc_len = 0;
                    } else {
                        if (acc_len < sizeof(line_acc) - 1) {
                            line_acc[acc_len++] = ch;
                        }
                    }
                }
            } else if (pr < 0 && errno != EINTR) {
                break;
            }

            /* Timeout-kontroll vid inaktivitet (2 minuter utan data från klustret) */
            if (logged_in && (time(NULL) - last_recv_time > 120)) {
                break;
            }
        }

        send(sock, "bye\r\n", 5, 0);
        close(sock);

        pthread_mutex_lock(&g_cluster_mutex);
        g_cluster_state = CLUSTER_DISCONNECTED;
        pthread_mutex_unlock(&g_cluster_mutex);

        if (g_cluster_running) {
            for (int w = 0; w < 50 && g_cluster_running; ++w) {
                usleep(100000);
            }
        }
    }

    return NULL;
}

void dxcluster_start(const char *host, int port, const char *my_call, const char *my_grid, const char *my_dxcc) {
    pthread_mutex_lock(&g_cluster_mutex);
    if (host && host[0] != '\0') {
        copy_str(g_cluster_host, sizeof(g_cluster_host), host);
    } else {
        copy_str(g_cluster_host, sizeof(g_cluster_host), DEFAULT_CLUSTER_HOST);
    }
    g_cluster_port = (port > 0) ? port : DEFAULT_CLUSTER_PORT;
    if (my_call && my_call[0] != '\0') {
        copy_str(g_my_call, sizeof(g_my_call), my_call);
    }
    if (my_grid && my_grid[0] != '\0') {
        copy_str(g_my_grid, sizeof(g_my_grid), my_grid);
    } else {
        g_my_grid[0] = '\0';
    }
    if (my_dxcc && my_dxcc[0] != '\0') {
        copy_str(g_my_dxcc, sizeof(g_my_dxcc), my_dxcc);
    } else {
        g_my_dxcc[0] = '\0';
    }

    if (g_cluster_thread_started) {
        pthread_mutex_unlock(&g_cluster_mutex);
        return;
    }

    g_cluster_running = 1;
    g_cluster_thread_started = 1;
    pthread_mutex_unlock(&g_cluster_mutex);

    pthread_create(&g_cluster_thread, NULL, dxcluster_worker_thread, NULL);
}

void dxcluster_stop(void) {
    pthread_mutex_lock(&g_cluster_mutex);
    if (!g_cluster_thread_started) {
        pthread_mutex_unlock(&g_cluster_mutex);
        return;
    }
    g_cluster_running = 0;
    pthread_mutex_unlock(&g_cluster_mutex);

    pthread_join(g_cluster_thread, NULL);

    pthread_mutex_lock(&g_cluster_mutex);
    g_cluster_thread_started = 0;
    g_cluster_state = CLUSTER_DISCONNECTED;
    pthread_mutex_unlock(&g_cluster_mutex);
}
