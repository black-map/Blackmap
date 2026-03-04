#ifndef SCANNING_H
#define SCANNING_H

#include <stdint.h>
#include <stdbool.h>
#include "blackmap.h"

/* Scanner Interface */
typedef struct scanner {
    const char *name;
    scan_type_t type;
    int (*init)(void);
    int (*scan_host)(host_info_t *host);
    void (*cleanup)(void);
} scanner_t;

/* Scanner functions */
int scanner_tcp_connect(void);
int scanner_tcp_syn(void);
int scanner_tcp_fin(void);
int scanner_tcp_null(void);
int scanner_tcp_xmas(void);
int scanner_tcp_ack(void);
int scanner_tcp_window(void);
int scanner_tcp_maimon(void);
int scanner_tcp_idle(void);
int scanner_udp(void);
int scanner_sctp_init(void);
int scanner_sctp_cookie(void);
int scanner_ip_proto(void);
int scanner_ping(void);

/* Host discovery */
int discover_host(uint32_t ip_addr);
int ping_host(uint32_t ip_addr);

#endif /* SCANNING_H */
