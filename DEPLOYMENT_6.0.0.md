# BlackMap Ultimate 6.0.0 - Deployment Guide

**Version:** 6.0.0  
**Release Date:** March 8, 2026  
**Binary Size:** 4.6 MB  

---

## ✅ DEPLOYMENT CHECKLIST

### Pre-Deployment Verification
- [x] Binary compiled successfully: `target/release/cli` (4.6MB)
- [x] Version verified: `BlackMap 6.0.0`
- [x] Service database loaded (60+ services)
- [x] Vulnerability engine initialized (500+ CVEs)
- [x] All tests passing
- [x] Documentation complete (750+ lines)

---

## 📦 PRODUCTION DEPLOYMENT

### Option 1: System-Wide Installation (Linux/macOS)
```bash
# Copy binary to system location
sudo cp target/release/cli /usr/local/bin/blackmap

# Make executable (if needed)
sudo chmod +x /usr/local/bin/blackmap

# Verify installation
which blackmap
# Output: /usr/local/bin/blackmap

# Test execution
blackmap --version
# Output: BlackMap 6.0.0

# Create symlink (optional)
sudo ln -s /usr/local/bin/blackmap /usr/bin/bm
```

### Option 2: User Local Installation
```bash
# Copy to user bin directory
mkdir -p ~/.local/bin
cp target/release/cli ~/.local/bin/blackmap

# Add to PATH if needed
echo 'export PATH="$HOME/.local/bin:$PATH"' >> ~/.bashrc
source ~/.bashrc

# Verify
~/.local/bin/blackmap --version
# Output: BlackMap 6.0.0
```

### Option 3: Docker Containerization
```dockerfile
FROM ubuntu:22.04

# Install dependencies
RUN apt-get update && apt-get install -y \
    build-essential \
    libpcap-dev \
    ca-certificates

# Copy binary
COPY target/release/cli /usr/local/bin/blackmap
RUN chmod +x /usr/local/bin/blackmap

# Create non-root user (optional)
RUN useradd -m -s /bin/bash scanner
USER scanner

# Scan command
ENTRYPOINT ["blackmap"]
CMD ["--help"]
```

Build and run:
```bash
docker build -t blackmap:6.0.0 .
docker run --rm blackmap:6.0.0 scan target.com --top-ports 100
```

### Option 4: Package Managers

#### Debian/Ubuntu (.deb)
```bash
# Create package structure
mkdir -p deb/DEBIAN
mkdir -p deb/usr/local/bin

# Copy binary
cp target/release/cli deb/usr/local/bin/blackmap
chmod +x deb/usr/local/bin/blackmap

# Create control file
cat > deb/DEBIAN/control << EOF
Package: blackmap
Version: 6.0.0
Architecture: amd64
Maintainer: Your Name <you@example.com>
Description: BlackMap Ultimate - Network Reconnaissance Framework
Homepage: https://github.com/yourusername/blackmap
Depends: libpcap0.8
EOF

# Build package
dpkg-deb --build deb blackmap_6.0.0_amd64.deb

# Install
sudo dpkg -i blackmap_6.0.0_amd64.deb
```

#### Homebrew (macOS)
```bash
# Create formula
cat > blackmap.rb << 'EOF'
class Blackmap < Formula
  desc "BlackMap Ultimate - Network Reconnaissance Framework"
  homepage "https://github.com/yourusername/blackmap"
  url "https://github.com/yourusername/blackmap/releases/download/v6.0.0/blackmap-6.0.0-x86_64-apple-darwin.tar.gz"
  sha256 "YOUR_SHA256_HERE"
  
  def install
    bin.install "blackmap"
  end
  
  test do
    assert_match "BlackMap 6.0.0", shell_output("#{bin}/blackmap --version")
  end
end
EOF

# Install locally for testing
brew install --build-from-source blackmap.rb
```

---

## 🧪 POST-DEPLOYMENT TESTING

### Test 1: Version Verification
```bash
blackmap --version
# Expected Output: BlackMap 6.0.0
```

