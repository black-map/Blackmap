#ifndef PACKET_BUILDER_H
#define PACKET_BUILDER_H

#define _DEFAULT_SOURCE

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

typedef struct {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq;
    uint32_t ack;
    uint8_t flags;
    uint16_t window;
    uint8_t ttl;
    uint8_t tos;
} packet_config_t;

int build_tcp_packet(const packet_config_t *config, char *buffer, size_t buffer_len);
int build_udp_packet(const packet_config_t *config, char *buffer, size_t buffer_len);
int build_ip_packet(const packet_config_t *config, char *protocol_data, size_t protocol_len, char *buffer, size_t buffer_len);

#endif
