#include "../include/scanners.h"
#include "../include/raw_socket.h"
#include "../include/packet_builder.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <sys/socket.h>

#define SYN_TIMEOUT 2000
#define PACKET_SIZE 128
#define TCP_HEADER_MIN_LEN 20

static uint32_t generate_seq() {
    return (uint32_t)(rand() % 0xFFFFFFFF);
}

static uint16_t generate_port() {
    return (uint16_t)(1024 + rand() % (65535 - 1024));
}

port_state_t syn_scan(const char *target_ip, int port, int timeout) {
    raw_socket_t *sock = raw_socket_create(true);
    if (!sock) {
        return connect_scan(target_ip, port, timeout);
    }
    
    uint32_t dst_ip = inet_addr(target_ip);
    uint32_t src_ip = sock->source_ip ? sock->source_ip : inet_addr("0.0.0.0");
    uint16_t src_port = generate_port();
    
    packet_config_t config = {
        .src_ip = src_ip,
        .dst_ip = dst_ip,
        .src_port = src_port,
        .dst_port = port,
        .seq = generate_seq(),
        .ack = 0,
        .flags = TH_SYN,
        .window = 65535,
        .ttl = 64,
        .tos = 0
    };
    
    char tcp_buffer[TCP_HEADER_MIN_LEN];
    char ip_buffer[PACKET_SIZE];
    
    int tcp_len = build_tcp_packet(&config, tcp_buffer, sizeof(tcp_buffer));
    if (tcp_len < 0) {
        raw_socket_destroy(sock);
        return PORT_FILTERED;
    }
    
    int total_len = build_ip_packet(&config, tcp_buffer, tcp_len, ip_buffer, sizeof(ip_buffer));
    if (total_len < 0) {
        raw_socket_destroy(sock);
        return PORT_FILTERED;
    }
    
    if (raw_socket_send(sock, ip_buffer, total_len, dst_ip, port) < 0) {
        raw_socket_destroy(sock);
        return connect_scan(target_ip, port, timeout);
    }
    
    char response[PACKET_SIZE];
    int recv_len = raw_socket_receive(sock, response, sizeof(response), timeout);
    raw_socket_destroy(sock);
    
    if (recv_len <= 0) {
        return PORT_FILTERED;
    }
    
    struct iphdr *resp_ip = (struct iphdr *)response;
    if (resp_ip->protocol != IPPROTO_TCP) {
        return PORT_FILTERED;
    }
    
    struct tcphdr *resp_tcp = (struct tcphdr *)(response + resp_ip->ihl * 4);
    uint16_t resp_port = ntohs(resp_tcp->th_sport);
    
    if (resp_port != port) {
        return PORT_FILTERED;
    }
    
    if ((resp_tcp->th_flags & (TH_SYN | TH_ACK)) == (TH_SYN | TH_ACK)) {
        return PORT_OPEN;
    }
    
    if (resp_tcp->th_flags & TH_RST) {
        return PORT_CLOSED;
    }
    
    return PORT_FILTERED;
}

port_state_t connect_scan(const char *target_ip, int port, int timeout) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return PORT_FILTERED;
    
    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, target_ip, &addr.sin_addr);
    
    int result = connect(sock, (struct sockaddr *)&addr, sizeof(addr));
    
    if (result < 0) {
        if (errno == EINPROGRESS) {
            fd_set write_fds;
            FD_ZERO(&write_fds);
            FD_SET(sock, &write_fds);
            
            struct timeval tv;
            tv.tv_sec = timeout / 1000;
            tv.tv_usec = (timeout % 1000) * 1000;
            
            int sel = select(sock + 1, NULL, &write_fds, NULL, &tv);
            
            if (sel > 0) {
                int so_error;
                socklen_t len = sizeof(so_error);
                getsockopt(sock, SOL_SOCKET, SO_ERROR, &so_error, &len);
                
                close(sock);
                if (so_error == 0) return PORT_OPEN;
                return PORT_CLOSED;
            } else if (sel == 0) {
                close(sock);
                return PORT_FILTERED;
            }
        }
    } else {
        close(sock);
        return PORT_OPEN;
    }
    
    close(sock);
    return PORT_CLOSED;
}

