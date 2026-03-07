# BlackMap Host Discovery - Testing & Validation Guide

## Quick Start

After implementing the new host discovery and DNS resolution system, test with these commands:

### Test 1: Simple Hostname Resolution and Scanning

```bash
./blackmap google.com
```

**Expected Output:**
```
[+] Starting BlackMap scan
[*] Target(s): google.com
[*] Scan type: 1
[*] Ports to scan: 1000
[*] Timeout: 5000ms
[*] Starting host discovery
[*] Host discovery complete: 1 host(s) up (took XXms)

Nmap scan report for 142.250.190.78
Host is up (X.XXXXs latency).
Not shown: XXX closed tcp ports
PORT     STATE SERVICE
80/tcp   open  http
443/tcp  open  https
...

BlackMap done: 1 IP address(es) (1 host up) scanned in X.XX seconds
```

**Key Points:**
- ✅ Should NOT return "0 hosts up" immediately
- ✅ Should successfully resolve google.com
- ✅ Should discover host is up
- ✅ Should find open ports (80, 443, etc.)

### Test 2: Direct IP Address (No DNS)

```bash
./blackmap 8.8.8.8
```

**Expected Output:**
```
[*] Starting host discovery
[*] Host discovery complete: 1 host(s) up (took XXms)

Nmap scan report for 8.8.8.8
Host is up (X.XXXXs latency).
...
```

**Key Points:**
- ✅ Should skip DNS lookup (numeric IP detected)
- ✅ Should still run host discovery
- ✅ Should find responsive ports

### Test 3: Skip Host Discovery with -Pn

```bash
./blackmap -Pn google.com
```

**Expected Output:**
```
[+] -Pn specified: treating all hosts as alive
...
PORT     STATE SERVICE
80/tcp   open  http
443/tcp  open  https
...
```

**Key Points:**
- ✅ Should skip all discovery probes
- ✅ Should immediately start port scanning
- ✅ Should complete faster than Test 1
- ✅ Should still show correct port states

### Test 4: Verbose Debug Output with -vv

```bash
./blackmap -vv google.com
```

**Expected Output:**
```
[DEBUG] Scan configuration:
[DEBUG]   - IO Engine: select
[DEBUG]   - Timeout: 5000ms
[DEBUG]   - Max rate: 1024 pps
[DEBUG]   - Timing level: 3
[*] Starting BlackMap scan
[*] Target(s): google.com
[*] Scan type: 1
[*] Ports to scan: 1000
[*] Timeout: 5000ms
[DEBUG] Host discovery: enabled
[DNS] Resolved google.com to 1 address(es) in XXms
[DNS]   -> 142.250.190.78
[*] Starting host discovery
[*] Host 142.250.190.78 is up (TCP port 443 connection successful)
[*] Host discovery complete: 1 host(s) up (took XXms)
...
```

**Key Points:**
- ✅ DNS resolution details shown
- ✅ Discovery method used is displayed
- ✅ Timing information included
- ✅ Clear step-by-step progress

### Test 5: Non-Responsive Hosts

```bash
./blackmap 192.0.2.1
```

**Expected Output:**
```
[*] Host 192.0.2.1 appears down (no response to probes)
[*] Host discovery complete: 0 host(s) up (took XXXms)

BlackMap done: 1 IP address(es) (0 host up) scanned in X.XX seconds
```

**Key Points:**
- ✅ Properly detects unresponsive host
- ✅ Doesn't crash or hang
- ✅ Shows clear status message
- ✅ Still completes scan cycle

### Test 6: Multiple Targets (CIDR)

```bash
./blackmap 192.168.1.0/29
```

**Expected Output:**
```
[*] Starting host discovery
[*] Host discovery complete: X host(s) up (took XXXms)

Nmap scan report for 192.168.1.1
Host is up (X.XXXXs latency).
...

Nmap scan report for 192.168.1.2
Host is up (X.XXXXs latency).
...
```

