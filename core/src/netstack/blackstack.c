#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <time.h>
#include "netstack.h"

/* BlackStack - Custom TCP/IP Stack Implementation */

uint16_t calculate_checksum(uint8_t *data, uint32_t len) {
    uint32_t sum = 0;
    uint16_t *ptr = (uint16_t *)data;
    uint32_t count = len / 2;
    
    while (count--) {
        sum += *ptr++;
        if (sum & 0x80000000) {
            sum = (sum & 0xFFFF) + (sum >> 16);
        }
    }
    
    if (len & 1) {
        sum += (uint16_t)*(uint8_t *)ptr;
    }
    
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    
    return ~sum;
}

uint16_t calculate_ipv4_checksum(uint8_t *ip_header) {
    struct iphdr *iph = (struct iphdr *)ip_header;
    uint16_t orig_checksum = iph->check;
    
    iph->check = 0;
    uint16_t checksum = calculate_checksum(ip_header, iph->ihl * 4);
    iph->check = orig_checksum;
    
    return checksum;
}

uint16_t calculate_tcp_checksum(uint8_t *ip_header, uint8_t *tcp_header) {
    struct iphdr *iph = (struct iphdr *)ip_header;
    struct tcphdr *tcph = (struct tcphdr *)tcp_header;
    
    uint16_t tcp_len = ntohs(iph->tot_len) - (iph->ihl * 4);
    
    /* Pseudo-header for checksum calculation */
    uint8_t pseudo_header[12 + 60]; /* pseudo + max TCP header */
    int pos = 0;
    
    memcpy(pseudo_header + pos, &iph->saddr, 4);
    pos += 4;
    memcpy(pseudo_header + pos, &iph->daddr, 4);
    pos += 4;
    pseudo_header[pos++] = 0;
    pseudo_header[pos++] = iph->protocol;
    memcpy(pseudo_header + pos, &tcp_len, 2);
    pos += 2;
    memcpy(pseudo_header + pos, tcp_header, tcp_len);
    
    return calculate_checksum(pseudo_header, pos + tcp_len);
}

packet_t *packet_create(uint32_t size) {
    packet_t *pkt = malloc(sizeof(packet_t));
    if (!pkt) return NULL;
    
    pkt->data = malloc(size);
    if (!pkt->data) {
        free(pkt);
        return NULL;
    }
    
    pkt->len = 0;
    pkt->capacity = size;
    pkt->offset = 0;
    
    return pkt;
}

void packet_destroy(packet_t *pkt) {
    if (pkt) {
        free(pkt->data);
        free(pkt);
    }
}

int packet_append(packet_t *pkt, const uint8_t *data, uint32_t len) {
    if (!pkt || !data) return -1;
    
    if (pkt->len + len > pkt->capacity) {
        return -1;  /* Buffer overflow */
    }
    
    memcpy(pkt->data + pkt->len, data, len);
    pkt->len += len;
    
    return 0;
}

int ip_send_raw(const uint8_t *packet, uint32_t len) {
    /* Stub: Send raw IP packet through socket */
    (void)packet;
    (void)len;
    return 0;
}

int ipv4_build_header(packet_t *pkt, uint32_t saddr, uint32_t daddr, 
                      uint8_t protocol, uint8_t ttl) {
    struct iphdr iph;
    
    memset(&iph, 0, sizeof(iph));
    iph.version = 4;
    iph.ihl = 5;
    iph.tos = 0;
    iph.tot_len = htons(sizeof(struct iphdr) + 20);  /* Will be updated */
    iph.id = htons(rand() % 0xFFFF);
    iph.frag_off = 0;
    iph.ttl = ttl ? ttl : 64;
    iph.protocol = protocol;
    iph.check = 0;
    iph.saddr = saddr;
    iph.daddr = daddr;
    
    iph.check = calculate_checksum((uint8_t *)&iph, sizeof(iph));
    
    return packet_append(pkt, (uint8_t *)&iph, sizeof(iph));
}

int ipv6_build_header(packet_t *pkt, struct in6_addr *saddr, 
                      struct in6_addr *daddr, uint8_t protocol, uint8_t ttl) {
    /* IPv6 header construction */
    (void)pkt;
    (void)saddr;
    (void)daddr;
    (void)protocol;
    (void)ttl;
    return 0;  /* TODO */
}

int tcp_build_header(packet_t *pkt, uint16_t sport, uint16_t dport,
                     uint32_t seq, uint32_t ack, uint8_t flags, 
                     uint16_t window, const uint8_t *options, uint32_t opt_len) {
    struct tcphdr tcph;
    
    memset(&tcph, 0, sizeof(tcph));
    tcph.source = htons(sport);
    tcph.dest = htons(dport);
    tcph.seq = htonl(seq);
    tcph.ack_seq = htonl(ack);
    tcph.doff = (sizeof(struct tcphdr) + opt_len) / 4;
    tcph.th_flags = flags;
    tcph.window = htons(window);
    tcph.check = 0;
    tcph.urg_ptr = 0;
    
    if (packet_append(pkt, (uint8_t *)&tcph, sizeof(tcph)) != 0) {
        return -1;
    }
    
    if (options && opt_len > 0) {
        if (packet_append(pkt, options, opt_len) != 0) {
            return -1;
        }
    }
    
    return 0;
}

