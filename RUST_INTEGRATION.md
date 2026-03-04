# Integración de Módulo Rust en BlackMap

## Estructura de Carpetas

```
blackmap/
├── rust/
│   ├── Cargo.toml
│   └── src/
│       └── lib.rs
├── include/
│   └── blackmap_rust.h
├── src/
│   └── service/
│       └── service.c (modificado)
└── Makefile (modificado)
```

## Compilación

### 1. Instalar Rust (si no está instalado)
```bash
curl --proto '=https' --tlsv1.2 -sSf https://sh.rustup.rs | sh
source $HOME/.cargo/env
```

### 2. Compilar el módulo Rust
```bash
cd rust
cargo build --release
```
Esto genera `target/release/libblackmap_rust.so`

### 3. Compilar BlackMap con enlazado a Rust
```bash
cd ..
make
```
El Makefile automáticamente compila Rust primero y enlaza con `-L./rust/target/release -lblackmap_rust -ldl`

## Uso

El módulo Rust se integra en `detect_service()` en `src/service/service.c`. Después de leer el banner, llama a `blackmap_analyze_banner()` que retorna un JSON con el análisis avanzado.

Ejemplo de salida:
```
[*] Rust analysis: {"service":"HTTP","version":"1.1","banner":"HTTP/1.1 200 OK","confidence":90}
```

## API FFI

- `const char* blackmap_analyze_banner(const char* input)`: Analiza el banner y retorna JSON.
- `void blackmap_free_string(char* s)`: Libera la memoria del string retornado.

## Extensión Futura

El módulo Rust está diseñado para ser extensible:
- Agregar más regex en `lib.rs`
- Cargar firmas desde archivo JSON
- Soporte para plugins con `libloading` crate

## Notas

- Asegurarse de que `libblackmap_rust.so` esté en el LD_LIBRARY_PATH o en el directorio del ejecutable.
- Para distribución, considerar compilar como estático si es necesario.