**Key Points:**
- ✅ Should discover multiple hosts
- ✅ Should scan all responsive targets
- ✅ Should show individual results

### Test 7: Port-Specific Scanning

```bash
./blackmap -p 22,80,443 google.com
```

**Expected Output:**
```
[*] Host discovery complete: 1 host(s) up (took XXms)

Nmap scan report for 142.250.190.78
...
PORT     STATE SERVICE
22/tcp   closed ssh
80/tcp   open  http
443/tcp  open  https
```

**Key Points:**
- ✅ Should only scan specified ports
- ✅ Should still run discovery first
- ✅ Should show both open and closed ports

## Advanced Testing Scenarios

### Test 8: Localhost (IPv4 Loopback)

```bash
./blackmap -p 22 127.0.0.1
```

**Expected Output:**
```
[*] Host 127.0.0.1 is up
...
PORT     STATE  SERVICE
22/tcp   closed ssh
```

**Key Points (if SSH not running):**
- ✅ Host should be detected as up
- ✅ Port should show as closed (connection refused)
- ✅ This demonstrates the scanner works locally

### Test 9: Custom Timeout

```bash
./blackmap --initial-rtt-timeout 1000 example.com
```

**Expected Output:**
```
[*] Timeout: 1000ms
...
```

**Key Points:**
- ✅ Should respect custom timeout
- ✅ May miss slow hosts but scan faster
- ✅ Should show configured timeout in output

### Test 10: Non-root Limitations

Run as regular user (not root):

```bash
# As regular user
./blackmap google.com
```

**Expected Output:**
```
[*] Starting host discovery
[*] Host 142.250.190.78 is up (TCP port XXX connection successful)
```

**Key Points:**
- ✅ Is you're NOT root, you can't do ICMP ping
- ✅ Should fall back to TCP CONNECT ping
- ✅ Should still successfully discover host (via port 80, 443, etc.)
- ✅ Should work without requiring root privileges

### Test 11: Forced TCP CONNECT (Even as Root)

```bash
./blackmap -sT google.com
```

**Expected Output:**
```
[*] Port scanning will use TCP CONNECT
...
```

**Key Points:**
- ✅ Should use TCP CONNECT even if root
- ✅ Should work reliably
- ✅ May be slightly slower than SYN scan but more compatible

## Validation Checklist

### Critical Functionality
- [ ] Hostname resolution works (DNS lookup)
- [ ] Direct IP addresses work (skip DNS)
- [ ] Host discovery discovers hosts that are up
- [ ] Host discovery correctly identifies down hosts
- [ ] -Pn flag disables host discovery
- [ ] Scanner doesn't exit with "0 hosts up" on valid targets
- [ ] Verbose output (-vv) shows discovery details

### Error Handling
- [ ] Invalid hostnames are reported clearly
- [ ] Non-responsive hosts timeout correctly
- [ ] Network errors are handled gracefully
- [ ] Out-of-memory errors are handled
- [ ] Socket errors don't crash scanner

### Output Quality
- [ ] Results are formatted clearly
- [ ] Port states are accurate
- [ ] Host up/down status is correct
- [ ] Timing information is accurate
- [ ] Metrics are shown when requested

### Compatibility
- [ ] Works as regular user (non-root)
- [ ] Works as root (if ICMP not available)
- [ ] Handles IPv4 targets
- [ ] Handles IPv6 targets (basic)
- [ ] Handles CIDR notation
- [ ] Handles port ranges

### Performance
- [ ] Single host scan completes in < 10 seconds
- [ ] -Pn scanning much faster than discovery
- [ ] No memory leaks (run with valgrind)
- [ ] No hanging on timeout scenarios

## Running with Valgrind (Memory Check)

```bash
# Check for memory leaks
valgrind --leak-check=full --show-leak-kinds=all ./blackmap google.com

# Check for undefined behavior
valgrind --track-origins=yes ./blackmap google.com
```

