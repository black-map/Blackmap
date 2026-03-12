#include "blackmap.h"
#include <getopt.h>

double get_timestamp_ms() {
    struct timeval tv;
    gettimeofday(&tv, NULL);
    return (double)tv.tv_sec * 1000.0 + (double)tv.tv_usec / 1000.0;
}

void signal_handler(int sig) {
    (void)sig;
    running = 0;
}

char* trim(char *str) {
    char *end;
    while(isspace((unsigned char)*str)) str++;
    if(*str == 0) return str;
    end = str + strlen(str) - 1;
    while(end > str && isspace((unsigned char)*end)) end--;
    end[1] = '\0';
    return str;
}

void random_delay(int base_ms, int variance) {
    if (variance <= 0) return;
    int delay = base_ms + (rand() % variance);
    usleep(delay * 1000);
}

void build_tcp_packet(struct tcphdr *tcp, int src_port, int dst_port, int seq, int ack, int flags, int window) {
    memset(tcp, 0, sizeof(struct tcphdr));
    tcp->th_sport = htons(src_port);
    tcp->th_dport = htons(dst_port);
    tcp->th_seq = htonl(seq);
    tcp->th_ack = htonl(ack);
    tcp->th_x2 = 0;
    tcp->th_off = 5;
    tcp->th_flags = flags;
    tcp->th_win = htons(window);
    tcp->th_urp = 0;
}

void usage(const char *prog) {
    printf("Blackmap v1.2 - Advanced Network Scanner\n");
    printf("==========================================\n\n");
    printf("Usage: %s [OPTIONS] <target>\n\n", prog);
    printf("OPTIONS:\n");
    printf("  -p <ports>       Ports (e.g., 22,80,443 or 1-1000 or all)\n");
    printf("  -s <type>        Scan type: connect|syn|fin|xmas|null|udp|ack|window|maimon\n");
    printf("  -T <timeout>     Timeout in ms (default: 2000)\n");
    printf("  -T <1-10>        Timing template (T1-T5)\n");
    printf("  -c <threads>     Concurrent threads (default: 50, max: 500)\n");
    printf("  -b               Banner grabbing\n");
    printf("  -sV              Service version detection\n");
    printf("  -O               OS detection (requires root)\n");
    printf("  -A               Enable all detections (OS, version, script)\n");
    printf("  -oN <file>       Normal output\n");
    printf("  -oX <file>       XML output\n");
    printf("  -oJ <file>       JSON output\n");
    printf("  -oG <file>       Grepable output\n");
    printf("  -D <decoy>       Decoy IPs (comma separated)\n");
    printf("  --randomize-hosts  Randomize host scan order\n");
    printf("  --scan-delay     Delay between probes (ms)\n");
    printf("  --source-port    Source port for scan\n");
    printf("  --source-ip      Source IP for scan\n");
    printf("  -v               Verbose mode\n");
    printf("  -vv              Very verbose\n");
    printf("  -h               Help\n\n");
    printf("EXAMPLES:\n");
    printf("  %s 192.168.1.1 -p 1-1000 -sS -sV -O -vv\n", prog);
    printf("  %s example.com -p 22,80,443 -sV -A\n", prog);
    printf("  %s 10.0.0.0/24 -p 22,80,443 -sV -oN scan.txt\n", prog);
}

void parse_ports(const char *port_str, int **ports, int *port_count) {
    int *ports_arr = NULL;
    int capacity = 1024;
    int count = 0;

    ports_arr = malloc(capacity * sizeof(int));
    if (!ports_arr) {
        perror("malloc");
        exit(1);
    }

    if (strcmp(port_str, "all") == 0 || strcmp(port_str, "-") == 0) {
        for (int p = 1; p <= 1024 && count < 65535; p++) {
            if (count >= capacity) {
                capacity *= 2;
                ports_arr = realloc(ports_arr, capacity * sizeof(int));
            }
            ports_arr[count++] = p;
        }
        for (int p = 1433; p <= 3306 && count < 65535; p += 1373) {
            if (count >= capacity) {
                capacity *= 2;
                ports_arr = realloc(ports_arr, capacity * sizeof(int));
            }
            ports_arr[count++] = p;
        }
        for (int p = 5000; p <= 10000 && count < 65535; p += 5000) {
            if (count >= capacity) {
                capacity *= 2;
                ports_arr = realloc(ports_arr, capacity * sizeof(int));
            }
            ports_arr[count++] = p;
        }
    } else {
        char *str = strdup(port_str);
        char *token = strtok(str, ",");

        while (token) {
            if (strchr(token, '-')) {
                int start, end;
                sscanf(token, "%d-%d", &start, &end);
                start = (start < 1) ? 1 : start;
                end = (end > 65535) ? 65535 : end;
                for (int p = start; p <= end && count < 65535; p++) {
                    if (count >= capacity) {
                        capacity *= 2;
                        ports_arr = realloc(ports_arr, capacity * sizeof(int));
                    }
                    ports_arr[count++] = p;
                }
            } else {
                int port = atoi(token);
                if (port > 0 && port <= 65535) {
                    if (count >= capacity) {
                        capacity *= 2;
                        ports_arr = realloc(ports_arr, capacity * sizeof(int));
                    }
                    ports_arr[count++] = port;
                }
            }
            token = strtok(NULL, ",");
        }
        free(str);
    }

    *ports = ports_arr;
    *port_count = count;
}

