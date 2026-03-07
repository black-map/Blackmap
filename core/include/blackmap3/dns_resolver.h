#ifndef BLACKMAP3_DNS_RESOLVER_H
#define BLACKMAP3_DNS_RESOLVER_H

#include <stdint.h>
#include <stddef.h>
#include <sys/socket.h>
#include <netinet/in.h>

/**
 * PROFESSIONAL DNS RESOLVER MODULE
 * 
 * This module provides robust hostname resolution with:
 * - IPv4 and IPv6 support
 * - Multiple resolution strategies
 * - Comprehensive error handling
 * - Timeout management
 * - Debug/verbose output support
 */

typedef enum {
    DNS_FAMILY_IPV4 = AF_INET,
    DNS_FAMILY_IPV6 = AF_INET6,
    DNS_FAMILY_ANY = AF_UNSPEC
} dns_family_t;

typedef struct {
    struct sockaddr_storage addr;
    socklen_t addr_len;
    char addr_str[INET6_ADDRSTRLEN];
    int family;  /* AF_INET or AF_INET6 */
} dns_resolved_addr_t;

typedef enum {
    DNS_STATUS_SUCCESS = 0,
    DNS_STATUS_NOT_FOUND = 1,
    DNS_STATUS_TIMEOUT = 2,
    DNS_STATUS_SERVER_FAILURE = 3,
    DNS_STATUS_INVALID_NAME = 4,
    DNS_STATUS_MEMORY_ERROR = 5,
    DNS_STATUS_SYSTEM_ERROR = 6
} dns_status_t;

typedef struct {
    dns_status_t status;
    const char *status_message;
    dns_resolved_addr_t *addresses;
    size_t address_count;
    uint32_t resolve_time_ms;  /* Time taken to resolve in milliseconds */
} dns_result_t;

/**
 * Resolve a hostname or IP address string to one or more addresses.
 * 
 * Supports:
 * - IPv4 addresses (e.g., "192.168.1.1")
 * - IPv6 addresses (e.g., "::1")
 * - Hostnames (e.g., "google.com")
 * - Automatic numeric IP detection
 * 
 * @param name The hostname or IP address to resolve
 * @param family Preferred address family (AF_INET, AF_INET6, or AF_UNSPEC)
 * @return Dynamically allocated dns_result_t (must be freed with dns_result_free())
 */
dns_result_t* dns_resolve(const char *name, int family);

/**
 * Resolve with explicit timeout support
 * 
 * @param name The hostname or IP address to resolve
 * @param family Preferred address family
 * @param timeout_ms Timeout in milliseconds (0 = no timeout)
 * @return Dynamically allocated dns_result_t
 */
dns_result_t* dns_resolve_with_timeout(const char *name, int family, uint32_t timeout_ms);

/**
 * Free a dns_result_t structure allocated by dns_resolve()
 */
void dns_result_free(dns_result_t *result);

/**
 * Check if a string is a numeric IP address (IPv4 or IPv6)
 * 
 * @param addr_str The string to check
 * @param family Receives the address family (AF_INET or AF_INET6) if recognized
 * @return 1 if numeric IP, 0 otherwise
 */
int dns_is_numeric_ip(const char *addr_str, int *family);

/**
 * Get human-readable status message
 */
const char* dns_status_to_string(dns_status_t status);

#endif /* BLACKMAP3_DNS_RESOLVER_H */
