#include "../include/packet_builder.h"
#include "../include/checksum.h"
#include <string.h>
#include <stdlib.h>
#include <arpa/inet.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>

#define IP_HEADER_MIN_LEN 20
#define TCP_HEADER_MIN_LEN 20
#define UDP_HEADER_LEN 8

int build_tcp_packet(const packet_config_t *config, char *buffer, size_t buffer_len) {
    if (!config || !buffer || buffer_len < TCP_HEADER_MIN_LEN) {
        return -1;
    }
    
    struct tcphdr *tcp = (struct tcphdr *)buffer;
    memset(tcp, 0, TCP_HEADER_MIN_LEN);
    
    tcp->th_sport = htons(config->src_port);
    tcp->th_dport = htons(config->dst_port);
    tcp->th_seq = htonl(config->seq);
    tcp->th_ack = htonl(config->ack);
    tcp->th_off = 5;
    tcp->th_x2 = 0;
    tcp->th_flags = config->flags;
    tcp->th_win = htons(config->window ? config->window : 65535);
    tcp->th_urp = 0;
    
    tcp->th_sum = 0;
    tcp->th_sum = calculate_tcp_checksum(config->src_ip, config->dst_ip, 
                                          (uint16_t *)tcp, TCP_HEADER_MIN_LEN);
    
    return TCP_HEADER_MIN_LEN;
}

int build_udp_packet(const packet_config_t *config, char *buffer, size_t buffer_len) {
    if (!config || !buffer || buffer_len < UDP_HEADER_LEN) {
        return -1;
    }
    
    struct udphdr *udp = (struct udphdr *)buffer;
    memset(udp, 0, UDP_HEADER_LEN);
    
    udp->uh_sport = htons(config->src_port);
    udp->uh_dport = htons(config->dst_port);
    udp->uh_ulen = htons(UDP_HEADER_LEN);
    udp->uh_sum = 0;
    
    udp->uh_sum = calculate_udp_checksum(config->src_ip, config->dst_ip,
                                            (uint16_t *)udp, UDP_HEADER_LEN);
    
    return UDP_HEADER_LEN;
}

int build_ip_packet(const packet_config_t *config, char *protocol_data, size_t protocol_len, 
                    char *buffer, size_t buffer_len) {
    if (!config || !protocol_data || !buffer) {
        return -1;
    }
    
    size_t total_len = IP_HEADER_MIN_LEN + protocol_len;
    if (buffer_len < total_len) {
        return -1;
    }
    
    struct iphdr *ip = (struct iphdr *)buffer;
    memset(ip, 0, IP_HEADER_MIN_LEN);
    
    ip->ihl = 5;
    ip->version = 4;
    ip->tos = config->tos ? config->tos : 0;
    ip->tot_len = htons(total_len);
    ip->id = htons(rand() % 65535);
    ip->frag_off = 0;
    ip->ttl = config->ttl ? config->ttl : 64;
    ip->protocol = (protocol_len == UDP_HEADER_LEN) ? IPPROTO_UDP : IPPROTO_TCP;
    ip->check = 0;
    ip->saddr = config->src_ip;
    ip->daddr = config->dst_ip;
    
    ip->check = calculate_ip_checksum((uint16_t *)ip, IP_HEADER_MIN_LEN);
    
    memcpy(buffer + IP_HEADER_MIN_LEN, protocol_data, protocol_len);
    
    return total_len;
}