port_state_t fin_scan(const char *target_ip, int port, int timeout) {
    raw_socket_t *sock = raw_socket_create(true);
    if (!sock) return PORT_FILTERED;
    
    uint32_t dst_ip = inet_addr(target_ip);
    uint32_t src_ip = sock->source_ip ? sock->source_ip : inet_addr("0.0.0.0");
    
    packet_config_t config = {
        .src_ip = src_ip,
        .dst_ip = dst_ip,
        .src_port = generate_port(),
        .dst_port = port,
        .seq = generate_seq(),
        .ack = 0,
        .flags = TH_FIN,
        .window = 65535,
        .ttl = 64,
        .tos = 0
    };
    
    char tcp_buffer[TCP_HEADER_MIN_LEN];
    char ip_buffer[PACKET_SIZE];
    
    int tcp_len = build_tcp_packet(&config, tcp_buffer, sizeof(tcp_buffer));
    if (tcp_len < 0) {
        raw_socket_destroy(sock);
        return PORT_FILTERED;
    }
    
    int total_len = build_ip_packet(&config, tcp_buffer, tcp_len, ip_buffer, sizeof(ip_buffer));
    if (total_len < 0) {
        raw_socket_destroy(sock);
        return PORT_FILTERED;
    }
    
    raw_socket_send(sock, ip_buffer, total_len, dst_ip, port);
    
    char response[PACKET_SIZE];
    int recv_len = raw_socket_receive(sock, response, sizeof(response), timeout);
    raw_socket_destroy(sock);
    
    if (recv_len <= 0) {
        return PORT_OPEN;
    }
    
    return PORT_CLOSED;
}

port_state_t xmas_scan(const char *target_ip, int port, int timeout) {
    raw_socket_t *sock = raw_socket_create(true);
    if (!sock) return PORT_FILTERED;
    
    uint32_t dst_ip = inet_addr(target_ip);
    uint32_t src_ip = sock->source_ip ? sock->source_ip : inet_addr("0.0.0.0");
    
    packet_config_t config = {
        .src_ip = src_ip,
        .dst_ip = dst_ip,
        .src_port = generate_port(),
        .dst_port = port,
        .seq = generate_seq(),
        .ack = 0,
        .flags = TH_FIN | TH_PUSH | TH_URG,
        .window = 65535,
        .ttl = 64,
        .tos = 0
    };
    
    char tcp_buffer[TCP_HEADER_MIN_LEN];
    char ip_buffer[PACKET_SIZE];
    
    int tcp_len = build_tcp_packet(&config, tcp_buffer, sizeof(tcp_buffer));
    if (tcp_len < 0) {
        raw_socket_destroy(sock);
        return PORT_FILTERED;
    }
    
    int total_len = build_ip_packet(&config, tcp_buffer, tcp_len, ip_buffer, sizeof(ip_buffer));
    if (total_len < 0) {
        raw_socket_destroy(sock);
        return PORT_FILTERED;
    }
    
    raw_socket_send(sock, ip_buffer, total_len, dst_ip, port);
    
    char response[PACKET_SIZE];
    int recv_len = raw_socket_receive(sock, response, sizeof(response), timeout);
    raw_socket_destroy(sock);
    
    if (recv_len <= 0) {
        return PORT_OPEN;
    }
    
    return PORT_CLOSED;
}

