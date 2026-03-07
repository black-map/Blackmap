#ifndef BLACKMAP3_DNS_H
#define BLACKMAP3_DNS_H

#include <stdint.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>

/**
 * Simple DNS resolver helper used by host-discovery and target parsing.
 * The implementation is intended to mimic professional scanners such as
 * Nmap: it is non-blocking from the caller's perspective (the call itself
 * may block, but it is invoked during setup), handles IPv4/IPv6, and
 * returns all addresses returned by the resolver.
 */

typedef struct {
    struct sockaddr_storage addr; /* sockaddr_in or sockaddr_in6 */
    socklen_t addr_len;
    char addr_str[INET6_ADDRSTRLEN]; /* textual representation (IPv4 or IPv6) */
} dns_addr_t;

/**
 * Resolve a single hostname (or numeric address) and allocate an array of
 * dns_addr_t structures in *out_addrs.  Caller must free the returned array
 * with free(*out_addrs) when finished.
 *
 * @param name opaque string supplied to getaddrinfo; may be numeric IP.
 * @param out_addrs pointer to array pointer that will be written on success
 * @param out_count number of entries returned (zero if resolution failed)
 * @return 0 on success, -1 on error (out_addrs may be NULL)
 */
int resolve_hostname(const char *name, dns_addr_t **out_addrs, size_t *out_count);

#endif /* BLACKMAP3_DNS_H */
