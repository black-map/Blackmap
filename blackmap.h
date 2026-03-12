#ifndef BLACKSCAN_H
#define BLACKSCAN_H

#define _DEFAULT_SOURCE

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
#include <netdb.h>
#include <pthread.h>
#include <ctype.h>
#include <fcntl.h>
#include <ifaddrs.h>

#define MAX_THREADS 500
#define DEFAULT_TIMEOUT 2000
#define BANNER_SIZE 4096
#define MAX_HOSTS 256
#define MAX_PORTS 65535
#define SERVICE_DB_SIZE 500

typedef enum {
    SCAN_CONNECT,
    SCAN_SYN,
    SCAN_FIN,
    SCAN_XMAS,
    SCAN_NULL,
    SCAN_UDP,
    SCAN_ACK,
    SCAN_WINDOW,
    SCAN_MAIMON
} scan_type_t;

typedef enum {
    PORT_OPEN,
    PORT_CLOSED,
    PORT_FILTERED,
    PORT_OPEN_FILTERED,
    PORT_UNFILTERED
} port_state_t;

typedef enum {
    FORMAT_NORMAL,
    FORMAT_XML,
    FORMAT_JSON,
    FORMAT_GREPEABLE,
    FORMAT_NMAP
} output_format_t;

typedef struct {
    int port;
    char service[64];
    char version[256];
    char product[128];
    char extra_info[512];
} service_info_t;

typedef struct {
    char os_name[128];
    char os_family[64];
    char os_gen[32];
    float accuracy;
    int cpe;
} os_info_t;

typedef struct {
    char ip[INET_ADDRSTRLEN];
    int port;
    port_state_t state;
    service_info_t service;
    char banner[BANNER_SIZE];
    double response_time;
} scan_result_t;

typedef struct {
    char target[256];
    int port_start;
    int port_end;
    int thread_id;
} scan_task_t;

typedef struct {
    int window_size;
    int ttl;
    int dont_frag;
    int tcp_options;
    short tcp_mss;
    short tcp_window_scale;
    char tcp_timestamp;
    char tcp_sack_ok;
    char tcp_nop;
} tcp_fingerprint_t;

static const char *service_db[][5] = {
    {"21", "ftp", "FTP", "vsftpd|proftpd|pure-ftpd|FileZilla", "220"},
    {"22", "ssh", "SSH", "OpenSSH|dropbear|PuTTY|WinSSHD", "SSH"},
    {"23", "telnet", "Telnet", "Linux telnetd|cisco|MikroTik", "\xff"},
    {"25", "smtp", "SMTP", "Postfix|Exim|Sendmail|Exchange", "220"},
    {"53", "domain", "DNS", "BIND|dnsmasq|Microsoft DNS", ""},
    {"80", "http", "HTTP", "Apache|Nginx|lighttpd|IIS", "HTTP/1"},
    {"110", "pop3", "POP3", "Dovecot|courier|Exchange", "+OK"},
    {"111", "rpcbind", "RPC", "rpcbind", ""},
    {"135", "msrpc", "MSRPC", "Microsoft RPC", ""},
    {"139", "netbios-ssn", "NetBIOS", "Samba|microsoft", ""},
    {"143", "imap", "IMAP", "Dovecot|courier|Exchange", "* OK"},
    {"443", "https", "HTTPS", "Nginx|Apache|ISS|Cloudflare", "HTTP/1"},
    {"445", "microsoft-ds", "SMB", "Samba|microsoft-ds", ""},
    {"465", "smtps", "SMTPS", "Postfix|Exim", "220"},
    {"514", "shell", "Syslog", "Linux syslogd", ""},
    {"587", "submission", "SMTP", "Postfix|Exim", "220"},
    {"993", "imaps", "IMAPS", "Dovecot|courier", "* OK"},
    {"995", "pop3s", "POP3S", "Dovecot|courier", "+OK"},
    {"1433", "ms-sql-s", "MSSQL", "Microsoft SQL Server", ""},
    {"1521", "oracle", "Oracle", "Oracle DB", ""},
    {"1723", "pptp", "PPTP", "Microsoft|mikrotik", ""},
    {"2049", "nfs", "NFS", "Linux nfsd|Sun", ""},
    {"3306", "mysql", "MySQL", "MySQL|MariaDB", "5\\."},
    {"3389", "ms-wbt-server", "RDP", "Microsoft Terminal Services", ""},
    {"5432", "postgresql", "PostgreSQL", "PostgreSQL", "5[0-9]\\."},
    {"5900", "vnc", "VNC", "RealVNC|TightVNC|UltraVNC", "RFB"},
    {"5985", "http", "WinRM", "Microsoft WinRM", "HTTP/1"},
    {"5986", "https", "WinRM", "Microsoft WinRM", "HTTP/1"},
    {"6379", "redis", "Redis", "Redis", "+PONG"},
    {"8080", "http-proxy", "HTTP", "Nginx|Apache|Jetty|Tomcat", "HTTP/1"},
    {"8443", "https-alt", "HTTPS", "Nginx|Apache|API Gateway", "HTTP/1"},
    {"9200", "http", "Elasticsearch", "Elasticsearch", ""},
    {"27017", "mongodb", "MongoDB", "MongoDB", "MongoDB"},
    {"0", "", "", "", ""}
};