const char* get_service_name(int port) {
    for (int i = 0; service_db[i][0][0] != '0'; i++) {
        if (atoi(service_db[i][0]) == port) {
            return service_db[i][1];
        }
    }
    return "unknown";
}

void detect_service_version(const char *ip, int port, int timeout, service_info_t *info) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return;

    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(sock);
        return;
    }

    char buffer[BANNER_SIZE] = {0};
    ssize_t n = 0;

    if (port == 80 || port == 8080 || port == 8000 || port == 8888) {
        const char *http_req = "HEAD / HTTP/1.0\r\nHost: localhost\r\nUser-Agent: Blackscan/1.2\r\n\r\n";
        send(sock, http_req, strlen(http_req), 0);
        n = recv(sock, buffer, BANNER_SIZE - 1, 0);
    } else if (port == 443 || port == 8443 || port == 9443) {
        const char *https_req = "HEAD / HTTP/1.0\r\nHost: localhost\r\nUser-Agent: Blackscan/1.2\r\n\r\n";
        send(sock, https_req, strlen(https_req), 0);
        n = recv(sock, buffer, BANNER_SIZE - 1, 0);
    } else if (port == 21) {
        n = recv(sock, buffer, BANNER_SIZE - 1, 0);
    } else if (port == 22) {
        n = recv(sock, buffer, BANNER_SIZE - 1, 0);
    } else if (port == 25 || port == 587 || port == 465) {
        const char *smtp_req = "EHLO localhost\r\n";
        send(sock, smtp_req, strlen(smtp_req), 0);
        n = recv(sock, buffer, BANNER_SIZE - 1, 0);
    } else if (port == 110) {
        const char *pop3_req = "CAPA\r\n";
        send(sock, pop3_req, strlen(pop3_req), 0);
        n = recv(sock, buffer, BANNER_SIZE - 1, 0);
    } else if (port == 143) {
        const char *imap_req = "A001 CAPABILITY\r\n";
        send(sock, imap_req, strlen(imap_req), 0);
        n = recv(sock, buffer, BANNER_SIZE - 1, 0);
    } else if (port == 3306) {
        const char *mysql_req = "\x00\x00\x00\x01\x85\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
        send(sock, mysql_req, 24, 0);
        n = recv(sock, buffer, BANNER_SIZE - 1, 0);
    } else if (port == 6379) {
        const char *redis_ping = "*1\r\n$4\r\nPING\r\n";
        send(sock, redis_ping, strlen(redis_ping), 0);
        n = recv(sock, buffer, BANNER_SIZE - 1, 0);
    } else if (port == 5432) {
        const char *postgres_req = "\x00\x00\x00\x08\x04\xD2\x16\x2F";
        send(sock, postgres_req, 8, 0);
        n = recv(sock, buffer, BANNER_SIZE - 1, 0);
    } else if (port == 27017) {
        const char *mongo_req = "\x16\x00\x00\x00\x10\x00\x00\x00\x02\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xff\xff\xff\xff";
        send(sock, mongo_req, 16, 0);
        n = recv(sock, buffer, BANNER_SIZE - 1, 0);
    } else {
        n = recv(sock, buffer, BANNER_SIZE - 1, 0);
    }

    close(sock);

    if (n > 0) {
        buffer[n] = '\0';
        for (int i = 0; i < n && i < BANNER_SIZE - 1; i++) {
            if (!isprint((unsigned char)buffer[i]) && buffer[i] != '\n' && buffer[i] != '\r' && buffer[i] != '\t') {
                buffer[i] = '.';
            }
        }
        strncpy(info->version, trim(buffer), 255);
        info->version[255] = '\0';

        for (int i = 0; service_db[i][0][0] != '0'; i++) {
            if (atoi(service_db[i][0]) == port) {
                strncpy(info->service, service_db[i][2], 63);
                strncpy(info->product, service_db[i][3], 127);
                if (strlen(info->version) > 0) {
                    snprintf(info->extra_info, 511, "%s", info->version);
                } else {
                    snprintf(info->extra_info, 511, "%s", service_db[i][3]);
                }
                return;
            }
        }
    }
}

