#include "../include/checksum.h"

uint16_t calculate_checksum(uint16_t *data, int len) {
    unsigned long sum = 0;
    unsigned short *ptr = data;
    
    while (len > 1) {
        sum += *ptr++;
        len -= 2;
    }
    
    if (len == 1) {
        sum += *(unsigned char *)ptr;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return (uint16_t)(~sum);
}

uint16_t calculate_ip_checksum(uint16_t *data, int len) {
    return calculate_checksum(data, len);
}

uint16_t calculate_tcp_checksum(uint32_t src_ip, uint32_t dst_ip, uint16_t *tcp_header, int tcp_len) {
    struct pseudo_header {
        uint32_t src_ip;
        uint32_t dst_ip;
        uint8_t placeholder;
        uint8_t protocol;
        uint16_t tcp_length;
    } pseudo;
    
    pseudo.src_ip = src_ip;
    pseudo.dst_ip = dst_ip;
    pseudo.placeholder = 0;
    pseudo.protocol = IPPROTO_TCP;
    pseudo.tcp_length = htons(tcp_len);
    
    int psize = sizeof(struct pseudo_header) + tcp_len;
    uint16_t *ptr = malloc(psize);
    if (!ptr) return 0;
    
    memcpy(ptr, &pseudo, sizeof(struct pseudo_header));
    memcpy((uint8_t *)ptr + sizeof(struct pseudo_header), tcp_header, tcp_len);
    
    uint16_t result = calculate_checksum(ptr, psize);
    free(ptr);
    
    return result;
}

uint16_t calculate_udp_checksum(uint32_t src_ip, uint32_t dst_ip, uint16_t *udp_header, int udp_len) {
    struct pseudo_header {
        uint32_t src_ip;
        uint32_t dst_ip;
        uint8_t placeholder;
        uint8_t protocol;
        uint16_t udp_length;
    } pseudo;
    
    pseudo.src_ip = src_ip;
    pseudo.dst_ip = dst_ip;
    pseudo.placeholder = 0;
    pseudo.protocol = IPPROTO_UDP;
    pseudo.udp_length = htons(udp_len);
    
    int psize = sizeof(struct pseudo_header) + udp_len;
    uint16_t *ptr = malloc(psize);
    if (!ptr) return 0;
    
    memcpy(ptr, &pseudo, sizeof(struct pseudo_header));
    memcpy((uint8_t *)ptr + sizeof(struct pseudo_header), udp_header, udp_len);
    
    uint16_t result = calculate_checksum(ptr, psize);
    free(ptr);
    
    return result;
}