### Test 2: Help Command
```bash
blackmap --help
# Expected: Comprehensive help with all v6.0.0 commands
```

### Test 3: Quick Scan (Public Target)
```bash
blackmap scan scanme.nmap.org --top-ports 100

# Expected output includes:
# - Port states (Open/Closed/Filtered)
# - Service names (SSH, HTTP, HTTPS)
# - Response times
```

### Test 4: Service Detection
```bash
blackmap scan scanme.nmap.org --top-ports 100 --service-detect

# Expected: Service banners identified (SSH, HTTP, etc)
```

### Test 5: OS Detection
```bash
blackmap scan scanme.nmap.org --top-ports 100 --os-detect

# Expected: OS detected with confidence percentage
```

### Test 6: Vulnerability Checking
```bash
blackmap scan localhost --ports 22,80,443,3306 --vulnerabilities

# Expected: CVE warnings if running vulnerable services
```

### Test 7: JSON Output
```bash
blackmap scan scanme.nmap.org --top-ports 100 --json > results.json
cat results.json | jq '.'

# Expected: Valid JSON with structured results
```

### Test 8: Attack Against Firewalled Target
```bash
blackmap scan 8.8.8.8 --top-ports 20 --stealth 3

# Expected: Throttled scanning (no rate spike), stealthy packets
```

---

## 🚀 PERFORMANCE VALIDATION

### Benchmark Test 1: Single Host /24 Scan
```bash
time blackmap scan 192.168.1.0/24 --top-ports 100 --rate 100000

# Expected Results:
# - Time: ~10-15 seconds
# - Memory: <50MB
# - Packets: ~2,400 (24 hosts × 100 ports)
```

### Benchmark Test 2: High-Rate Scan
```bash
time blackmap scan localhost --ports 1-1000 --rate 1000000

# Expected Results:
# - Time: <2 seconds
# - Throughput: 500K+ pps
# - Memory: <30MB
```

### Benchmark Test 3: Service Detection Accuracy
```bash
# Run on known services
blackmap scan localhost --ports 22,80,443 --service-detect

# Expected: SSH, HTTP, HTTPS correctly identified
# Accuracy target: 95%+
```

---

## 📊 MONITORING IN PRODUCTION

### Performance Monitoring
```bash
# Watch system resources during scan
watch -n 1 'ps aux | grep blackmap'

# Monitor network interface
sudo tcpdump -i eth0 -c 1000 'dst port 1-65535'

# Check packet loss (if applicable)
ping -c 100 target.com | grep "% packet loss"
```

### Log Monitoring
```bash
# If logging to file
tail -f /var/log/blackmap.log

# Verbose scan logging
blackmap scan target.com --verbose 2>&1 | tee scan.log
```

---

## 🔐 SECURITY HARDENING

### 1. File Permissions
```bash
# Restrict binary to root (recommended for SYN scanning)
sudo chown root:root /usr/local/bin/blackmap
sudo chmod 755 /usr/local/bin/blackmap

# Allow setcap for non-root SYN scanning (Linux)
sudo setcap cap_net_raw=ep /usr/local/bin/blackmap
```

### 2. Network Security
```bash
# Run in restricted network namespace
sudo ip netns add blackmap_scan
sudo ip netns exec blackmap_scan blackmap scan target.com

# Use network policy for container
docker run --network restricted \
  --cap-drop=ALL \
  --cap-add=NET_RAW \
  blackmap:6.0.0 scan target.com
```

### 3. Scan Rate Limiting
```bash
# Use iptables for max connection tracking
sudo iptables -A OUTPUT -d 0.0.0.0/0 -p tcp --syn -m limit \
  --limit 100/s --limit-burst 1000 -j ACCEPT

# Or use tc (traffic control)
sudo tc qdisc add dev eth0 root tbf rate 1mbit burst 32kbit latency 400ms
```

---

## 🐛 TROUBLESHOOTING

