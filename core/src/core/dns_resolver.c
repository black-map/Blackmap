#include "blackmap.h"
#include "blackmap3/dns.h"
#include "blackmap3/dns_resolver.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <netdb.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>

/**
 * Legacy wrapper for backward compatibility
 */
int resolve_hostname(const char *name, dns_addr_t **out_addrs, size_t *out_count) {
    if (!name || !out_addrs || !out_count) return -1;
    *out_addrs = NULL;
    *out_count = 0;

    /* Use the new DNS resolver API */
    dns_result_t *result = dns_resolve(name, AF_UNSPEC);
    if (!result || result->status != DNS_STATUS_SUCCESS) {
        if (result) dns_result_free(result);
        return -1;
    }

    /* Convert dns_result_t to dns_addr_t format for backward compatibility */
    dns_addr_t *addrs = calloc(result->address_count, sizeof(dns_addr_t));
    if (!addrs) {
        dns_result_free(result);
        return -1;
    }

    for (size_t i = 0; i < result->address_count; i++) {
        addrs[i].addr = result->addresses[i].addr;
        addrs[i].addr_len = result->addresses[i].addr_len;
        strncpy(addrs[i].addr_str, result->addresses[i].addr_str, INET6_ADDRSTRLEN - 1);
    }

    *out_addrs = addrs;
    *out_count = result->address_count;
    dns_result_free(result);
    return 0;
}

/* Forward declarations */
static int dns_is_ipv4_address(const char *str);
static int dns_is_ipv6_address(const char *str);

const char* dns_status_to_string(dns_status_t status) {
    switch (status) {
        case DNS_STATUS_SUCCESS:         return "Success";
        case DNS_STATUS_NOT_FOUND:       return "Name or service not known";
        case DNS_STATUS_TIMEOUT:         return "Resolver timeout";
        case DNS_STATUS_SERVER_FAILURE:  return "DNS server failure";
        case DNS_STATUS_INVALID_NAME:    return "Invalid hostname";
        case DNS_STATUS_MEMORY_ERROR:    return "Out of memory";
        case DNS_STATUS_SYSTEM_ERROR:    return "System error";
        default:                         return "Unknown error";
    }
}

void dns_result_free(dns_result_t *result) {
    if (!result) return;
    if (result->addresses) {
        free(result->addresses);
    }
    free(result);
}

int dns_is_numeric_ip(const char *addr_str, int *family) {
    if (!addr_str) return 0;
    
    if (dns_is_ipv4_address(addr_str)) {
        if (family) *family = AF_INET;
        return 1;
    }
    
    if (dns_is_ipv6_address(addr_str)) {
        if (family) *family = AF_INET6;
        return 1;
    }
    
    return 0;
}

static int dns_is_ipv4_address(const char *str) {
    struct in_addr addr;
    return inet_pton(AF_INET, str, &addr) == 1;
}

static int dns_is_ipv6_address(const char *str) {
    struct in6_addr addr;
    return inet_pton(AF_INET6, str, &addr) == 1;
}

/**
 * Handle direct numeric IP addresses without DNS resolution
 */
static dns_result_t* dns_resolve_numeric_ip(const char *addr_str) {
    dns_result_t *result = calloc(1, sizeof(dns_result_t));
    if (!result) {
        return NULL;
    }
    
    int family;
    if (dns_is_ipv4_address(addr_str)) {
        struct in_addr addr;
        inet_pton(AF_INET, addr_str, &addr);
        
        result->addresses = calloc(1, sizeof(dns_resolved_addr_t));
        if (!result->addresses) {
            free(result);
            return NULL;
        }
        
        result->address_count = 1;
        result->addresses[0].family = AF_INET;
        result->addresses[0].addr_len = sizeof(struct sockaddr_in);
        
        struct sockaddr_in *sin = (struct sockaddr_in *)&result->addresses[0].addr;
        sin->sin_family = AF_INET;
        sin->sin_addr = addr;
        sin->sin_port = 0;
        
        strncpy(result->addresses[0].addr_str, addr_str, INET6_ADDRSTRLEN - 1);
        result->status = DNS_STATUS_SUCCESS;
        result->status_message = "IP address parsed (no lookup needed)";
        result->resolve_time_ms = 0;
        
        return result;
    } else if (dns_is_ipv6_address(addr_str)) {
        struct in6_addr addr6;
        inet_pton(AF_INET6, addr_str, &addr6);
        
        result->addresses = calloc(1, sizeof(dns_resolved_addr_t));
        if (!result->addresses) {
            free(result);
            return NULL;
        }
        
        result->address_count = 1;
        result->addresses[0].family = AF_INET6;
        result->addresses[0].addr_len = sizeof(struct sockaddr_in6);
        
        struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&result->addresses[0].addr;
        sin6->sin6_family = AF_INET6;
        sin6->sin6_addr = addr6;
        sin6->sin6_port = 0;
        
        strncpy(result->addresses[0].addr_str, addr_str, INET6_ADDRSTRLEN - 1);
        result->status = DNS_STATUS_SUCCESS;
        result->status_message = "IPv6 address parsed (no lookup needed)";
        result->resolve_time_ms = 0;
        
        return result;
    }
    
    /* Not a numeric IP */
    free(result);
    return NULL;
}

dns_result_t* dns_resolve(const char *name, int family) {
    return dns_resolve_with_timeout(name, family, 5000);  /* 5 second default timeout */
}

