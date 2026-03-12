#ifndef RAW_SOCKET_H
#define RAW_SOCKET_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

typedef struct {
    int sock_fd;
    int epoll_fd;
    bool raw_mode;
    uint32_t source_ip;
    uint16_t source_port;
} raw_socket_t;

raw_socket_t* raw_socket_create(bool enable_ip_hdr);
void raw_socket_destroy(raw_socket_t *sock);
int raw_socket_send(raw_socket_t *sock, const char *packet, size_t len, uint32_t dst_ip, uint16_t dst_port);
int raw_socket_receive(raw_socket_t *sock, char *buffer, size_t len, int timeout_ms);
int raw_socket_set_filter(raw_socket_t *sock, uint16_t port);
int raw_socket_bind_to_interface(raw_socket_t *sock, const char *interface);

#endif
