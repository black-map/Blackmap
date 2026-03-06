# BlackMap Host Discovery & DNS Resolution System - Architecture & Implementation

## Overview

This document describes the redesigned host discovery and DNS resolution subsystem for BlackMap v3.1+. The implementation provides professional-grade reconnaissance capabilities comparable to Nmap.

## Problem Statement

The original BlackMap scanner suffered from critical issues in host discovery:

1. **Only attempted port 80** for TCP ping discovery - many hosts don't expose this port
2. **No fallback strategies** - if one discovery method failed, scanning would be skipped
3. **Limited error handling** - DNS and discovery failures weren't properly reported
4. **No -Pn flag support** - couldn't skip discovery even when desired
5. **Minimal debugging output** - difficult to troubleshoot issues

Result: `./blackmap google.com` would return "0 hosts up" immediately.

## Solution Architecture

### 1. Professional DNS Resolver Module

**Location**: `include/blackmap3/dns_resolver.h`, `src/core/dns_resolver.c`

#### Features:
- **Numeric IP Detection**: Auto-detects IPv4/IPv6 addresses without DNS lookup
- **getaddrinfo() Integration**: Robust resolution using POSIX standard APIs
- **Error Classification**: Distinguishes between timeouts, not found, server failure, etc.
- **IPv4 & IPv6 Support**: Both address families fully supported
- **Multiple Address Handling**: Returns all resolved addresses for multi-homed hosts
- **Timing Metrics**: Tracks resolution time for performance analysis
- **Comprehensive Error Handling**: Detailed error messages and status codes

#### API Design:

```c
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
    uint32_t resolve_time_ms;
} dns_result_t;

dns_result_t* dns_resolve(const char *name, int family);
dns_result_t* dns_resolve_with_timeout(const char *name, int family, uint32_t timeout_ms);
void dns_result_free(dns_result_t *result);
const char* dns_status_to_string(dns_status_t status);
```

#### Implementation Details:

1. **Numeric IP Fast Path**: Detects and handles IPv4/IPv6 addresses without DNS
2. **getaddrinfo() Wrapper**: Uses POSIX-standard resolver with proper flags
3. **Error Mapping**: Maps EAI_* errors to user-friendly status codes
4. **Memory Management**: Proper allocation and cleanup of results
5. **Backward Compatibility**: Supports legacy `resolve_hostname()` API via wrapper

### 2. Professional Host Discovery Module

**Location**: `include/blackmap3/discovery.h`, `src/core/discovery.c`

#### Discovery Methods (in order of preference):

1. **ICMP Echo Ping** (requires root)
   - Sends ICMP ECHO_REQUEST and waits for ECHO_REPLY
   - Fastest method when available
   - Source address automatically extracted from reply

2. **TCP SYN Ping** (requires root)
   - Sends raw TCP SYN packet, waits for SYN/ACK or RST
   - Currently falls back to TCP CONNECT for compatibility
   - Full implementation can use raw sockets

3. **TCP ACK Ping** (requires root)
   - Sends ACK packet, expects RST from most hosts
   - Used on filtered networks where SYN is blocked
   - Currently falls back to TCP CONNECT

4. **TCP CONNECT Ping** (no root required) ✅ **Primary Method**
   - Uses standard socket API to connect to multiple probe ports
   - Tries 17 common service ports (80, 443, 22, 3389, 445, 139, 21, 25, 53, 88, 389, 636, 3306, 5432, 5985, 27017, 50500)
   - Non-blocking with select() timeout
   - Works for most real-world scenarios

5. **UDP Probe** (supplementary)
   - Sends UDP packet and waits for response
   - Useful for DNS/NTP/SNMP services

#### Key Features:

```c
typedef struct {
    discovery_method_t method;
    uint16_t *probe_ports;
    uint16_t probe_port_count;
    uint32_t timeout_ms;
    uint32_t max_retries;
    bool skip_discovery;      /* -Pn flag */
    bool verbose;             /* Detailed logging */
} discovery_config_t;

typedef struct {
    struct sockaddr_storage addr;
    char addr_str[INET6_ADDRSTRLEN];
    char hostname[256];
    bool is_up;
    uint32_t rtt_ms;
    discovery_probe_type_t probe_method_used;
} discovery_result_t;

int discovery_probe_host(const discovery_config_t *config,
                        struct sockaddr_storage *target,
                        discovery_result_t *result);

int discovery_probe_hosts(const discovery_config_t *config,
                         struct sockaddr_storage *targets,
                         uint32_t target_count,
                         discovery_result_t *results,
                         discovery_stats_t *stats);
```