dns_result_t* dns_resolve_with_timeout(const char *name, int family, uint32_t timeout_ms) {
    if (!name || *name == '\0') {
        dns_result_t *result = calloc(1, sizeof(dns_result_t));
        if (result) {
            result->status = DNS_STATUS_INVALID_NAME;
            result->status_message = "Empty hostname";
        }
        return result;
    }
    
    struct timeval start_time, end_time;
    gettimeofday(&start_time, NULL);
    
    /* First check if it's a numeric IP - no DNS needed */
    dns_result_t *numeric_result = dns_resolve_numeric_ip(name);
    if (numeric_result) {
        /* If numeric IP matches requested family (or family is ANY), return it */
        if (family == AF_UNSPEC || numeric_result->addresses[0].family == family) {
            return numeric_result;
        }
        /* Family mismatch - return error */
        dns_result_free(numeric_result);
        dns_result_t *result = calloc(1, sizeof(dns_result_t));
        if (result) {
            result->status = DNS_STATUS_INVALID_NAME;
            result->status_message = "Address family mismatch";
        }
        return result;
    }
    
    /* Perform actual DNS resolution using getaddrinfo */
    struct addrinfo hints, *ai_res, *p;
    memset(&hints, 0, sizeof(hints));
    hints.ai_family = (family == AF_UNSPEC) ? AF_UNSPEC : family;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_flags = AI_ADDRCONFIG;
    
    int gai_err = getaddrinfo(name, NULL, &hints, &ai_res);
    
    gettimeofday(&end_time, NULL);
    uint32_t resolve_time_ms = (end_time.tv_sec - start_time.tv_sec) * 1000 +
                               (end_time.tv_usec - start_time.tv_usec) / 1000;
    
    dns_result_t *result = calloc(1, sizeof(dns_result_t));
    if (!result) {
        if (gai_err == 0) freeaddrinfo(ai_res);
        dns_result_t *err_result = calloc(1, sizeof(dns_result_t));
        if (err_result) {
            err_result->status = DNS_STATUS_MEMORY_ERROR;
            err_result->status_message = "Out of memory";
        }
        return err_result;
    }
    
    result->resolve_time_ms = resolve_time_ms;
    
    if (gai_err != 0) {
        /* Handle getaddrinfo errors */
        switch (gai_err) {
            case EAI_AGAIN:
                result->status = DNS_STATUS_TIMEOUT;
                result->status_message = "Temporary failure in name resolution";
                break;
            case EAI_NONAME:
                result->status = DNS_STATUS_NOT_FOUND;
                result->status_message = "Name or service not known";
                break;
            case EAI_FAIL:
                result->status = DNS_STATUS_SERVER_FAILURE;
                result->status_message = "DNS server failure";
                break;
            case EAI_MEMORY:
                result->status = DNS_STATUS_MEMORY_ERROR;
                result->status_message = "Out of memory";
                break;
            default:
                result->status = DNS_STATUS_SYSTEM_ERROR;
                result->status_message = gai_strerror(gai_err);
                break;
        }
        
        if (g_config && g_config->verbosity > 1) {
            fprintf(stderr, "[DNS] Resolution failed for %s: %s\n", 
                    name, result->status_message);
        }
        
        result->addresses = NULL;
        result->address_count = 0;
        return result;
    }
    
    /* Count valid addresses */
    size_t count = 0;
    for (p = ai_res; p != NULL; p = p->ai_next) {
        if ((p->ai_family == AF_INET || p->ai_family == AF_INET6) &&
            (family == AF_UNSPEC || p->ai_family == family)) {
            count++;
        }
    }
    
    if (count == 0) {
        freeaddrinfo(ai_res);
        result->status = DNS_STATUS_NOT_FOUND;
        result->status_message = "No addresses found matching requested family";
        result->addresses = NULL;
        result->address_count = 0;
        return result;
    }
    
    /* Allocate result addresses */
    result->addresses = calloc(count, sizeof(dns_resolved_addr_t));
    if (!result->addresses) {
        freeaddrinfo(ai_res);
        result->status = DNS_STATUS_MEMORY_ERROR;
        result->status_message = "Out of memory";
        result->address_count = 0;
        return result;
    }
    
    /* Copy addresses */
    size_t idx = 0;
    for (p = ai_res; p != NULL && idx < count; p = p->ai_next) {
        if (p->ai_family == AF_INET) {
            struct sockaddr_in *sin = (struct sockaddr_in *)p->ai_addr;
            result->addresses[idx].family = AF_INET;
            result->addresses[idx].addr_len = sizeof(struct sockaddr_in);
            memcpy(&result->addresses[idx].addr, sin, sizeof(struct sockaddr_in));
            inet_ntop(AF_INET, &sin->sin_addr, result->addresses[idx].addr_str, 
                     INET6_ADDRSTRLEN);
            idx++;
        } else if (p->ai_family == AF_INET6) {
            struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)p->ai_addr;
            result->addresses[idx].family = AF_INET6;
            result->addresses[idx].addr_len = sizeof(struct sockaddr_in6);
            memcpy(&result->addresses[idx].addr, sin6, sizeof(struct sockaddr_in6));
            inet_ntop(AF_INET6, &sin6->sin6_addr, result->addresses[idx].addr_str, 
                     INET6_ADDRSTRLEN);
            idx++;
        }
    }
    
    freeaddrinfo(ai_res);
    
    result->status = DNS_STATUS_SUCCESS;
    result->status_message = "Success";
    result->address_count = idx;
    
    if (g_config && g_config->verbosity > 1) {
        printf("[DNS] Resolved %s to %zu address(es) in %ums\n", 
               name, count, resolve_time_ms);
        for (size_t i = 0; i < result->address_count; i++) {
            printf("[DNS]   -> %s\n", result->addresses[i].addr_str);
        }
    }
    
    return result;
}
