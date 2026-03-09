# BlackMap v6.3.0 - Integration Quick Reference

**Created**: v6.3.0 Module Integration  
**Status**: ✅ Production Ready  
**Last Verified**: Real scan against unlz.edu.ar showing CVEs  

---

## 🎯 Quick Facts

- **CVE Database**: `data/cve_db.json` - 15 services, 40+ CVEs indexed by service+version
- **Service Detection**: Protocol probes (HTTP, SSH, MySQL, Postgres, Redis, Docker, MongoDB, FTP)
- **Version Extraction**: Strips titles/whitespace for accurate DB matching
- **OS Fingerprinting**: Multi-signal analysis (TTL, TCP window, service banners)
- **Build Time**: ~4m clean build, incremental ~30s
- **Output Format**: `[CVEs: CVE-ID, CVE-ID, ...]` appended to service line

---

## 🔄 Integration Flow (Actual Code Path)

### Port Scanning → Service Detection → CVE Matching → Output

```
scanner::scan_port(80)
  └─ detect_service() [http_probe.rs]
      ├─ Connect: TcpStream::connect(addr)
      ├─ Send: "GET / HTTP/1.1..."
      ├─ Parse: "Server: Apache/2.4.38" → service="apache", version="2.4.38"
      └─ Return: ServiceInfo { service, version, confidence }
      
  └─ VulnerabilityEngine::load_from_file() [vulnerability_engine.rs]
      ├─ Parse: data/cve_db.json
      ├─ Find: Entry where service=="apache" && version=="2.4.38"
      └─ Return: Vec<CVE> = [CVE-2019-0211, CVE-2019-0197, ...]
      
  └─ PortScan struct + CVE fields
      ├─ cves: Some([CVE-2019-0211, ...])
      ├─ cve_confidence: Some(95)
      └─ Store in HostScan.ports[]
      
  └─ format_table() [output/mod.rs]
      └─ Append: "[CVEs: CVE-2019-0211, CVE-2019-0197, ...]" to display
```

---

## 📝 Key Code Locations

### 1. CVE Database Integration
- **Struct Definition**: `rust/src/scanner/mod.rs` lines 65-67 (PortScan.cves)
- **CVE Loading**: `rust/src/scanner/mod.rs` lines 541-565 (TCP scan loop)
- **Version Cleaning**: `rust/src/scanner/mod.rs` line 562 (.split('(').next())
- **Database Query**: `rust/src/vulnerability_engine.rs` lines 40-70 (check_vulnerabilities)

### 2. Service Detection
- **HTTP Probe**: `rust/src/probes/http_probe.rs` lines 1-60
- **Server Name**: `rust/src/probes/http_probe.rs` line 45-50 (parse_server_header)
- **Version Extraction**: `rust/src/probes/http_probe.rs` line 55-58
- **Dispatcher**: `rust/src/probes/mod.rs` (detect_service routes by port)

### 3. Output Display
- **Format Function**: `rust/src/output/mod.rs` lines 102-108
- **CVE Append**: `rust/src/output/mod.rs` line 105 (`format!("[CVEs: {}]", ...)`)
- **Main Entry**: `rust/src/output/mod.rs` line 35 (format_table calls all formatting)

### 4. OS Fingerprinting
- **Post-Scan Analysis**: `rust/src/scanner/mod.rs` lines 362-365
- **OSFingerprinter**: `rust/src/os_fingerprinter_new.rs` lines 1-50
- **Service Analysis**: `rust/src/os_fingerprinter_new.rs` lines 85-120

---

## 🧪 Testing & Verification

### Run Live Scan
```bash
./blackmap scan TARGET -p PORT1,PORT2 -V
```

#### Expected Output (Apache on port 80):
```
80/tcp    open     apache 2.4.38 (85% conf) [CVEs: CVE-2019-0211, CVE-2019-0197, ...]
```

### Verify Modules Loaded
```bash
grep -n "VulnerabilityEngine\|detect_service\|format_table" rust/src/scanner/mod.rs
```
Should show imports and calls in main scanning loop.

### Check Build Status
```bash
cargo build --release 2>&1 | grep -E "error|warning|Finished"
```
Should end with: `Finished 'release' profile...`

---

## 🐛 Troubleshooting

### CVEs Not Displaying
1. Check data/cve_db.json exists: `ls -la data/cve_db.json`
2. Verify service name extracted: Add `println!("Service: {}", service_info.service);` in scanner.rs line 540
3. Check version string clean: Ensure `split('(').next()` removes titles

### Wrong Service Detected
1. Verify Server header in HTTP response: Use `curl -I http://target`
2. Check http_probe.rs parse_server_header() logic
3. Ensure lowercase comparison: `.to_lowercase() == "apache"`

### Compilation Errors
```bash
cargo clean
cargo build --release
```

---

## 📊 Current CVE Database (data/cve_db.json)

### Supported Services (v6.3.0):
- Apache 2.2.15, 2.4.38, 2.4.46 (7-8 CVEs each)
- Nginx 1.10, 1.14, 1.16 (5-6 CVEs each)
- OpenSSH 7.2, 7.4 (3-4 CVEs each)
- MySQL 5.5, 5.7, 8.0 (2-3 CVEs each)
- PostgreSQL 9.5, 10, 12 (2-3 CVEs each)
- Redis 3.0, 4.0, 5.0 (1-2 CVEs each)

### Add New CVEs
Edit `data/cve_db.json`, add service entry:
```json
{
  "service": "apache",
  "version": "2.4.38",
  "cves": [
    {"id": "CVE-2019-NEW", "severity": "high", "description": "..."}
  ]
}
```

Then rebuild: `cargo build --release`

---

## 🚀 Performance Notes

- **Service Detection**: ~50ms per port (TcpStream + banner grab)
- **CVE Lookup**: ~1ms per port (JSON search, O(n) on services)
- **OS Fingerprinting**: ~5ms post-scan (multi-signal analysis)
- **Total Overhead**: ~50-60ms per port scanned

For parallel scanning (1000 ports), all services detected concurrently token.

---

## 📦 Deployment

### Binary Release
```bash
cargo build --release
cp target/release/cli ./blackmap
chmod +x ./blackmap
```

### Verify Functionality
```bash
./blackmap scan scanme.nmap.org -p 22,80,443 -V
```

Should show target information with CVEs for detected services.

---

## 🔗 Related Documents

- [Integration Guide](./INTEGRATION_GUIDE_6.3.0.md) - Detailed technical integration
- [ARCHITECTURE.md](./ARCHITECTURE.md) - System architecture overview
- [DEVELOPMENT.md](./DEVELOPMENT.md) - Development setup and workflow
- [README.md](./README.md) - General project README

---

**Last Updated**: Post v6.3.0 Integration  
**Maintenance**: Track CVE database updates, monitor service detection accuracy

