#ifndef COMMON_H
#define COMMON_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <errno.h>
#include <signal.h>
#include <time.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <netinet/ip.h>
#include <netinet/tcp.h>
#include <netinet/udp.h>
#include <netinet/ip_icmp.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <sys/time.h>
#include <sys/ioctl.h>
#include <sys/epoll.h>
#include <netdb.h>
#include <pthread.h>
#include <ctype.h>
#include <fcntl.h>
#include <ifaddrs.h>
#include <net/if.h>

#define MAX_THREADS 500
#define MAX_PORTS 65535
#define MAX_HOSTS 256
#define BANNER_SIZE 4096
#define DEFAULT_TIMEOUT 2000
#define MAX_PACKET_SIZE 65536
#define EPOLL_MAX_EVENTS 1024

typedef enum {
    PORT_OPEN,
    PORT_CLOSED,
    PORT_FILTERED,
    PORT_OPEN_FILTERED,
    PORT_UNFILTERED
} port_state_t;

typedef enum {
    SCAN_CONNECT,
    SCAN_SYN,
    SCAN_FIN,
    SCAN_XMAS,
    SCAN_NULL,
    SCAN_ACK,
    SCAN_UDP,
    SCAN_WINDOW,
    SCAN_MAIMON
} scan_type_t;

typedef enum {
    FORMAT_NORMAL,
    FORMAT_XML,
    FORMAT_JSON,
    FORMAT_GREPEABLE
} output_format_t;

typedef struct {
    char ip[INET_ADDRSTRLEN];
    int port;
    port_state_t state;
    char service[64];
    char version[256];
    char banner[BANNER_SIZE];
    double response_time;
} scan_result_t;

typedef struct {
    char target[256];
    int port_start;
    int port_end;
    int thread_id;
    scan_type_t scan_type;
    int timeout;
    int rate_limit;
} scan_task_t;

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
} tcp_packet_info_t;

extern volatile int running;
extern int verbose_mode;

#endif