void detect_os_fingerprint(const char *ip, int timeout, os_info_t *os_info) {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock < 0) {
        if (verbose_mode) fprintf(stderr, "[*] OS detection requires root\n");
        strcpy(os_info->os_name, "Unknown");
        strcpy(os_info->os_family, "Unknown");
        strcpy(os_info->os_gen, "Unknown");
        os_info->accuracy = 0;
        return;
    }

    int on = 1;
    setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));

    char packet[128];
    struct iphdr *ip_hdr = (struct iphdr *)packet;
    struct tcphdr *tcp_hdr = (struct tcphdr *)(packet + sizeof(struct iphdr));

    memset(packet, 0, sizeof(packet));
    ip_hdr->ihl = 5;
    ip_hdr->version = 4;
    ip_hdr->tos = 0;
    ip_hdr->tot_len = htons(60);
    ip_hdr->id = htons(rand() % 65535);
    ip_hdr->frag_off = 0;
    ip_hdr->ttl = 64;
    ip_hdr->protocol = IPPROTO_TCP;
    ip_hdr->check = 0;
    ip_hdr->saddr = inet_addr(ip);
    ip_hdr->daddr = inet_addr(ip);

    tcp_hdr->th_sport = htons(rand() % 65535);
    tcp_hdr->th_dport = htons(80);
    tcp_hdr->th_seq = htonl(rand());
    tcp_hdr->th_ack = 0;
    tcp_hdr->th_off = 5;
    tcp_hdr->th_flags = TH_SYN;
    tcp_hdr->th_win = htons(65535);
    tcp_hdr->th_urp = 0;

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr(ip);

    double start = get_timestamp_ms();
    sendto(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dest, sizeof(dest));

    char response[256];
    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int n = recv(sock, response, sizeof(response), 0);
    double elapsed = get_timestamp_ms() - start;

    close(sock);

    if (n > 0) {
        struct iphdr *resp_ip = (struct iphdr *)response;
        struct tcphdr *resp_tcp = (struct tcphdr *)(response + resp_ip->ihl * 4);

        int window = ntohs(resp_tcp->th_win);
        int ttl = resp_ip->ttl;

        float best_score = 0;
        int best_match = -1;

        for (int i = 0; os_fingerprints[i][0][0] != '\0'; i++) {
            int fp_window = atoi(os_fingerprints[i][4]);
            int fp_ttl = atoi(os_fingerprints[i][5]);

            float score = 1.0;
            if (abs(window - fp_window) > 1000) score -= 0.3;
            if (abs(ttl - fp_ttl) > 10) score -= 0.3;

            if (score > best_score) {
                best_score = score;
                best_match = i;
            }
        }

        if (best_match >= 0) {
            strncpy(os_info->os_name, os_fingerprints[best_match][0], 127);
            strncpy(os_info->os_family, os_fingerprints[best_match][1], 63);
            strncpy(os_info->os_gen, os_fingerprints[best_match][2], 31);
            os_info->accuracy = best_score * 100;
        }
    } else {
        strcpy(os_info->os_name, "Unknown");
        strcpy(os_info->os_family, "Unknown");
        strcpy(os_info->os_gen, "Unknown");
        os_info->accuracy = 0;
    }
}

port_state_t tcp_connect_scan(const char *ip, int port, int timeout) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return PORT_FILTERED;

    int flags = fcntl(sock, F_GETFL, 0);
    fcntl(sock, F_SETFL, flags | O_NONBLOCK);

    struct sockaddr_in addr;
    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    inet_pton(AF_INET, ip, &addr.sin_addr);

    double start = get_timestamp_ms();
    connect(sock, (struct sockaddr *)&addr, sizeof(addr));

    fd_set write_fds;
    FD_ZERO(&write_fds);
    FD_SET(sock, &write_fds);

    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;

    int sel = select(sock + 1, NULL, &write_fds, NULL, &tv);
    double elapsed = get_timestamp_ms() - start;

    close(sock);

    if (sel > 0) return PORT_OPEN;
    if (elapsed >= timeout) return PORT_FILTERED;
    return PORT_CLOSED;
}

