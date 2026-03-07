# BlackMap v3.2 - FFI Design & Performance Optimization Guide

**Document Type**: Technical Reference for Developers  
**Audience**: C and Rust developers working on FFI boundaries and optimizations  
**Version**: 3.2.0

---

## Table of Contents

1. [FFI Boundary Design](#ffi-boundary-design)
2. [Memory Safety Patterns](#memory-safety-patterns)
3. [Performance Optimization Techniques](#performance-optimization-techniques)
4. [Concurrency Model](#concurrency-model)
5. [Buffer Management](#buffer-management)
6. [Benchmarking Guide](#benchmarking-guide)

---

## FFI Boundary Design

### Design Principle

The FFI boundary between C (application layer) and Rust (analysis layer) is treated as a **security perimeter**:
- All C inputs are untrusted
- All Rust outputs are validated
- No `unsafe` Rust code in public APIs
- Reference counting for resource lifecycle

### Safe FFI Pattern #1: Input Validation & Conversion

**Problem**: C passes raw pointers with user-controlled sizes.

```rust
// UNSAFE: Direct pointer usage
#[no_mangle]
pub extern "C" fn unsafe_analyze(banner: *const u8, banner_len: u32) {
    let slice = unsafe { 
        std::slice::from_raw_parts(banner, banner_len as usize) 
    };
    // User might pass invalid pointer -> segfault
}
```

**Solution**: Safe wrapper with validation

```rust
#[no_mangle]
pub extern "C" fn safe_analyze(
    banner: *const u8,
    banner_len: u32,
) -> *mut service_fingerprint_c_t {
    // Validate inputs BEFORE any unsafe code
    if banner.is_null() || banner_len == 0 || banner_len > 65536 {
        return std::ptr::null_mut();  // Return error
    }

    // Check pointer alignment
    if banner as usize % std::mem::align_of::<u8>() != 0 {
        return std::ptr::null_mut();
    }

    // NOW safe to use unsafe code
    let banner_slice = unsafe {
        std::slice::from_raw_parts(banner, banner_len as usize)
    };

    // Process in safe Rust
    match analyze_banner_safe(banner_slice) {
        Ok(fp) => Box::into_raw(Box::new(fp)),
        Err(_) => std::ptr::null_mut(),
    }
}

// Helper (purely safe)
fn analyze_banner_safe(banner: &[u8]) -> Result<service_fingerprint_c_t, String> {
    // All safe Rust code here
    // No unsafe blocks
    Ok(/* constructed result */)
}
```

### Safe FFI Pattern #2: String Marshaling

**Problem**: Strings require careful marshaling across language boundary.

```rust
// UNSAFE: Incorrect null-termination
#[no_mangle]
pub extern "C" fn unsafe_extract_version(
    banner: *const u8,
    banner_len: u32,
) -> *const c_char {
    let banner_slice = unsafe {
        std::slice::from_raw_parts(banner, banner_len as usize)
    };

    let version_str = extract_version(banner_slice);
    
    // WRONG: String bytes aren't null-terminated
    version_str.as_ptr() as *const c_char
}
```

**Solution**: Use CString for safe null-termination

```rust
use std::ffi::CString;

#[no_mangle]
pub extern "C" fn safe_extract_version(
    banner: *const u8,
    banner_len: u32,
    out_len: *mut u32,  // OUTPUT: length written
) -> *mut c_char {
    // Validate
    if banner.is_null() || banner_len == 0 || out_len.is_null() {
        return std::ptr::null_mut();
    }

    // Safe conversion
    let banner_slice = unsafe {
        std::slice::from_raw_parts(banner, banner_len as usize)
    };

    let version_str = match extract_version_safe(banner_slice) {
        Some(v) => v,
        None => return std::ptr::null_mut(),
    };

    // Create C-compatible string
    match CString::new(version_str) {
        Ok(cstring) => {
            // Return raw pointer (C owns it now)
            // C MUST free with free() or analysis_free_string()
            unsafe {
                *out_len = cstring.len() as u32;
            }
            cstring.into_raw()
        }
        Err(_) => std::ptr::null_mut(),  // Invalid UTF-8
    }
}

// IMPORTANT: C side must free with:
// extern void analysis_free_string(char *ptr);
// void analysis_free_string(char *ptr) {
//     free(ptr);  // Allocated by CString::into_raw()
// }
```

### Safe FFI Pattern #3: Complex Output Structures

**Problem**: Returning complex Rust structures to C.

```rust
// Opaque pointer approach (recommended)

// In Rust (lib.rs)
pub struct ServiceFingerprint {
    service: String,
    product: String,
    version: Option<String>,
    // ... many fields
}

// Opaque handle for C side
pub struct ServiceAnalyzerHandle {
    fingerprints: Vec<ServiceFingerprint>,
}

#[no_mangle]
pub extern "C" fn analyzer_analyze_batch(
    banners: *const *const u8,
    banner_lens: *const u32,
    banner_count: u32,
) -> *mut ServiceAnalyzerHandle {
    // Validate arrays
    if banners.is_null() || banner_lens.is_null() || banner_count == 0 {
        return std::ptr::null_mut();
    }

    let mut fingerprints = Vec::new();

    unsafe {
        for i in 0..banner_count as usize {
            let banner = *banners.add(i);
            let banner_len = *banner_lens.add(i);

            if !banner.is_null() && banner_len > 0 && banner_len < 65536 {
                let slice = std::slice::from_raw_parts(banner, banner_len as usize);
                if let Some(fp) = analyze_safe(slice) {
                    fingerprints.push(fp);
                }
            }
        }
    }

    Box::into_raw(Box::new(ServiceAnalyzerHandle { fingerprints }))
}

// C can't access internals, must use query functions

#[no_mangle]
pub extern "C" fn analyzer_get_count(handle: *mut ServiceAnalyzerHandle) -> u32 {
    if handle.is_null() {
        return 0;
    }
    
    unsafe {
        (*handle).fingerprints.len() as u32
    }
}

#[no_mangle]
pub extern "C" fn analyzer_get_service_json(
    handle: *mut ServiceAnalyzerHandle,
    index: u32,
) -> *mut c_char {
    if handle.is_null() {
        return std::ptr::null_mut();
    }

    unsafe {
        let analyzer = &(*handle);
        if (index as usize) < analyzer.fingerprints.len() {
            let fp = &analyzer.fingerprints[index as usize];
            
            // Serialize to JSON
            if let Ok(json) = serde_json::to_string(fp) {
                if let Ok(cstring) = CString::new(json) {
                    return cstring.into_raw();
                }
            }
        }
    }

    std::ptr::null_mut()
}

#[no_mangle]
pub extern "C" fn analyzer_free(handle: *mut ServiceAnalyzerHandle) {
    if !handle.is_null() {
        unsafe {
            Box::from_raw(handle);  // Rust cleanup
        }
    }
}
```

**C Side Usage**:

```c
// C code
typedef void ServiceAnalyzerHandle;  // Opaque

ServiceAnalyzerHandle* analyzer = analyzer_analyze_batch(banners, lens, count);
uint32_t result_count = analyzer_get_count(analyzer);

for (uint32_t i = 0; i < result_count; i++) {
    char *json = analyzer_get_service_json(analyzer, i);
    printf("%s\n", json);
    analysis_free_string(json);  // Free individual results
}

analyzer_free(analyzer);  // Free analyzer
```

### Safe FFI Pattern #4: Callback Functions

**Problem**: C code needs to invoke Rust functions dynamically.

```rust
// Define callback signature
pub type PluginProbeCallback = extern "C" fn(
    *const u8,  // banner
    u32,        // banner_len
    *mut u8,    // output buffer (pre-allocated by C)
    u32,        // output buffer size
) -> i32;      // return status

// Plugin loads callback
#[no_mangle]
pub extern "C" fn register_probe_callback(
    probe_name: *const c_char,
    callback: PluginProbeCallback,
) -> i32 {
    if probe_name.is_null() || callback == std::ptr::null() {
        return -1;
    }

    let name = unsafe {
        match CStr::from_ptr(probe_name).to_str() {
            Ok(s) => s.to_string(),
            Err(_) => return -1,  // Invalid UTF-8
        }
    };

    // Store callback safely
    PROBE_CALLBACKS.with(|cb| {
        cb.borrow_mut().insert(name, callback);
    });

    0
}

// Thread-local storage for callbacks
thread_local! {
    static PROBE_CALLBACKS: std::cell::RefCell<
        std::collections::HashMap<String, PluginProbeCallback>
    > = std::cell::RefCell::new(std::collections::HashMap::new());
}

// Invoke callback safely
fn invoke_probe(name: &str, banner: &[u8]) -> Result<Vec<u8>, String> {
    PROBE_CALLBACKS.with(|cb| {
        let callbacks = cb.borrow();
        
        match callbacks.get(name) {
            None => Err(format!("Unknown probe: {}", name)),
            Some(callback) => {
                // Pre-allocate output buffer
                let mut output = vec![0u8; 4096];
                
                let status = callback(
                    banner.as_ptr(),
                    banner.len() as u32,
                    output.as_mut_ptr(),
                    output.len() as u32,
                );
                
                if status == 0 {
                    Ok(output)
                } else {
                    Err(format!("Probe failed: {}", status))
                }
            }
        }
    })
}
```

---

## Memory Safety Patterns

### Reference Counting Pattern

For resources that need cleanup:

```c
// C side: Reference-counted handle
typedef struct {
    void *rust_object;      // Opaque Rust pointer
    uint32_t ref_count;
    pthread_mutex_t lock;
} ObjectHandle;

ObjectHandle* handle_clone(ObjectHandle *orig) {
    pthread_mutex_lock(&orig->lock);
    orig->ref_count++;
    pthread_mutex_unlock(&orig->lock);
    return orig;
}

void handle_release(ObjectHandle *handle) {
    pthread_mutex_lock(&handle->lock);
    handle->ref_count--;
    if (handle->ref_count == 0) {
        // Call Rust cleanup
        rust_object_drop(handle->rust_object);
        free(handle);
    }
    pthread_mutex_unlock(&handle->lock);
}
```

### Arena Allocation Pattern (for batch operations)

```c
// Pre-allocate arena for multiple result structures
typedef struct {
    void *base;
    size_t capacity;
    size_t used;
} Arena;

Arena* arena_create(size_t capacity) {
    Arena *arena = malloc(sizeof(Arena));
    arena->base = malloc(capacity);
    arena->capacity = capacity;
    arena->used = 0;
    return arena;
}

void* arena_alloc(Arena *arena, size_t size) {
    if (arena->used + size > arena->capacity) {
        return NULL;  // Out of arena space
    }
    
    void *ptr = (char*)arena->base + arena->used;
    arena->used += size;
    return ptr;
}

void arena_free(Arena *arena) {
    free(arena->base);
    free(arena);
}
```

**Usage for batch fingerprinting**:

```c
// Pre-allocate arena for 10,000 results
Arena *arena = arena_create(10000 * sizeof(service_fingerprint_c_t));

for (int i = 0; i < 10000; i++) {
    service_fingerprint_c_t *result = arena_alloc(
        arena,
        sizeof(service_fingerprint_c_t)
    );
    
    // Use result...
}

arena_free(arena);  // Free all at once
```

---

## Performance Optimization Techniques

### Technique #1: Zero-Copy Banner Analysis

**Problem**: Copying banner data across language boundaries is expensive.

**Solution**: Pass only the pointer and length

```c
// BEFORE (with copying)
void analyze_banner_copy(const uint8_t *banner, uint32_t len) {
    // Copy to temporary buffer
    uint8_t *buffer = malloc(len);
    memcpy(buffer, banner, len);
    
    // Pass to Rust
    analysis_analyze(buffer, len);
    
    free(buffer);  // Overhead!
}

// AFTER (zero-copy)
void analyze_banner_zc(const uint8_t *banner, uint32_t len) {
    // Pass raw pointer directly
    // Rust uses it as &[u8] slice
    analysis_analyze(banner, len);
}
```

**Rust side**:

```rust
#[no_mangle]
pub extern "C" fn analysis_analyze(banner: *const u8, banner_len: u32) {
    if banner.is_null() || banner_len == 0 {
        return;
    }

    // Create slice from pointer (no copy)
    let banner_slice = unsafe {
        std::slice::from_raw_parts(banner, banner_len as usize)
    };

    // Process directly from slice
    analyze_internally(banner_slice);
}
```

### Technique #2: Batch Processing

**Problem**: FFI overhead is high per-call.

**Solution**: Process many items in one FFI call

```rust
// BEFORE (per-item FFI)
#[no_mangle]
pub extern "C" fn analyze_one(banner: *const u8, len: u32) -> *mut service_fingerprint_c_t {
    // FFI overhead for each call
}

// Usage: 10,000 calls = 10,000 FFI transitions (expensive!)
for (int i = 0; i < 10000; i++) {
    analyze_one(banners[i], lens[i]);
}

// AFTER (batch FFI)
#[no_mangle]
pub extern "C" fn analyze_batch(
    banners: *const *const u8,
    lens: *const u32,
    count: u32,
    results: *mut service_fingerprint_c_t,  // Pre-allocated by C
) -> u32 {
    // Single FFI call, analyze 10,000 items internally
}

// Usage: 1 FFI call
analyze_batch(banners_array, lens_array, 10000, results);
```

**Result**: 10,000x reduction in FFI overhead!

### Technique #3: SIMD-Accelerated Pattern Matching

```rust
// Use SIMD for fast pattern scanning
#[cfg(target_arch = "x86_64")]
use std::arch::x86_64::*;

fn fast_banner_scan(banner: &[u8]) -> Option<BannerType> {
    // Check for common patterns using SIMD
    
    if is_http_banner_simd(banner) {
        return Some(BannerType::Http);
    }
    
    if is_ssh_banner_simd(banner) {
        return Some(BannerType::Ssh);
    }
    
    // Fallback to regex for others
    analyze_with_regex(banner)
}

#[inline]
fn is_http_banner_simd(banner: &[u8]) -> bool {
    if banner.len() < 4 {
        return false;
    }
    
    // Check for "HTTP" using byte comparison
    #[cfg(target_arch = "x86_64")]
    unsafe {
        // Load first 4 bytes into SIMD register
        let pattern = _mm_setr_epi8(
            b'H' as i8, b'T' as i8, b'T' as i8, b'P' as i8,
            0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
        );
        
        let data = _mm_loadu_si128(banner.as_ptr() as *const __m128i);
        let cmp = _mm_cmpeq_epi8(data, pattern);
        let mask = _mm_movemask_epi8(cmp);
        
        return mask & 0xF == 0xF;  // First 4 bytes match
    }
    
    #[cfg(not(target_arch = "x86_64"))]
    {
        // Fallback for non-x86
        banner[0..4] == b"HTTP"[..]
    }
}
```

### Technique #4: Regex Caching

**Problem**: Recompiling regexes is expensive.

**Solution**: Use lazy_static or once_cell

```rust
use lazy_static::lazy_static;
use regex::Regex;

lazy_static! {
    static ref APACHE_VERSION: Regex = 
        Regex::new(r"Apache/(\d+\.\d+\.\d+)").unwrap();
    
    static ref NGINX_VERSION: Regex = 
        Regex::new(r"nginx/(\d+\.\d+\.\d+)").unwrap();
    
    // ... 30+ more regexes
}

fn extract_version(banner: &str) -> Option<String> {
    // Regexes already compiled (first call)
    // Subsequent calls use cached versions
    
    if let Some(cap) = APACHE_VERSION.captures(banner) {
        return cap.get(1).map(|m| m.as_str().to_string());
    }
    
    if let Some(cap) = NGINX_VERSION.captures(banner) {
        return cap.get(1).map(|m| m.as_str().to_string());
    }
    
    None
}
```

### Technique #5: Connection Pooling (C side)

```c
typedef struct {
    int *sockets;
    uint32_t pool_size;
    uint32_t available;
    pthread_mutex_t lock;
} SocketPool;

SocketPool* socket_pool_create(uint32_t size) {
    SocketPool *p = malloc(sizeof(SocketPool));
    p->sockets = malloc(size * sizeof(int));
    p->pool_size = size;
    p->available = 0;
    pthread_mutex_init(&p->lock, NULL);
    
    // Pre-allocate sockets
    for (uint32_t i = 0; i < size; i++) {
        p->sockets[i] = socket(AF_INET, SOCK_STREAM, 0);
        if (p->sockets[i] >= 0) {
            p->available++;
        }
    }
    
    return p;
}

int socket_pool_acquire(SocketPool *p) {
    pthread_mutex_lock(&p->lock);
    
    if (p->available > 0) {
        p->available--;
        int sock = p->sockets[p->available];
        
        pthread_mutex_unlock(&p->lock);
        return sock;
    }
    
    pthread_mutex_unlock(&p->lock);
    
    // Pool empty, create new socket
    return socket(AF_INET, SOCK_STREAM, 0);
}

void socket_pool_release(SocketPool *p, int sock) {
    if (sock < 0) return;
    
    pthread_mutex_lock(&p->lock);
    
    if (p->available < p->pool_size) {
        p->sockets[p->available++] = sock;
        // Reset socket state
        setsockopt(sock, SOL_SOCKET, SO_RCVBUF, &(int){65536}, sizeof(int));
    } else {
        close(sock);  // Pool is full
    }
    
    pthread_mutex_unlock(&p->lock);
}
```

---

## Concurrency Model

### Single-Threaded Event Loop (Recommended)

```c
// Main event loop (one thread per node)
int event_loop_main(network_context_t *ctx) {
    struct epoll_event events[1024];
    
    while (!shutdown_requested) {
        // Get next task (queue is thread-safe)
        task_t *task = scheduler_next_task(scheduler);
        if (!task) break;
        
        // Queue connection (non-blocking)
        int fd = socket(...);
        fcntl(fd, F_SETFL, O_NONBLOCK);
        connect(fd, ...);  // Returns EINPROGRESS
        
        // Register with epoll
        struct epoll_event ev;
        ev.events = EPOLLOUT | EPOLLIN | EPOLLERR;
        ev.data.ptr = connection_create(fd, task);
        epoll_ctl(epoll_fd, EPOLL_CTL_ADD, fd, &ev);
        
        // Process completed connections
        int n = epoll_wait(epoll_fd, events, 1024, timeout_ms);
        for (int i = 0; i < n; i++) {
            connection_t *conn = (connection_t*)events[i].data.ptr;
            
            if (events[i].events & EPOLLOUT) {
                // Connection ready for write
                handle_connect_complete(conn);
            }
            
            if (events[i].events & EPOLLIN) {
                // Data ready to read
                handle_data_ready(conn);
            }
            
            if (events[i].events & EPOLLERR) {
                // Connection error
                handle_connection_error(conn);
            }
        }
        
        // Metrics sampling (non-blocking)
        if (should_sample_metrics) {
            metrics_sample_time_series(metrics, ...);
        }
    }
    
    return 0;
}
```

**Benefits:**
- No mutex contention
- Deterministic scheduling
- Perfect for epoll/io_uring
- Maximum throughput per core

### Lock-Free Data Structures (for scheduler)

```c
// Lock-free task queue using compare-and-swap
typedef struct {
    task_t *buffer;
    uint32_t capacity;
    uint64_t head;      // Atomic
    uint64_t tail;      // Atomic
} LockFreeQueue;

int queue_enqueue(LockFreeQueue *q, task_t *task) {
    uint64_t tail = __atomic_load_n(&q->tail, __ATOMIC_RELEASE);
    uint64_t next = (tail + 1) % q->capacity;
    
    if (next == __atomic_load_n(&q->head, __ATOMIC_ACQUIRE)) {
        return -1;  // Queue full
    }
    
    q->buffer[tail] = *task;
    __atomic_store_n(&q->tail, next, __ATOMIC_RELEASE);
    return 0;
}

int queue_dequeue(LockFreeQueue *q, task_t *out) {
    uint64_t head = __atomic_load_n(&q->head, __ATOMIC_ACQUIRE);
    
    if (head == __atomic_load_n(&q->tail, __ATOMIC_RELEASE)) {
        return -1;  // Queue empty
    }
    
    *out = q->buffer[head];
    __atomic_store_n(&q->head, (head + 1) % q->capacity, __ATOMIC_RELEASE);
    return 0;
}
```

---

## Buffer Management

### Ring Buffer for Circular Tasks

```c
typedef struct {
    task_t *tasks;
    uint32_t capacity;
    uint32_t head;
    uint32_t tail;
    uint32_t count;
} RingBuffer;

RingBuffer* ring_buffer_create(uint32_t capacity) {
    RingBuffer *rb = malloc(sizeof(RingBuffer));
    rb->tasks = malloc(capacity * sizeof(task_t));
    rb->capacity = capacity;
    rb->head = rb->tail = rb->count = 0;
    return rb;
}

int ring_buffer_push(RingBuffer *rb, const task_t *task) {
    if (rb->count >= rb->capacity) {
        return -1;  // Full
    }
    
    rb->tasks[rb->tail] = *task;
    rb->tail = (rb->tail + 1) % rb->capacity;
    rb->count++;
    return 0;
}

int ring_buffer_pop(RingBuffer *rb, task_t *out) {
    if (rb->count == 0) {
        return -1;  // Empty
    }
    
    *out = rb->tasks[rb->head];
    rb->head = (rb->head + 1) % rb->capacity;
    rb->count--;
    return 0;
}

// Memory efficient: no allocation/deallocation during operation
// Perfect for real-time systems (constant memory footprint)
```

### Pre-Allocated Object Pools

```c
typedef struct {
    connection_t *pool;
    uint32_t *freelist;
    uint32_t pool_size;
    uint32_t free_count;
    pthread_spinlock_t lock;
} ObjectPool;

ObjectPool* object_pool_create(uint32_t size) {
    ObjectPool *p = malloc(sizeof(ObjectPool));
    p->pool = malloc(size * sizeof(connection_t));
    p->freelist = malloc(size * sizeof(uint32_t));
    
    for (uint32_t i = 0; i < size; i++) {
        p->freelist[i] = i;
    }
    
    p->pool_size = size;
    p->free_count = size;
    pthread_spinlock_init(&p->lock, PTHREAD_PROCESS_PRIVATE);
    
    return p;
}

connection_t* object_pool_acquire(ObjectPool *p) {
    pthread_spinlock_lock(&p->lock);
    
    if (p->free_count == 0) {
        pthread_spinlock_unlock(&p->lock);
        return NULL;  // No free objects
    }
    
    uint32_t idx = p->freelist[--p->free_count];
    connection_t *obj = &p->pool[idx];
    obj->pool_index = idx;
    
    pthread_spinlock_unlock(&p->lock);
    return obj;
}

void object_pool_release(ObjectPool *p, connection_t *obj) {
    if (!obj) return;
    
    pthread_spinlock_lock(&p->lock);
    
    if (p->free_count < p->pool_size) {
        p->freelist[p->free_count++] = obj->pool_index;
    }
    
    pthread_spinlock_unlock(&p->lock);
}
```

---

## Benchmarking Guide

### Benchmark Suite Template

```c
// benchmarks/benchmark_suite.c

#include <time.h>
#include <stdio.h>
#include "blackmap.h"

typedef struct {
    const char *name;
    uint64_t iterations;
    uint64_t total_ns;
    uint64_t min_ns;
    uint64_t max_ns;
} BenchResult;

#define BENCHMARK(name, code, iterations) do { \
    struct timespec start, end; \
    uint64_t min = UINT64_MAX, max = 0, total = 0; \
    \
    for (int i = 0; i < (iterations); i++) { \
        clock_gettime(CLOCK_MONOTONIC, &start); \
        { code } \
        clock_gettime(CLOCK_MONOTONIC, &end); \
        \
        uint64_t ns = (end.tv_sec - start.tv_sec) * 1e9 + \
                      (end.tv_nsec - start.tv_nsec); \
        total += ns; \
        min = (ns < min) ? ns : min; \
        max = (ns > max) ? ns : max; \
    } \
    \
    printf("%-40s: %8.2f µs (min: %.2f, max: %.2f)\n", \
           (name), total / (iterations) / 1000.0, \
           min / 1000.0, max / 1000.0); \
} while(0)

int main() {
    printf("BlackMap v3.2 Benchmarks\n");
    printf("========================\n\n");
    
    // Fingerprinting benchmark
    uint8_t http_banner[] = "HTTP/1.1 200 OK\r\nServer: Apache/2.4.41\r\n";
    BENCHMARK("Fingerprint HTTP Banner", {
        service_fingerprint_c_t *fp = analysis_fingerprint_banner(
            http_banner, sizeof(http_banner), 80
        );
        analysis_free_fingerprint(fp);
    }, 10000);
    
    // Connection pooling benchmark
    SocketPool *pool = socket_pool_create(1000);
    BENCHMARK("Acquire/Release Socket from Pool", {
        int sock = socket_pool_acquire(pool);
        socket_pool_release(pool, sock);
    }, 100000);
    
    // Stealth delay calculation
    stealth_config_v32_t stealth = stealth_get_preset(STEALTH_BALANCED);
    BENCHMARK("Calculate Stealth Delay", {
        uint32_t delay_us = stealth_get_pre_connect_delay_us(&stealth);
        (void)delay_us;
    }, 100000);
    
    // Metrics recording
    metrics_engine_t *metrics = metrics_engine_init(100);
    BENCHMARK("Record Connection Metric", {
        connection_metric_t m = {
            .connection_id = 1,
            .target_port = 80,
            .rtt_us = 15000,
        };
        metrics_record_connection(metrics, &m);
    }, 100000);
    
    printf("\nDone.\n");
    return 0;
}
```

### Performance Profiling with perf

```bash
# Compile with debug symbols
gcc -g -O2 blackmap.c -o blackmap

# Run under perf
perf record -g ./blackmap scan_config.json

# Generate report
perf report

# Flame graph
perf record -g --call-graph=dwarf ./blackmap scan_config.json
perf script > perf.script
# Convert to flame graph (flamegraph.pl)
```

---

## Summary: Optimization Checklist

- [ ] Use zero-copy FFI (pass pointers, not copies)
- [ ] Batch operations to reduce FFI transitions
- [ ] Pre-compile regexes (lazy_static)
- [ ] Use connection pooling
- [ ] Ring buffers for task queues
- [ ] Single-threaded event loop
- [ ] epoll/io_uring for I/O multiplexing
- [ ] Lock-free data structures where possible
- [ ] SIMD for pattern scanning
- [ ] Regular benchmarking and profiling
- [ ] Memory profiling with valgrind
- [ ] CPU profiling with perf/flight-recorder

