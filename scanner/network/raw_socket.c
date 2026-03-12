#include "../include/raw_socket.h"
#include "../include/common.h"
#include <stdio.h>
#include <errno.h>
#include <string.h>

raw_socket_t* raw_socket_create(bool enable_ip_hdr) {
    raw_socket_t *sock = malloc(sizeof(raw_socket_t));
    if (!sock) return NULL;
    
    memset(sock, 0, sizeof(raw_socket_t));
    
    int protocol = enable_ip_hdr ? IPPROTO_RAW : IPPROTO_TCP;
    sock->sock_fd = socket(AF_INET, SOCK_RAW, protocol);
    
    if (sock->sock_fd < 0) {
        sock->sock_fd = socket(AF_INET, SOCK_RAW, IPPROTO_RAW);
        if (sock->sock_fd < 0) {
            free(sock);
            return NULL;
        }
    }
    
    int on = 1;
    if (setsockopt(sock->sock_fd, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on)) < 0) {
        close(sock->sock_fd);
        free(sock);
        return NULL;
    }
    
    sock->epoll_fd = epoll_create1(0);
    if (sock->epoll_fd < 0) {
        close(sock->sock_fd);
        free(sock);
        return NULL;
    }
    
    struct epoll_event ev;
    ev.events = EPOLLIN;
    ev.data.fd = sock->sock_fd;
    epoll_ctl(sock->epoll_fd, EPOLL_CTL_ADD, sock->sock_fd, &ev);
    
    sock->raw_mode = enable_ip_hdr;
    
    return sock;
}

void raw_socket_destroy(raw_socket_t *sock) {
    if (!sock) return;
    if (sock->epoll_fd >= 0) close(sock->epoll_fd);
    if (sock->sock_fd >= 0) close(sock->sock_fd);
    free(sock);
}

int raw_socket_send(raw_socket_t *sock, const char *packet, size_t len, uint32_t dst_ip, uint16_t dst_port) {
    if (!sock || !packet) return -1;
    
    struct sockaddr_in dest;
    memset(&dest, 0, sizeof(dest));
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = dst_ip;
    dest.sin_port = htons(dst_port);
    
    ssize_t sent = sendto(sock->sock_fd, packet, len, 0, 
                          (struct sockaddr *)&dest, sizeof(dest));
    
    return (sent == (ssize_t)len) ? 0 : -1;
}

int raw_socket_receive(raw_socket_t *sock, char *buffer, size_t len, int timeout_ms) {
    if (!sock || !buffer) return -1;
    
    struct epoll_event events[EPOLL_MAX_EVENTS];
    int nfds = epoll_wait(sock->epoll_fd, events, EPOLL_MAX_EVENTS, timeout_ms);
    
    if (nfds <= 0) return 0;
    
    ssize_t received = recv(sock->sock_fd, buffer, len, 0);
    return (received > 0) ? (int)received : 0;
}

int raw_socket_set_filter(raw_socket_t *sock, uint16_t port) {
    (void)sock;
    (void)port;
    return 0;
}

int raw_socket_bind_to_interface(raw_socket_t *sock, const char *interface) {
    if (!sock || !interface) return -1;
    
    if (setsockopt(sock->sock_fd, SOL_SOCKET, SO_BINDTODEVICE, interface, strlen(interface)) < 0) {
        return -1;
    }
    
    return 0;
}