static const char *os_fingerprints[][8] = {
    {"Linux", "Linux", "2.6.x", "64", "64", "255", "0", "0"},
    {"Linux", "Linux", "3.x", "64", "64", "255", "0", "0"},
    {"Linux", "Linux", "4.x", "64", "64", "255", "0", "0"},
    {"Linux", "Linux", "5.x", "64", "64", "255", "0", "0"},
    {"Windows", "Windows", "XP", "32", "8192", "0", "1", "1"},
    {"Windows", "Windows", "7", "32", "8192", "0", "1", "1"},
    {"Windows", "Windows", "10", "32", "65535", "0", "1", "1"},
    {"Windows", "Windows", "Server 2008", "32", "8192", "0", "1", "1"},
    {"Windows", "Windows", "Server 2012", "32", "65535", "0", "1", "1"},
    {"Windows", "Windows", "Server 2016", "32", "65535", "0", "1", "1"},
    {"FreeBSD", "FreeBSD", "9.x", "64", "65535", "0", "1", "0"},
    {"FreeBSD", "FreeBSD", "10.x", "64", "65535", "0", "1", "0"},
    {"OpenBSD", "OpenBSD", "6.x", "64", "16384", "0", "1", "0"},
    {"NetBSD", "NetBSD", "6.x", "64", "16384", "0", "1", "0"},
    {"Mac OS X", "macOS", "10.x", "64", "65535", "0", "1", "1"},
    {"Mac OS X", "macOS", "11.x", "64", "65535", "0", "1", "1"},
    {"Solaris", "Solaris", "10", "32", "32768", "0", "1", "0"},
    {"Solaris", "Solaris", "11", "32", "32768", "0", "1", "0"},
    {"RouterOS", "MikroTik", "6.x", "32", "65535", "64", "1", "0"},
    {"iOS", "iOS", "12.x", "64", "65535", "0", "1", "1"},
    {"Android", "Android", "9.x", "64", "65535", "0", "1", "1"},
    {"", "", "", "", "", "", "", ""}
};

volatile int running = 1;
int global_thread_count = 0;
int verbose_mode = 0;
pthread_mutex_t results_mutex = PTHREAD_MUTEX_INITIALIZER;
scan_result_t results[MAX_PORTS];
int result_count = 0;

void parse_ports(const char *port_str, int **ports, int *port_count);
const char* get_service_name(int port);
void detect_service_version(const char *ip, int port, int timeout, service_info_t *info);
void detect_os_fingerprint(const char *ip, int timeout, os_info_t *os_info);
void print_nmap_output(const char *ip, scan_result_t *results, int count, os_info_t *os_info);
void* scan_worker(void *arg);
void random_delay(int base_ms, int variance);
char* trim(char *str);
void build_tcp_packet(struct tcphdr *tcp, int src_port, int dst_port, int seq, int ack, int flags, int window);

#endif
