# BlackMap 4.0 Architecture

BlackMap 4.0 represents a complete paradigm shift from its C-only origins (v3.x) to a high-concurrency, asynchronous Rust framework, while retaining the raw packet manipulation capabilities of C for tasks requiring root privileges.

## Core Design Philosophy

1. **Rust for Logic & Concurrency**: The top-level orchestrator, async scheduler, parsing, plugin management, and reporting are handled by Rust and the `tokio` runtime.
2. **C for Raw Interfaces**: Packet crafting (SYN, FIN, NULL, XMAS) and deep packet inspection logic are handled in C and bridged to Rust using standard FFI (Foreign Function Interface) tools like `bindgen` and `cc`.
3. **Event-driven & Non-blocking**: Absolute asynchronous design. Even the DNS resolver avoids thread-blocking behaviors using `trust-dns`.
4. **Stealth & Evasion**: Stealth capabilities exist at the core of the engine, not as an afterthought. Timing profiles dynamic adjust jitter, fragmentation, and packet rates.

## System Components

### 1. `scanner.rs` (The Orchestrator)
The central component that schedules port sweeps. It leverages Tokio's `JoinSet` to handle `N` concurrent connections, regulated globally by `--threads`. 

### 2. `dns.rs` (The Resolver)
Replaces the unreliable libc native resolver with an async-native `trust-dns` resolver that allows caching, recursive queries, and parallel lookups without locking the OS threads.

### 3. C-Backend (The Engine `src/core/*`)
During compilation, `build.rs` compiles `discovery.c` and other packet-mangling mechanisms into static libraries that are linked dynamically to Rust code. Rust invokes `discovery_probe_host()` through the `src/ffi.rs` bridge.

### 4. `detection.rs` (Service & OS Engine)
Instead of hardcoding logic, BlackMap 4 parses large JSON datasets (`data/fingerprints.json`) containing signature definitions. It leverages lazy loading to cache compiled regular expressions into memory.

### 5. `distributed.rs`
Enabled by native Tokio TCP sockets. Exposes 2 endpoints logic: Master (Task allocator) and Worker (Probe executor).

## Building

The compilation relies on Cargo but also requires standard C build tools (`gcc`, `clang`) for the C-sources.

```bash
cd rust
cargo build --release
```
