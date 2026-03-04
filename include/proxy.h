#ifndef PROXY_H
#define PROXY_H

#include <stdbool.h>

/* Proxy detection and handling */
bool detect_proxy_active(void);
void enforce_proxy_mode(void);
void disable_raw_sockets_for_proxy(void);
void disable_os_detection_for_proxy(void);

#endif /* PROXY_H */