/**
 * @file dxcluster.h
 * @brief Telnet-klient för anslutning till DX-Kluster (DXSpider / W3LPL) och spothantering.
 */

#ifndef DXCLUSTER_H
#define DXCLUSTER_H

#include <stddef.h>
#include <time.h>

#ifdef __cplusplus
extern "C" {
#endif

#define DEFAULT_CLUSTER_HOST "dxc.w3lpl.net"
#define DEFAULT_CLUSTER_PORT 7373
#define FALLBACK_CLUSTER_PORT 23

#define MAX_CLUSTER_SPOTS 256

/**
 * @brief En enskild DX-spot från klustret.
 */
typedef struct {
    char call[32];         /**< Anropssignal på den spottade stationen (t.ex. "SM5UEI") */
    double freq_khz;       /**< Frekvens i kHz (t.ex. 14025.1) */
    char band[16];         /**< Amatörband (t.ex. "20 m") */
    char spotter[32];      /**< Den som skickade spotten (t.ex. "W3LPL") */
    char comment[64];      /**< Kommentar / mode / rapport */
    char time_str[16];     /**< Tid i UTC (t.ex. "1205Z") */
    time_t received_at;    /**< Tidsstämpel för mottagning */
} DxSpot;

/**
 * @brief Anslutningsstatus för DX-Klustret.
 */
typedef enum {
    CLUSTER_DISCONNECTED = 0,
    CLUSTER_CONNECTING,
    CLUSTER_LOGGING_IN,
    CLUSTER_CONNECTED,
    CLUSTER_ERROR
} ClusterState;

/**
 * @brief Initierar datastrukturer och lås för klustret.
 */
void dxcluster_init(void);

/**
 * @brief Startar asynkron anslutningstråd mot angiven DXSpider-server.
 * @param host Värdnamn (default "dxc.w3lpl.net").
 * @param port Portnummer (default 7373, fallback 23).
 * @param my_call Egen anropssignal för automatisk login.
 * @param my_grid Egen lokator för att sätta på klustret med set/qra (valfri).
 * @param my_dxcc Eget DXCC-land för att sätta på klustret med set/dxcc (valfri).
 */
void dxcluster_start(const char *host, int port, const char *my_call, const char *my_grid, const char *my_dxcc);

/**
 * @brief Formaterar inloggningskommandon (login, set/qra, set/dxcc och sh/dx 30).
 * @return Antal tecken skrivna till out_buf.
 */
int dxcluster_format_login_commands(const char *call, const char *grid, const char *dxcc, char *out_buf, size_t out_sz);

/**
 * @brief Stoppar anslutningstråden och stänger nätverkskopplingen.
 */
void dxcluster_stop(void);

/**
 * @brief Returnerar 1 om ansluten och inloggad mot klustret, annars 0.
 */
int dxcluster_is_connected(void);

/**
 * @brief Hämtar aktuell statussträng för gränssnittet.
 */
void dxcluster_get_status(char *buf, size_t sz);

/**
 * @brief Skickar en spot till klustret via telnet: "dx <freq_khz> <call> <comment>".
 * @param freq_khz Frekvens i kHz.
 * @param call Anropssignal.
 * @param comment Kommentar (valfri).
 * @return 0 vid framgång, -1 vid fel eller om klustret inte är anslutet.
 */
int dxcluster_send_spot(double freq_khz, const char *call, const char *comment);

/**
 * @brief Hämtar aktiva spots, sorterade efter lägsta frekvens upptill.
 * @param out_spots Målarray för spots.
 * @param max_out Maximalt antal poster som får skrivas till out_spots.
 * @param band_filter Om angivet, returnera endast spots på detta band (t.ex. "20 m"). Om NULL visas alla.
 * @param out_count Antal returnerade spots.
 * @return 0 vid framgång.
 */
int dxcluster_get_spots(DxSpot *out_spots, int max_out, const char *band_filter, int *out_count);

/**
 * @brief Tolkar en rad mottagen från klustret.
 * @param line Rå textrad.
 * @param spot Målstruktur om raden var en DX-spot.
 * @return 1 om giltig spot extraherades, annars 0.
 */
int dxcluster_parse_line(const char *line, DxSpot *spot);

/**
 * @brief Lägger till eller uppdaterar en spot direkt i listan (för tester och lokal spegling).
 */
void dxcluster_add_spot_direct(const char *call, double freq_khz, const char *spotter, const char *comment, const char *time_str);

/**
 * @brief Rensar alla sparade spots i minnet.
 */
void dxcluster_clear_spots(void);

#ifdef __cplusplus
}
#endif

#endif /* DXCLUSTER_H */