port_state_t null_scan(const char *target_ip, int port, int timeout) {
    raw_socket_t *sock = raw_socket_create(true);
    if (!sock) return PORT_FILTERED;
    
    uint32_t dst_ip = inet_addr(target_ip);
    uint32_t src_ip = sock->source_ip ? sock->source_ip : inet_addr("0.0.0.0");
    
    packet_config_t config = {
        .src_ip = src_ip,
        .dst_ip = dst_ip,
        .src_port = generate_port(),
        .dst_port = port,
        .seq = generate_seq(),
        .ack = 0,
        .flags = 0,
        .window = 65535,
        .ttl = 64,
        .tos = 0
    };
    
    char tcp_buffer[TCP_HEADER_MIN_LEN];
    char ip_buffer[PACKET_SIZE];
    
    int tcp_len = build_tcp_packet(&config, tcp_buffer, sizeof(tcp_buffer));
    if (tcp_len < 0) {
        raw_socket_destroy(sock);
        return PORT_FILTERED;
    }
    
    int total_len = build_ip_packet(&config, tcp_buffer, tcp_len, ip_buffer, sizeof(ip_buffer));
    if (total_len < 0) {
        raw_socket_destroy(sock);
        return PORT_FILTERED;
    }
    
    raw_socket_send(sock, ip_buffer, total_len, dst_ip, port);
    
    char response[PACKET_SIZE];
    int recv_len = raw_socket_receive(sock, response, sizeof(response), timeout);
    raw_socket_destroy(sock);
    
    if (recv_len <= 0) {
        return PORT_OPEN;
    }
    
    return PORT_CLOSED;
}

port_state_t ack_scan(const char *target_ip, int port, int timeout) {
    raw_socket_t *sock = raw_socket_create(true);
    if (!sock) return PORT_FILTERED;
    
    uint32_t dst_ip = inet_addr(target_ip);
    uint32_t src_ip = sock->source_ip ? sock->source_ip : inet_addr("0.0.0.0");
    
    packet_config_t config = {
        .src_ip = src_ip,
        .dst_ip = dst_ip,
        .src_port = generate_port(),
        .dst_port = port,
        .seq = generate_seq(),
        .ack = 0,
        .flags = TH_ACK,
        .window = 65535,
        .ttl = 64,
        .tos = 0
    };
    
    char tcp_buffer[TCP_HEADER_MIN_LEN];
    char ip_buffer[PACKET_SIZE];
    
    int tcp_len = build_tcp_packet(&config, tcp_buffer, sizeof(tcp_buffer));
    if (tcp_len < 0) {
        raw_socket_destroy(sock);
        return PORT_FILTERED;
    }
    
    int total_len = build_ip_packet(&config, tcp_buffer, tcp_len, ip_buffer, sizeof(ip_buffer));
    if (total_len < 0) {
        raw_socket_destroy(sock);
        return PORT_FILTERED;
    }
    
    raw_socket_send(sock, ip_buffer, total_len, dst_ip, port);
    
    char response[PACKET_SIZE];
    int recv_len = raw_socket_receive(sock, response, sizeof(response), timeout);
    raw_socket_destroy(sock);
    
    if (recv_len > 0) {
        struct iphdr *resp_ip = (struct iphdr *)response;
        if (resp_ip->protocol == IPPROTO_TCP) {
            struct tcphdr *resp_tcp = (struct tcphdr *)(response + resp_ip->ihl * 4);
            if (resp_tcp->th_flags & TH_RST) {
                return PORT_UNFILTERED;
            }
        }
    }
    
    return PORT_FILTERED;
}

port_state_t udp_scan(const char *target_ip, int port, int timeout) {
    int sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock < 0) return PORT_FILTERED;
    
    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));
    
    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, target_ip, &addr.sin_addr);
    
    char probe[8] = {0};
    sendto(sock, probe, sizeof(probe), 0, (struct sockaddr *)&addr, sizeof(addr));
    
    char buffer[128];
    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);
    
    int n = recvfrom(sock, buffer, sizeof(buffer), 0, (struct sockaddr *)&from, &from_len);
    close(sock);
    
    if (n > 0) return PORT_OPEN;
    if (errno == EAGAIN || errno == EWOULDBLOCK) return PORT_FILTERED;
    return PORT_CLOSED;
}

scan_func_t get_scan_function(scan_type_t scan_type) {
    switch (scan_type) {
        case SCAN_SYN: return syn_scan;
        case SCAN_FIN: return fin_scan;
        case SCAN_XMAS: return xmas_scan;
        case SCAN_NULL: return null_scan;
        case SCAN_ACK: return ack_scan;
        case SCAN_UDP: return udp_scan;
        case SCAN_CONNECT:
        default:
            return connect_scan;
    }
}