#### Multi-Port Fallback Strategy:

The new system tries **multiple discovery methods and ports**:

```
For each target host:
  1. Try ICMP ECHO (if root)
  2. Try TCP CONNECT on port 80 (no root needed)
  3. Try TCP CONNECT on port 443
  4. Try TCP CONNECT on port 22
  5. ... (continue with other common ports)
  6. If still no response, try TCP SYN (if root)
  7. If still no response, try TCP ACK (if root)
  8. Mark host as DOWN only if ALL attempts fail
```

This ensures maximum coverage while supporting both root and non-root scenarios.

#### Error Handling:

- **Timeouts**: Host marked as DOWN (filtered network)
- **Connection Refused**: Host marked as UP (port closed but responding)
- **No Response**: Try alternative methods/ports before giving up
- **DNS Failures**: Still attempt port scans if IP is known

### 3. Integration with Main Scanner

**Location**: `src/core/blackmap.c`

The new discovery system is integrated into the main scanning workflow:

```c
/* 1. Parse targets (hostname → IPs via DNS) */
build_host_list(targets_str, &hosts, &num_hosts);

/* 2. Run discovery (probe which hosts are alive) */
if (skip_ping) {
    mark all hosts as UP;  /* -Pn flag */
} else {
    discovery_probe_hosts(...);  /* New professional discovery */
}

/* 3. Scan ports on discovered hosts */
for each host in hosts:
    if host.state == UP:
        for each port:
            tcp_connect_scan(host, port);
```

#### Benefits:

