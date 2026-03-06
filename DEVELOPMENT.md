# BlackMap Development Guide

Welcome! BlackMap is an open-source project combining the best of Rust's safety and concurrency with C's absolute raw system access.

## Repository Layout

* `/rust/` - The core application written in Rust (2021 Edition). Contains the CLI interface, async core, Master/Worker components, and Plugin system.
* `/src/` - The C engine components. These handle raw sockets, host discovery routines, SYN scanning, and packet fragmentation.
* `/include/` - standard C headers describing the FFI interface boundaries for `bindgen`.
* `/data/` - Static data like `fingerprints.json` used by the service detection engine.

## Setting Up Your Environment

You will need:
- Rust (1.75+)
- GCC / Clang
- `make` and `cmake` (optional for standalone C tests)

```bash
# Clone the repository
git clone https://github.com/Brian-Rojo/Blackmap
cd Blackmap/rust

# Fetch deps and compile bindings
cargo build
```

## Creating a Plugin

BlackMap 4.0 uses dynamic plugins loaded at runtime (`.so` / `.dll`).

1. Create a `cdylib` rust project:
```toml
[lib]
crate-type = ["cdylib"]
```
2. Implement the `blackmap::plugin::Plugin` trait.
3. Use the `export_plugin!(MyPlugin);` macro provided by BlackMap.
4. Run `cargo build --release` and load the `.so` using `blackmap --plugin target/release/libmyplugin.so`.

## Architectural Rules

- **No Panics in Rust**: Always use `Result<T, BlackMapError>` and propagate using `?`.
- **C Memory Management**: Any memory allocated in C must be freed explicitly through a C function exported and called via Rust's `Drop` trait.
- **Asynchronous Execution**: Do not use `std::thread::sleep` or blocking I/O calls. Everything goes through `tokio`.