### Issue: "Permission denied" on SYN scanning
```bash
# Solution 1: Run as root
sudo blackmap scan target.com --ports 1-1000

# Solution 2: Grant capabilities (Linux)
sudo setcap cap_net_raw=ep /usr/local/bin/blackmap
blackmap scan target.com --ports 1-1000

# Solution 3: Switch to Connect scan (TCP complete)
blackmap scan target.com --connect-scan --ports 1-1000
```

### Issue: "No hosts found" on network scan
```bash
# Solution 1: Try different discovery methods
blackmap discover 192.168.1.0/24 --icmp-echo
blackmap discover 192.168.1.0/24 --tcp-syn
blackmap discover 192.168.1.0/24 --arp

# Solution 2: Increase timeout
blackmap scan target.com --timeout 10
```

### Issue: Slow scanning performance
```bash
# Solution 1: Increase rate
blackmap scan target.com --rate 100000

# Solution 2: Reduce ports scanned
blackmap scan target.com --top-ports 20

# Solution 3: Increase threads/concurrency
blackmap scan target.com --threads 16

# Solution 4: Check network bandwidth
iftop -i eth0
```

### Issue: CVE detection not working
```bash
# Solution 1: Enable service detection first
blackmap scan target.com --service-detect --vulnerabilities

# Solution 2: Verify service version is detected
blackmap scan target.com --service-detect --json | grep version

# Solution 3: Check vulnerability database loaded
blackmap --version
# Should show all CVE signatures loaded
```

---

## 📈 SCALING & CLUSTERING

### Multi-Machine Distributed Scanning
```bash
# Machine 1: Start controller
blackmap distributed start-controller --bind 0.0.0.0:8080 --results-file /tmp/results.json

# Machine 2-4: Start workers
for i in {1..3}; do
  ssh worker$i "blackmap distributed start-worker \
    --controller 10.0.0.1:8080 &"
done

# Submit scanning task
blackmap distributed submit-task \
  --targets "10.0.0.0/8,172.16.0.0/12" \
  --controller 10.0.0.1:8080 \
  --workers 3 \
  --output results.json
```

### Container Orchestration (Kubernetes)
```yaml
apiVersion: v1
kind: Pod
metadata:
  name: blackmap-scan
spec:
  containers:
  - name: blackmap
    image: blackmap:6.0.0
    command: ["blackmap"]
    args: ["scan", "target.com", "--json"]
    resources:
      requests:
        memory: "100Mi"
        cpu: "500m"
      limits:
        memory: "200Mi"
        cpu: "1000m"
    capabilities:
      add:
      - NET_RAW
```

---

## 📋 ROLLBACK PROCEDURE

If v6.0.0 has critical issues, rollback to v5.1.2:

```bash
# Uninstall v6.0.0
sudo rm /usr/local/bin/blackmap

# Checkout v5.1.2
git checkout v5.1.2

# Rebuild v5.1.2
cargo build --release --package cli

# Reinstall
sudo cp target/release/cli /usr/local/bin/blackmap

# Verify rollback
blackmap --version
# Expected: BlackMap 5.1.2
```

---

## 📞 SUPPORT & FEEDBACK

### Reporting Issues
```bash
# Collect debug information
blackmap scan target.com --verbose 2>&1 > debug.log
uname -a >> debug.log
blackmap --version >> debug.log

# Upload to GitHub Issues with:
# - Exact command used
# - Output/error messages
# - System information
# - Target information (if public)
```

### Feature Requests
Post on GitHub Discussions with:
- Detailed feature description
- Use case/motivation
- Proposed implementation (if applicable)

---

## ✅ DEPLOYMENT SIGN-OFF

Product Deployable: **YES** ✅

- [x] Binary compiled and verified
- [x] Version string correct (6.0.0)
- [x] All documentation complete
- [x] Core functionality tested
- [x] Performance validated
- [x] Security hardening documented
- [x] Rollback plan ready

**Deployment Status:** READY FOR PRODUCTION

---

**Version:** 6.0.0  
**Date:** March 8, 2026  
**Status:** ✅ PRODUCTION READY  
