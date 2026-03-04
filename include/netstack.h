#ifndef NETSTACK_H
#define NETSTACK_H

#include <stdint.h>
#include <stdbool.h>
#include <netinet/in.h>

/* BlackStack - Custom TCP/IP Stack */

/* Packet types */
typedef struct {
    uint8_t *data;
    uint32_t len;
    uint32_t capacity;
    uint32_t offset;
} packet_t;

/* TCP State Machine */
typedef enum {
    BMAP_TCP_CLOSED = 0,
    BMAP_TCP_LISTEN = 1,
    BMAP_TCP_SYN_SENT = 2,
    BMAP_TCP_SYN_RECEIVED = 3,
    BMAP_TCP_ESTABLISHED = 4,
    BMAP_TCP_FIN_WAIT_1 = 5,
    BMAP_TCP_FIN_WAIT_2 = 6,
    BMAP_TCP_CLOSE_WAIT = 7,
    BMAP_TCP_CLOSING = 8,
    BMAP_TCP_LAST_ACK = 9,
    BMAP_TCP_TIME_WAIT = 10
} tcp_state_t;

/* TCP Socket State */
typedef struct {
    uint32_t saddr;
    uint32_t daddr;
    uint16_t sport;
    uint16_t dport;
    uint32_t seq;
    uint32_t ack;
    tcp_state_t state;
    struct timespec created;
} tcp_socket_t;

/* Checksum calculations */
uint16_t calculate_checksum(uint8_t *data, uint32_t len);
uint16_t calculate_ipv4_checksum(uint8_t *ip_header);
uint16_t calculate_tcp_checksum(uint8_t *ip_header, uint8_t *tcp_header);

/* Packet construction */
packet_t *packet_create(uint32_t size);
void packet_destroy(packet_t *pkt);
int packet_append(packet_t *pkt, const uint8_t *data, uint32_t len);

/* IP functions */
int ip_send_raw(const uint8_t *packet, uint32_t len);
int ipv4_build_header(packet_t *pkt, uint32_t saddr, uint32_t daddr, 
                      uint8_t protocol, uint8_t ttl);
int ipv6_build_header(packet_t *pkt, struct in6_addr *saddr, 
                      struct in6_addr *daddr, uint8_t protocol, uint8_t ttl);

/* TCP functions */
int tcp_build_header(packet_t *pkt, uint16_t sport, uint16_t dport,
                     uint32_t seq, uint32_t ack, uint8_t flags, 
                     uint16_t window, const uint8_t *options, uint32_t opt_len);
int tcp_send_syn(uint32_t daddr, uint16_t dport);
int tcp_send_ack(uint32_t daddr, uint16_t dport, uint32_t seq, uint32_t ack);
int tcp_send_rst(uint32_t daddr, uint16_t dport, uint32_t seq, uint32_t ack);
int tcp_send_fin(uint32_t daddr, uint16_t dport, uint32_t seq, uint32_t ack);

/* UDP functions */
int udp_build_header(packet_t *pkt, uint16_t sport, uint16_t dport,
                     const uint8_t *payload, uint32_t payload_len);
int udp_send_packet(uint32_t daddr, uint16_t dport, 
                    const uint8_t *payload, uint32_t payload_len);

/* ICMP functions */
int icmp_build_echo_request(packet_t *pkt, uint16_t id, uint16_t seq,
                            const uint8_t *data, uint32_t data_len);
int icmp_send_echo(uint32_t daddr, uint16_t id, uint16_t seq);

/* SCTP functions */
int sctp_build_init_chunk(packet_t *pkt, uint32_t vtag);
int sctp_send_init(uint32_t daddr, uint16_t dport);

#endif /* NETSTACK_H */
