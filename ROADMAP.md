# BlackMap Roadmap

This roadmap indicates the planned features for upcoming minor and patch releases branching from the major 4.0 rewrite.

## v4.1 (Current Focus)
- [/] Async DNS Resolver Cache Persistence.
- [ ] Connect TCP SYN/ACK ping to Rust `ffi.rs`.
- [ ] Stabilize `--distributed` Worker/Master mode.

## v4.2
- [ ] **Stealth Module Configuration:** Implement timing `-T0-T5` mapping directly to the underlying `pcap` emitters to bypass standard IDSs. 
- [ ] Fragment routing and injection via `src/evasion`.

## v4.3
- [ ] Enable Rust Dynamic Plugin `.so` repository.
- [ ] Add 20 example plugins for custom vulnerability enumeration.
- [ ] Expand JSON fingerprint database to 5000+ signatures.

## v4.4
- [ ] Create interactive HTTP/REST API Dashboard (Web UI natively inside Master node).
- [ ] PostgreSQL integration for massive scan results storage.

## v5.0
- [ ] ML-based Host Fingerprinting using trained models on network signatures.
- [ ] Switch to deep AF_XDP bypass for theoretical 15M pps.