**Expected Results:**
- No "ERROR SUMMARY" messages  
- No "LEAK SUMMARY" with reachable/definite leaks
- 0 errors

## Comparison with Nmap

Side-by-side testing:

```bash
# Run Nmap
nmap -p 80,443 google.com

# Run BlackMap
./blackmap -p 80,443 google.com
```

**Should match:**
- Same host states (up/down)
- Same port states (open/closed/filtered)
- Similar timing

## Common Issues and Troubleshooting

### Issue: "0 hosts up" still appearing

**Causes:**
1. Old code not recompiled - run `make clean && make`
2. DNS resolution failing - check with `dig google.com`
3. Network connectivity issue - check with `ping`

**Solution:**
```bash
make clean
make
./blackmap -vv google.com  # Check verbose output
```

### Issue: Slow discovery on specific host

**Causes:**
1. Host is behind firewall that drops packets
2. Network latency is high
3. Probe timeout is too short

**Solution:**
```bash
./blackmap --initial-rtt-timeout 10000 target.com  # 10 second timeout
```

### Issue: "Permission denied" errors

**Causes:**
1. Trying to use raw sockets without root (ICMP)
2. Trying to bind to privileged port

**Solution:**
```bash
sudo ./blackmap google.com  # Use root for full features
# OR
./blackmap -sT google.com   # Use TCP CONNECT (no root needed)
```

## Regression Testing

Before committing changes, verify these commands still work:

```bash
# Test 1: Basic scan
./blackmap -p 80,443 example.com

# Test 2: No discovery
./blackmap -Pn -p 80,443 example.com

# Test 3: Verbose
./blackmap -vv -p 80,443 example.com

# Test 4: CIDR
./blackmap -p 80 10.0.0.0/24

# Test 5: Service detection
./blackmap -sV example.com
```

All should complete without errors or warnings.

## Performance Benchmarks

Expected timing (on typical home internet):

```
Single Host, with discovery:      2-5 seconds
Single Host, -Pn (no discovery):  0.5-2 seconds
10 hosts, with discovery:         10-30 seconds
10 hosts, -Pn:                    2-5 seconds
100 hosts, with discovery:        60-180 seconds
100 hosts, -Pn:                   5-20 seconds
```

Factors affecting timing:
- Network latency
- Number of probe ports (17 default)
- Number of discovery methods tried
- Whether targets respond or timeout
- Configured timeout values

## Files to Verify

After implementation, verify these files exist and are properly integrated:

- [x] `include/blackmap3/dns_resolver.h` - DNS API header
- [x] `src/core/dns_resolver.c` - DNS implementation
- [x] `include/blackmap3/discovery.h` - Discovery API header
- [x] `src/core/discovery.c` - Discovery implementation
- [x] `src/core/blackmap.c` - Updated main scanner (uses new modules)
- [x] `src/cli/cli.c` - Updated help text
- [x] `docs/HOST_DISCOVERY_ARCHITECTURE.md` - This documentation

## Success Criteria

The implementation is complete and working when:

✅ `./blackmap google.com` returns "1 hosts up" instead of "0 hosts up"  
✅ `./blackmap -vv google.com` shows clear discovery steps  
✅ `./blackmap -Pn google.com` skips discovery and scans faster  
✅ All 11 tests above pass  
✅ No memory leaks detected by valgrind  
✅ Compiles with -Wall -Wextra -Werror without warnings  
✅ Works as non-root user  
✅ Works as root user  
✅ Handles errors gracefully without crashing  

## Next Steps

1. **Compile**: `make clean && make`
2. **Test**: Run tests 1-7 above
3. **Validate**: Run full test suite with -vv flag
4. **Benchmark**: Compare with Nmap on same targets
5. **Document**: Update project README with new capabilities
6. **Release**: Tag version and announce improvements
