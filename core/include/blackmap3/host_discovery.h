#ifndef BLACKMAP3_HOST_DISCOVERY_H
#define BLACKMAP3_HOST_DISCOVERY_H

#include "blackmap.h"
#include "blackmap3/dns.h"
#include "blackmap3/discovery.h"
#include <stdint.h>
#include <sys/socket.h>

/* per-host information used during discovery and later scanning */
typedef struct {
    struct sockaddr_storage addr;   /* address to probe */
    socklen_t addr_len;
    bool is_ipv6;
    host_state_t state;            /* resulting state from discovery */
    char addr_str[INET6_ADDRSTRLEN];
    char hostname[256];            /* original string, if any */
} host_entry_t;

/**
 * Build a list of hosts from the comma-separated target string supplied on
 * the command line.  The resulting array is allocated with malloc; caller
 * must free it via host_list_free().  Supports:
 *   - single IP or hostname
 *   - CIDR/range notation (192.168.1.0/24, 10.0.0.1-50)
 *   - hostnames that resolve to multiple addresses
 *
 * @param targets comma-separated targets (as passed to cli)
 * @param out_hosts pointer to receive allocated array
 * @param out_count number of entries in array
 * @return 0 on success, -1 on failure
 */
int build_host_list(const char *targets, host_entry_t **out_hosts, uint32_t *out_count);

/**
 * Free an array produced by build_host_list()
 */
void host_list_free(host_entry_t *hosts, uint32_t count);

/**
 * Execute host discovery on the array of hosts.  Hosts with successful
 * probes will have their state set to HOST_UP; others will remain HOST_DOWN.
 * If g_config->skip_ping is set, all hosts are marked up immediately.
 * Returns number of hosts determined to be up.
 */
int host_discovery_run(host_entry_t *hosts, uint32_t count);

#endif /* BLACKMAP3_HOST_DISCOVERY_H */