port_state_t syn_scan(const char *ip, int port, int timeout) {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock < 0) return tcp_connect_scan(ip, port, timeout);

    int on = 1;
    setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));

    char packet[64];
    struct iphdr *ip_hdr = (struct iphdr *)packet;
    struct tcphdr *tcp_hdr = (struct tcphdr *)(packet + sizeof(struct iphdr));

    memset(packet, 0, sizeof(packet));
    ip_hdr->ihl = 5;
    ip_hdr->version = 4;
    ip_hdr->tos = 0;
    ip_hdr->tot_len = htons(40);
    ip_hdr->id = htons(rand() % 65535);
    ip_hdr->frag_off = 0;
    ip_hdr->ttl = 64;
    ip_hdr->protocol = IPPROTO_TCP;
    ip_hdr->saddr = rand();
    ip_hdr->daddr = inet_addr(ip);

    tcp_hdr->th_sport = htons(rand() % 65535);
    tcp_hdr->th_dport = htons(port);
    tcp_hdr->th_seq = htonl(rand());
    tcp_hdr->th_ack = 0;
    tcp_hdr->th_off = 5;
    tcp_hdr->th_flags = TH_SYN;
    tcp_hdr->th_win = htons(65535);

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr(ip);
    dest.sin_port = htons(port);

    sendto(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dest, sizeof(dest));

    char response[128];
    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    struct sockaddr_in from;
    socklen_t from_len = sizeof(from);

    int n = recvfrom(sock, response, sizeof(response), 0, (struct sockaddr *)&from, &from_len);
    close(sock);

    if (n > 0) {
        struct tcphdr *resp = (struct tcphdr *)(response + 20);
        if (resp->th_flags & (TH_SYN | TH_ACK)) {
            return PORT_OPEN;
        } else if (resp->th_flags & TH_RST) {
            return PORT_CLOSED;
        }
    }

    return PORT_FILTERED;
}

port_state_t fin_scan(const char *ip, int port, int timeout) {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock < 0) return PORT_FILTERED;

    int on = 1;
    setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));

    char packet[64];
    struct iphdr *ip_hdr = (struct iphdr *)packet;
    struct tcphdr *tcp_hdr = (struct tcphdr *)(packet + sizeof(struct iphdr));

    memset(packet, 0, sizeof(packet));
    ip_hdr->ihl = 5;
    ip_hdr->version = 4;
    ip_hdr->ttl = 64;
    ip_hdr->protocol = IPPROTO_TCP;
    ip_hdr->saddr = rand();
    ip_hdr->daddr = inet_addr(ip);

    tcp_hdr->th_sport = htons(rand() % 65535);
    tcp_hdr->th_dport = htons(port);
    tcp_hdr->th_flags = TH_FIN;

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr(ip);

    sendto(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dest, sizeof(dest));

    char response[128];
    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int n = recv(sock, response, sizeof(response), 0);
    close(sock);

    if (n <= 0) return PORT_OPEN;
    return PORT_CLOSED;
}

port_state_t xmas_scan(const char *ip, int port, int timeout) {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock < 0) return PORT_FILTERED;

    int on = 1;
    setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));

    char packet[64];
    struct iphdr *ip_hdr = (struct iphdr *)packet;
    struct tcphdr *tcp_hdr = (struct tcphdr *)(packet + sizeof(struct iphdr));

    memset(packet, 0, sizeof(packet));
    ip_hdr->ihl = 5;
    ip_hdr->version = 4;
    ip_hdr->ttl = 64;
    ip_hdr->protocol = IPPROTO_TCP;
    ip_hdr->saddr = rand();
    ip_hdr->daddr = inet_addr(ip);

    tcp_hdr->th_sport = htons(rand() % 65535);
    tcp_hdr->th_dport = htons(port);
    tcp_hdr->th_flags = TH_FIN | TH_PUSH | TH_URG;

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr(ip);

    sendto(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dest, sizeof(dest));

    char response[128];
    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int n = recv(sock, response, sizeof(response), 0);
    close(sock);

    if (n <= 0) return PORT_OPEN;
    return PORT_CLOSED;
}

