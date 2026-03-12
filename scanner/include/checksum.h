#ifndef CHECKSUM_H
#define CHECKSUM_H

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <arpa/inet.h>
#include <netinet/in.h>

uint16_t calculate_ip_checksum(uint16_t *data, int len);
uint16_t calculate_tcp_checksum(uint32_t src_ip, uint32_t dst_ip, uint16_t *tcp_header, int tcp_len);
uint16_t calculate_udp_checksum(uint32_t src_ip, uint32_t dst_ip, uint16_t *udp_header, int udp_len);
uint16_t calculate_checksum(uint16_t *data, int len);

#endif