int tcp_send_syn(uint32_t daddr, uint16_t dport) {
    packet_t *pkt = packet_create(65536);
    if (!pkt) return -1;
    
    ipv4_build_header(pkt, 0, daddr, IPPROTO_TCP, 64);
    tcp_build_header(pkt, rand() % 65536, dport, rand() % 0xFFFFFFFF, 0, 
                     0x02, 65535, NULL, 0);  /* SYN flag */
    
    int ret = ip_send_raw(pkt->data, pkt->len);
    packet_destroy(pkt);
    
    return ret;
}

int tcp_send_ack(uint32_t daddr, uint16_t dport, uint32_t seq, uint32_t ack) {
    packet_t *pkt = packet_create(65536);
    if (!pkt) return -1;
    
    ipv4_build_header(pkt, 0, daddr, IPPROTO_TCP, 64);
    tcp_build_header(pkt, rand() % 65536, dport, seq, ack, 0x10, 65535, NULL, 0);
    
    int ret = ip_send_raw(pkt->data, pkt->len);
    packet_destroy(pkt);
    
    return ret;
}

int tcp_send_rst(uint32_t daddr, uint16_t dport, uint32_t seq, uint32_t ack) {
    (void)daddr;
    (void)dport;
    (void)seq;
    (void)ack;
    return 0;  /* TODO */
}

int tcp_send_fin(uint32_t daddr, uint16_t dport, uint32_t seq, uint32_t ack) {
    (void)daddr;
    (void)dport;
    (void)seq;
    (void)ack;
    return 0;  /* TODO */
}

int udp_build_header(packet_t *pkt, uint16_t sport, uint16_t dport,
                     const uint8_t *payload, uint32_t payload_len) {
    struct udphdr udph;
    
    memset(&udph, 0, sizeof(udph));
    udph.source = htons(sport);
    udph.dest = htons(dport);
    udph.len = htons(sizeof(struct udphdr) + payload_len);
    udph.check = 0;
    
    if (packet_append(pkt, (uint8_t *)&udph, sizeof(udph)) != 0) {
        return -1;
    }
    
    if (payload && payload_len > 0) {
        if (packet_append(pkt, payload, payload_len) != 0) {
            return -1;
        }
    }
    
    return 0;
}

int udp_send_packet(uint32_t daddr, uint16_t dport, 
                    const uint8_t *payload, uint32_t payload_len) {
    packet_t *pkt = packet_create(65536);
    if (!pkt) return -1;
    
    ipv4_build_header(pkt, 0, daddr, IPPROTO_UDP, 64);
    udp_build_header(pkt, rand() % 65536, dport, payload, payload_len);
    
    int ret = ip_send_raw(pkt->data, pkt->len);
    packet_destroy(pkt);
    
    return ret;
}

int icmp_build_echo_request(packet_t *pkt, uint16_t id, uint16_t seq,
                            const uint8_t *data, uint32_t data_len) {
    struct icmphdr icmph;
    
    memset(&icmph, 0, sizeof(icmph));
    icmph.type = ICMP_ECHO;
    icmph.code = 0;
    icmph.checksum = 0;
    icmph.un.echo.id = htons(id);
    icmph.un.echo.sequence = htons(seq);
    
    if (packet_append(pkt, (uint8_t *)&icmph, sizeof(icmph)) != 0) {
        return -1;
    }
    
    if (data && data_len > 0) {
        if (packet_append(pkt, data, data_len) != 0) {
            return -1;
        }
    }
    
    return 0;
}

int icmp_send_echo(uint32_t daddr, uint16_t id, uint16_t seq) {
    packet_t *pkt = packet_create(65536);
    if (!pkt) return -1;
    
    ipv4_build_header(pkt, 0, daddr, IPPROTO_ICMP, 64);
    icmp_build_echo_request(pkt, id, seq, NULL, 0);
    
    int ret = ip_send_raw(pkt->data, pkt->len);
    packet_destroy(pkt);
    
    return ret;
}

int sctp_build_init_chunk(packet_t *pkt, uint32_t vtag) {
    /* SCTP INIT chunk construction */
    (void)pkt;
    (void)vtag;
    return 0;  /* TODO */
}

int sctp_send_init(uint32_t daddr, uint16_t dport) {
    (void)daddr;
    (void)dport;
    return 0;  /* TODO */
}