port_state_t null_scan(const char *ip, int port, int timeout) {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock < 0) return PORT_FILTERED;

    int on = 1;
    setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));

    char packet[64];
    struct iphdr *ip_hdr = (struct iphdr *)packet;
    struct tcphdr *tcp_hdr = (struct tcphdr *)(packet + sizeof(struct iphdr));

    memset(packet, 0, sizeof(packet));
    ip_hdr->ihl = 5;
    ip_hdr->version = 4;
    ip_hdr->ttl = 64;
    ip_hdr->protocol = IPPROTO_TCP;
    ip_hdr->saddr = rand();
    ip_hdr->daddr = inet_addr(ip);

    tcp_hdr->th_sport = htons(rand() % 65535);
    tcp_hdr->th_dport = htons(port);
    tcp_hdr->th_flags = 0;

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr(ip);

    sendto(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dest, sizeof(dest));

    char response[128];
    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int n = recv(sock, response, sizeof(response), 0);
    close(sock);

    if (n <= 0) return PORT_OPEN;
    return PORT_CLOSED;
}

port_state_t ack_scan(const char *ip, int port, int timeout) {
    int sock = socket(AF_INET, SOCK_RAW, IPPROTO_TCP);
    if (sock < 0) return PORT_FILTERED;

    int on = 1;
    setsockopt(sock, IPPROTO_IP, IP_HDRINCL, &on, sizeof(on));

    char packet[64];
    struct iphdr *ip_hdr = (struct iphdr *)packet;
    struct tcphdr *tcp_hdr = (struct tcphdr *)(packet + sizeof(struct iphdr));

    memset(packet, 0, sizeof(packet));
    ip_hdr->ihl = 5;
    ip_hdr->version = 4;
    ip_hdr->ttl = 64;
    ip_hdr->protocol = IPPROTO_TCP;
    ip_hdr->saddr = rand();
    ip_hdr->daddr = inet_addr(ip);

    tcp_hdr->th_sport = htons(rand() % 65535);
    tcp_hdr->th_dport = htons(port);
    tcp_hdr->th_flags = TH_ACK;

    struct sockaddr_in dest;
    dest.sin_family = AF_INET;
    dest.sin_addr.s_addr = inet_addr(ip);

    sendto(sock, packet, sizeof(packet), 0, (struct sockaddr *)&dest, sizeof(dest));

    char response[128];
    struct timeval tv;
    tv.tv_sec = timeout / 1000;
    tv.tv_usec = (timeout % 1000) * 1000;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    int n = recv(sock, response, sizeof(response), 0);
    close(sock);

    if (n > 0) {
        struct tcphdr *resp = (struct tcphdr *)(response + 20);
        if (resp->th_flags & TH_RST) return PORT_UNFILTERED;
    }

    return PORT_FILTERED;
}

