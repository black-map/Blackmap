#ifndef BLACKMAP_RUST_H
#define BLACKMAP_RUST_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

const char* blackmap_analyze_banner(const char* input);
void blackmap_free_string(char* s);

#ifdef __cplusplus
}
#endif

#endif /* BLACKMAP_RUST_H */