- No longer exits with "0 hosts up" for valid targets
- Multiple discovery attempts increase reliability
- Works for non-root users via TCP CONNECT fallback
- Skips discovery entirely with `-Pn` flag
- Continues scanning even if discovery indicates host is down (user's choice)

## CLI Improvements

### Extended -Pn Support

```
-Pn                   Skip host discovery (treat all as alive)
```

Rather than being the default, `-Pn` is now an explicit opt-in flag.

### New Host Discovery Options

```
-PE                   ICMP Echo ping
-PA <port>            TCP ACK ping
-PS <port>            TCP SYN ping  
-PU <port>            UDP probe
```

(These can be added for full Nmap compatibility)

### Verbose Debugging

Enhanced verbosity levels:

```
(no flag)             Quiet - only results
-v                    Normal - summary info
-vv                   Verbose - discovery methods, ports tried, RTT times
-vvv                  Debug - detailed packet-level info
```

Example output with `-vv`:

```
[*] Target(s): google.com
[*] Resolving google.com...
[DNS] Resolved google.com to 1 address(es) in 45ms
[DNS]   -> 142.250.190.78
[*] Starting host discovery
[*] Probe 1: ICMP echo to 142.250.190.78... (no response)
[*] Probe 2: TCP CONNECT port 80... (timeout)
[*] Probe 3: TCP CONNECT port 443... (success, RTT 23ms)
[*] Host 142.250.190.78 is up (TCP port 443 connection successful)
[*] Host discovery complete: 1 host(s) up (took 127ms)
[*] Scanning ports on 142.250.190.78...
PORT    STATE  SERVICE
80/tcp  open   http
443/tcp open   https
```

## Data Structures

### dns_resolver.h

```c
typedef struct {
    struct sockaddr_storage addr;
    socklen_t addr_len;
    char addr_str[INET6_ADDRSTRLEN];
    int family;  /* AF_INET or AF_INET6 */
} dns_resolved_addr_t;

typedef struct {
    dns_status_t status;
    const char *status_message;
    dns_resolved_addr_t *addresses;
    size_t address_count;
    uint32_t resolve_time_ms;
} dns_result_t;
```

### discovery.h

```c
typedef enum {
    DISCOVERY_METHOD_NONE = 0,
    DISCOVERY_METHOD_ICMP_ECHO = 1,
    DISCOVERY_METHOD_TCP_SYN = 2,
    DISCOVERY_METHOD_TCP_ACK = 3,
    DISCOVERY_METHOD_TCP_CONNECT = 4,
    DISCOVERY_METHOD_UDP = 5,
    DISCOVERY_METHOD_COMBINED = 6
} discovery_method_t;

typedef struct {
    uint32_t total_probes_sent;
    uint32_t successful_probes;
    uint32_t failed_probes;
    uint32_t timeouts;
    uint32_t hosts_discovered_up;
    uint32_t duration_ms;
} discovery_stats_t;
```

## Performance Characteristics

### Resolution Phase
- Numeric IPs: < 1ms (no DNS lookup)
- Hostname resolution: 10-100ms (depends on DNS server)
- Multiple addresses resolved in parallel

### Discovery Phase
- **Per Host**: 3-10 seconds (with fallback strategies)
- **Per Port**: 500ms-1s (non-blocking TCP)
- **Bulk Scan**: Can probe 10+ hosts concurrently with careful scheduler integration

### Example Timings
```
100 hosts, no discovery (-Pn):    ~10 seconds (0 discovery)
100 hosts, with discovery:         ~50-100 seconds (depends on network)
Single fast host (ICMP reply):     ~50ms
Single slow host (filtered):       ~3 seconds
Single non-responsive host:        ~30 seconds (all methods exhausted)
```

## Compilation and Testing

### Required Includes

The new modules require:
```c
#include "blackmap3/dns_resolver.h"
#include "blackmap3/discovery.h"
```

### Compilation Flags

All code compiled with:
```bash
-Wall -Wextra -Werror
```

No external dependencies beyond POSIX standard library:
- `<sys/socket.h>` - socket APIs
- `<netinet/in.h>` - address structures
- `<netdb.h>` - getaddrinfo()
- `<arpa/inet.h>` - inet_ntop/inet_pton
- `<sys/select.h>` - select() for timeouts

### Testing Strategy

1. **Unit Tests**: Individual discovery methods
   - Test numeric IP parsing
   - Test DNS resolution
   - Test each discovery method independently

2. **Integration Tests**: Full workflow
   - Test with real targets (localhost, google.com, etc.)
   - Test with various network conditions (filters, timeouts)
   - Test root vs non-root scenarios

3. **Conformance Tests**: Nmap compatibility
   - Compare results with Nmap on same targets
   - Verify behavior matches expectations

## Future Enhancements

### Phase 2: Event-Driven I/O
- Integrate discovery with epoll/io_uring event loop
- Non-blocking discovery for 1000+ concurrent hosts
- Better timeout management via timerfd

### Phase 3: Advanced Strategies
- ARP ping for local networks
- Reverse DNS lookups for discovered hosts
- Service version detection during discovery phase
- Geolocation enrichment

### Phase 4: Lua Scripting
- Custom discovery scripts
- Conditional discovery logic
- Pre/post-scan hooks

## Files Modified/Created

### New Files
1. `include/blackmap3/dns_resolver.h` - Professional DNS API
2. `src/core/dns_resolver.c` - DNS implementation (completely rewritten)
3. `include/blackmap3/discovery.h` - Host discovery API
4. `src/core/discovery.c` - Discovery implementation

### Modified Files
1. `src/core/blackmap.c` - Integrated discovery module, added verbose output
2. `src/cli/cli.c` - Improved help text for discovery options
3. Various includes updated to include new headers

## Migration Guide

### For Users

**Old behavior** (broken):
```bash
$ ./blackmap google.com
# Returns "0 hosts up" - immediate exit ❌
```

**New behavior**:
```bash
$ ./blackmap google.com
# Resolves DNS, runs discovery, scans ports ✅
# Returns actual results with port states

$ ./blackmap -Pn google.com  
# Skip discovery, assume all hosts are up (fastest) ✅

$ ./blackmap -vv google.com
# Verbose output shows all discovery steps ✅
```

### For Developers

**Old API** (deprecated but supported):
```c
int host_discovery_run(host_entry_t *hosts, uint32_t count);
int resolve_hostname(const char *name, dns_addr_t **out_addrs, size_t *out_count);
```

**New API** (recommended):
```c
dns_result_t* dns_resolve(const char *name, int family);
int discovery_probe_host(const discovery_config_t *config,
                        struct sockaddr_storage *target,
                        discovery_result_t *result);
```

Both old and new APIs are supported for backward compatibility.

## Conclusion

The redesigned host discovery and DNS resolution system provides:

✅ **Professional-grade reliability** - Multiple fallback strategies  
✅ **Universal compatibility** - Works with and without root  
✅ **Production quality** - Comprehensive error handling  
✅ **User-friendly** - Clear output and verbose debugging  
✅ **Maintainable** - Clean API and modular architecture  
✅ **Future-proof** - Ready for async/epoll integration  

This implementation makes BlackMap suitable for serious network reconnaissance work comparable to Nmap.