port_state_t udp_scan(const char *ip, int port, int timeout) {
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
    inet_pton(AF_INET, ip, &addr.sin_addr);

    char probe[32] = {0};
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

void* scan_worker(void *arg) {
    scan_task_t *task = (scan_task_t *)arg;
    const char *ip = task->target;
    int port = task->port_start;

    scan_type_t scan_type = SCAN_CONNECT;
    int timeout = DEFAULT_TIMEOUT;

    if (task->port_end > 0) {
        timeout = task->port_end;
    }

    port_state_t state;
    if (scan_type == SCAN_SYN || scan_type == 0) {
        state = syn_scan(ip, port, timeout);
    } else if (scan_type == SCAN_FIN) {
        state = fin_scan(ip, port, timeout);
    } else if (scan_type == SCAN_XMAS) {
        state = xmas_scan(ip, port, timeout);
    } else if (scan_type == SCAN_NULL) {
        state = null_scan(ip, port, timeout);
    } else if (scan_type == SCAN_ACK) {
        state = ack_scan(ip, port, timeout);
    } else if (scan_type == SCAN_UDP) {
        state = udp_scan(ip, port, timeout);
    } else {
        state = tcp_connect_scan(ip, port, timeout);
    }

    scan_result_t result;
    memset(&result, 0, sizeof(result));
    strncpy(result.ip, ip, INET_ADDRSTRLEN - 1);
    result.port = port;
    result.state = state;
    result.response_time = 0;

    strncpy(result.service.service, get_service_name(port), 63);

    pthread_mutex_lock(&results_mutex);
    if (result_count < MAX_PORTS) {
        results[result_count++] = result;
    }
    pthread_mutex_unlock(&results_mutex);

    free(task);
    return NULL;
}

void print_nmap_output(const char *ip, scan_result_t *results, int count, os_info_t *os_info) {
    time_t now = time(NULL);
    char timestamp[64];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    printf("\n");
    printf("Nmap scan report for %s\n", ip);
    printf("Host is %s (%s latency).\n", 
           count > 0 ? "up" : "down", "0.0023s");
    printf("Not shown: %d closed tcp ports (conn-refused)\n", 1000 - count);
    printf("\n");
    printf("PORT      STATE SERVICE       VERSION\n");

    for (int i = 0; i < count; i++) {
        if (results[i].state == PORT_OPEN) {
            const char *state_str = "open";
            const char *service = results[i].service.service[0] ? 
                results[i].service.service : get_service_name(results[i].port);
            
            printf("%-9d/%-4s %-11s %s",
                   results[i].port, "tcp", state_str, service);
            
            if (results[i].service.version[0] != '\0') {
                printf(" %s", results[i].service.version);
            }
            printf("\n");
        }
    }

    printf("\n");
    if (os_info && os_info->os_name[0] != '\0' && strcmp(os_info->os_name, "Unknown") != 0) {
        printf("OS details: %s %s", os_info->os_family, os_info->os_gen);
        if (os_info->accuracy > 0) {
            printf(" (%.0f%%)", os_info->accuracy);
        }
        printf("\n");
    }

    printf("\n");
    printf("Blackscan done: 1 IP address scanned in %.2f seconds\n", 0.15);
    printf("\n");
}

void print_result(const char *ip, int port, port_state_t state, const char *banner, int format) {
    const char *state_str;
    switch (state) {
        case PORT_OPEN: state_str = "OPEN"; break;
        case PORT_CLOSED: state_str = "CLOSED"; break;
        case PORT_FILTERED: state_str = "FILTERED"; break;
        case PORT_OPEN_FILTERED: state_str = "OPEN|FILTERED"; break;
        case PORT_UNFILTERED: state_str = "UNFILTERED"; break;
        default: state_str = "UNKNOWN";
    }

    const char *service = get_service_name(port);

    if (format == FORMAT_NORMAL || format == FORMAT_NMAP) {
        printf("[%s] %d/tcp %s", state_str, port, service);
        if (banner && banner[0] != '\0') {
            printf(" | %s", banner);
        }
        printf("\n");
    } else if (format == FORMAT_JSON) {
        static int first = 1;
        if (!first) printf(",\n");
        first = 0;
        printf("  {\"port\": %d, \"protocol\": \"tcp\", \"state\": \"%s\", \"service\": \"%s\", \"version\": \"%s\"}",
               port, state_str, service, banner ? banner : "");
    } else if (format == FORMAT_GREPEABLE) {
        printf("PORT|%d/tcp|%s|%s", port, state_str, service);
        if (banner && banner[0] != '\0') {
            printf("|%s", banner);
        }
        printf("\n");
    } else if (format == FORMAT_XML) {
        printf("  <port><number>%d</number><protocol>tcp</protocol><state>%s</state><service>%s</service></port>\n",
               port, state_str, service);
    }
}

int main(int argc, char *argv[]) {
    char target[256] = {0};
    char *port_str = NULL;
    char output_file[256] = {0};
    scan_type_t scan_type = SCAN_CONNECT;
    int timeout = DEFAULT_TIMEOUT;
    int threads = 50;
    int verbose = 0;
    int version_detect = 0;
    int os_detect = 0;
    int output_format = FORMAT_NORMAL;
    int timing = 3;
    int scan_delay = 0;
    int source_port = 0;
    char source_ip[64] = {0};
    int randomize_hosts = 0;
    char *decoys = NULL;

    int opt;

    while ((opt = getopt(argc, argv, "p:s:T:c:bVSo:f:h?vAO")) != -1) {
        switch (opt) {
            case 'p':
                port_str = optarg;
                break;
            case 'T':
                if (strlen(optarg) == 1 && optarg[0] >= '1' && optarg[0] <= '5') {
                    timing = atoi(optarg);
                    switch (timing) {
                        case 1: timeout = 30000; threads = 5; scan_delay = 15000; break;
                        case 2: timeout = 15000; threads = 10; scan_delay = 5000; break;
                        case 3: timeout = 8000; threads = 50; scan_delay = 1000; break;
                        case 4: timeout = 4000; threads = 150; scan_delay = 250; break;
                        case 5: timeout = 2000; threads = 300; scan_delay = 0; break;
                    }
                } else {
                    timeout = atoi(optarg);
                }
                break;
            case 'c':
                threads = atoi(optarg);
                if (threads > MAX_THREADS) threads = MAX_THREADS;
                break;
            case 'b':
                version_detect = 1;
                break;
            case 's':
                if (strcmp(optarg, "V") == 0) {
                    version_detect = 1;
                } else if (strcmp(optarg, "syn") == 0 || strcmp(optarg, "S") == 0) {
                    scan_type = SCAN_SYN;
                } else if (strcmp(optarg, "fin") == 0 || strcmp(optarg, "F") == 0) {
                    scan_type = SCAN_FIN;
                } else if (strcmp(optarg, "xmas") == 0 || strcmp(optarg, "X") == 0) {
                    scan_type = SCAN_XMAS;
                } else if (strcmp(optarg, "null") == 0 || strcmp(optarg, "N") == 0) {
                    scan_type = SCAN_NULL;
                } else if (strcmp(optarg, "udp") == 0 || strcmp(optarg, "U") == 0) {
                    scan_type = SCAN_UDP;
                } else if (strcmp(optarg, "ack") == 0) {
                    scan_type = SCAN_ACK;
                } else if (strcmp(optarg, "window") == 0) {
                    scan_type = SCAN_WINDOW;
                } else if (strcmp(optarg, "maimon") == 0) {
                    scan_type = SCAN_MAIMON;
                } else {
                    scan_type = SCAN_CONNECT;
                }
                break;
            case 'o':
                if (optarg[0] == 'N' || optarg[0] == 'n') {
                    output_format = FORMAT_NORMAL;
                    strncpy(output_file, optarg + 2, 255);
                } else if (optarg[0] == 'X' || optarg[0] == 'x') {
                    output_format = FORMAT_XML;
                    strncpy(output_file, optarg + 2, 255);
                } else if (optarg[0] == 'J' || optarg[0] == 'j') {
                    output_format = FORMAT_JSON;
                    strncpy(output_file, optarg + 2, 255);
                } else if (optarg[0] == 'G' || optarg[0] == 'g') {
                    output_format = FORMAT_GREPEABLE;
                    strncpy(output_file, optarg + 2, 255);
                }
                break;
            case 'O':
                os_detect = 1;
                break;
            case 'A':
                os_detect = 1;
                version_detect = 1;
                break;
            case 'v':
                verbose++;
                verbose_mode = verbose;
                break;
            case 1001:
                randomize_hosts = 1;
                break;
            case 1002:
                scan_delay = atoi(optarg);
                break;
            case 1003:
                source_port = atoi(optarg);
                break;
            case 1004:
                strncpy(source_ip, optarg, 63);
                break;
            case 'h':
            case '?':
            default:
                usage(argv[0]);
                return opt == 'h' ? 0 : 1;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Error: Target is required\n\n");
        usage(argv[0]);
        return 1;
    }
    strncpy(target, argv[optind], 255);

    if (port_str == NULL) {
        fprintf(stderr, "Error: Ports are required\n\n");
        usage(argv[0]);
        return 1;
    }

    if (scan_type != SCAN_CONNECT && geteuid() != 0) {
        fprintf(stderr, "Warning: Raw scans require root privileges\n");
        scan_type = SCAN_CONNECT;
    }

    signal(SIGINT, signal_handler);
    signal(SIGPIPE, SIG_IGN);
    srand(time(NULL));

    struct in_addr addr;
    char ip[INET_ADDRSTRLEN];
    if (inet_pton(AF_INET, target, &addr) == 1) {
        inet_ntop(AF_INET, &addr, ip, INET_ADDRSTRLEN);
    } else {
        struct hostent *he = gethostbyname(target);
        if (!he) {
            fprintf(stderr, "Error: Could not resolve %s\n", target);
            return 1;
        }
        struct in_addr **addr_list = (struct in_addr **)he->h_addr_list;
        inet_ntop(AF_INET, addr_list[0], ip, INET_ADDRSTRLEN);
    }
    ip[INET_ADDRSTRLEN - 1] = '\0';

    if (verbose) {
        printf("[*] Blackmap v1.2\n");
        printf("[*] Target: %s (%s)\n", target, ip);
        printf("[*] Scan Type: %d\n", scan_type);
        printf("[*] Timing: T%d (timeout: %d ms, threads: %d)\n", timing, timeout, threads);
    }

    int *ports = NULL;
    int port_count = 0;
    parse_ports(port_str, &ports, &port_count);

    if (verbose) {
        printf("[*] Ports to scan: %d\n", port_count);
    }

    if (randomize_hosts) {
        for (int i = 0; i < port_count; i++) {
            int j = rand() % port_count;
            int temp = ports[i];
            ports[i] = ports[j];
            ports[j] = temp;
        }
    }

    FILE *output_fp = stdout;
    int file_output = 0;
    if (output_file[0] != '\0') {
        output_fp = fopen(output_file, "w");
        file_output = 1;
    }

    if (output_format == FORMAT_JSON) {
        fprintf(output_fp, "{\n");
        fprintf(output_fp, "  \"scanner\": \"blackmap\",\n");
        fprintf(output_fp, "  \"version\": \"1.2\",\n");
        fprintf(output_fp, "  \"target\": \"%s\",\n", ip);
        fprintf(output_fp, "  \"scan_type\": \"%d\",\n", scan_type);
        fprintf(output_fp, "  \"results\": [\n");
    } else if (output_format == FORMAT_XML) {
        fprintf(output_fp, "<?xml version=\"1.0\" encoding=\"UTF-8\"?>\n");
        fprintf(output_fp, "<nmaprun scanner=\"blackmap\" version=\"1.2\" args=\"%s", argv[0]);
        for (int i = 1; i < argc; i++) fprintf(output_fp, " %s", argv[i]);
        fprintf(output_fp, "\">\n");
        fprintf(output_fp, "<host><address addr=\"%s\" addrtype=\"ipv4\"/></host>\n", ip);
        fprintf(output_fp, "<ports>\n");
    }

    pthread_t scan_threads[MAX_THREADS];
    int thread_idx = 0;
    int open_count = 0;

    double scan_start = get_timestamp_ms();

    for (int i = 0; i < port_count && running; i++) {
        if (verbose > 1) {
            printf("[*] Scanning port %d/%d (%d)...\n", i + 1, port_count, ports[i]);
        }

        scan_task_t *task = malloc(sizeof(scan_task_t));
        strncpy(task->target, ip, 255);
        task->port_start = ports[i];
        task->port_end = timeout;
        task->thread_id = i;

        pthread_create(&scan_threads[thread_idx], NULL, scan_worker, task);
        thread_idx++;

        if (thread_idx >= threads || i == port_count - 1) {
            for (int j = 0; j < thread_idx; j++) {
                pthread_join(scan_threads[j], NULL);
            }
            thread_idx = 0;
        }

        if (scan_delay > 0) {
            usleep(scan_delay * 1000);
        }
    }

    double scan_end = get_timestamp_ms();
    double scan_duration = (scan_end - scan_start) / 1000.0;

    for (int i = 0; i < result_count; i++) {
        if (results[i].state == PORT_OPEN) {
            open_count++;
            if (version_detect) {
                detect_service_version(ip, results[i].port, timeout, &results[i].service);
            }
        }
    }

    os_info_t os_info;
    memset(&os_info, 0, sizeof(os_info));
    if (os_detect) {
        if (verbose) printf("[*] Performing OS detection...\n");
        detect_os_fingerprint(ip, timeout, &os_info);
    }

    if (output_format == FORMAT_NMAP || (output_format == FORMAT_NORMAL && verbose)) {
        print_nmap_output(ip, results, result_count, &os_info);
    } else {
        for (int i = 0; i < result_count; i++) {
            if (results[i].state == PORT_OPEN) {
                print_result(ip, results[i].port, results[i].state, 
                           results[i].service.version, output_format);
            }
        }
    }

    if (output_format == FORMAT_JSON) {
        fprintf(output_fp, "\n  ],\n");
        fprintf(output_fp, "  \"open_ports\": %d,\n", open_count);
        fprintf(output_fp, "  \"scan_duration\": %.2f,\n", scan_duration);
        if (os_detect && strcmp(os_info.os_name, "Unknown") != 0) {
            fprintf(output_fp, "  \"os\": {\"name\": \"%s\", \"family\": \"%s\", \"generation\": \"%s\", \"accuracy\": %.0f}\n", 
                   os_info.os_name, os_info.os_family, os_info.os_gen, os_info.accuracy);
        }
        fprintf(output_fp, "}\n");
    } else if (output_format == FORMAT_XML) {
        for (int i = 0; i < result_count; i++) {
            if (results[i].state == PORT_OPEN) {
                fprintf(output_fp, "  <port><protocol>tcp</protocol><portid>%d</portid><state state=\"open\"/></port>\n",
                       results[i].port);
            }
        }
        fprintf(output_fp, "</ports>\n</nmaprun>\n");
    }

    free(ports);

    if (file_output && output_fp != stdout) {
        fclose(output_fp);
    }

    if (verbose) {
        printf("[*] Scan completed. Open ports: %d\n", open_count);
        printf("[*] Scan duration: %.2f seconds\n", scan_duration);
    }

    return 0;
}
