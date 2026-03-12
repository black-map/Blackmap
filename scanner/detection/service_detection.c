#include "../include/service_detection.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <ctype.h>

static const service_signature_t service_signatures[] = {
    {21, "ftp", "vsftpd|proftpd|pure-ftpd", "220"},
    {22, "ssh", "OpenSSH|dropbear", "SSH-"},
    {23, "telnet", "Linux telnetd", ""},
    {25, "smtp", "Postfix|Exim|Sendmail", "220"},
    {53, "dns", "BIND|dnsmasq", ""},
    {80, "http", "Apache|Nginx|lighttpd|IIS", "HTTP"},
    {110, "pop3", "Dovecot|courier", "+OK"},
    {111, "rpcbind", "rpcbind", ""},
    {135, "msrpc", "Microsoft RPC", ""},
    {139, "netbios-ssn", "Samba|microsoft", ""},
    {143, "imap", "Dovecot|courier", "* OK"},
    {443, "https", "Nginx|Apache|ISS|Cloudflare", "HTTP"},
    {445, "microsoft-ds", "Samba|microsoft-ds", ""},
    {465, "smtps", "Postfix|Exim", "220"},
    {514, "shell", "Linux syslogd", ""},
    {587, "submission", "Postfix|Exim", "220"},
    {993, "imaps", "Dovecot|courier", "* OK"},
    {995, "pop3s", "Dovecot|courier", "+OK"},
    {1433, "ms-sql-s", "MSSQL", ""},
    {1521, "oracle", "Oracle", ""},
    {1723, "pptp", "Microsoft|mikrotik", ""},
    {2049, "nfs", "Linux nfsd", ""},
    {3306, "mysql", "MySQL|MariaDB", "5."},
    {3389, "ms-wbt-server", "RDP", ""},
    {5432, "postgresql", "PostgreSQL", "5"},
    {5900, "vnc", "RealVNC|TightVNC", "RFB"},
    {5985, "http", "WinRM", "HTTP"},
    {5986, "https", "WinRM", "HTTP"},
    {6379, "redis", "Redis", "+PONG"},
    {8080, "http-proxy", "Nginx|Apache|Jetty", "HTTP"},
    {8443, "https-alt", "Nginx|Apache", "HTTP"},
    {9200, "elasticsearch", "Elasticsearch", "{"},
    {27017, "mongodb", "MongoDB", "MongoDB"},
    {0, NULL, NULL, NULL}
};

const char* get_service_name(int port) {
    for (int i = 0; service_signatures[i].service != NULL; i++) {
        if (service_signatures[i].port == port) {
            return service_signatures[i].service;
        }
    }
    return "unknown";
}

void detect_service(const char *target_ip, int port, int timeout, scan_result_t *result) {
    strncpy(result->service, get_service_name(port), 63);
    result->banner[0] = '\0';
    result->version[0] = '\0';
    
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
    inet_pton(AF_INET, target_ip, &addr.sin_addr);
    
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        close(sock);
        return;
    }
    
    char buffer[BANNER_SIZE] = {0};
    ssize_t n = 0;
    
    if (port == 80 || port == 8080 || port == 8000) {
        const char *http_req = "HEAD / HTTP/1.0\r\nHost: localhost\r\nUser-Agent: Blackmap/1.0\r\n\r\n";
        send(sock, http_req, strlen(http_req), 0);
        n = recv(sock, buffer, BANNER_SIZE - 1, 0);
    } else if (port == 443 || port == 8443) {
        const char *https_req = "HEAD / HTTP/1.0\r\nHost: localhost\r\nUser-Agent: Blackmap/1.0\r\n\r\n";
        send(sock, https_req, strlen(https_req), 0);
        n = recv(sock, buffer, BANNER_SIZE - 1, 0);
    } else if (port == 22) {
        n = recv(sock, buffer, BANNER_SIZE - 1, 0);
    } else if (port == 21) {
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
    } else if (port == 6379) {
        const char *redis_ping = "*1\r\n$4\r\nPING\r\n";
        send(sock, redis_ping, strlen(redis_ping), 0);
        n = recv(sock, buffer, BANNER_SIZE - 1, 0);
    } else if (port == 3306) {
        const char *mysql_req = "\x00\x00\x00\x01\x85\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00";
        send(sock, mysql_req, 24, 0);
        n = recv(sock, buffer, BANNER_SIZE - 1, 0);
    } else if (port == 5432) {
        const char *postgres_req = "\x00\x00\x00\x08\x04\xD2\x16\x2F";
        send(sock, postgres_req, 8, 0);
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
        
        strncpy(result->banner, buffer, BANNER_SIZE - 1);
        result->banner[BANNER_SIZE - 1] = '\0';
        
        for (int i = 0; service_signatures[i].service != NULL; i++) {
            if (service_signatures[i].port == port) {
                if (service_signatures[i].version_pattern[0] != '\0') {
                    if (strstr(buffer, service_signatures[i].version_pattern)) {
                        snprintf(result->version, 255, "%s", service_signatures[i].product);
                    }
                } else {
                    snprintf(result->version, 255, "%s", service_signatures[i].product);
                }
                break;
            }
        }
    }
}
