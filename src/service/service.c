#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <regex.h>
#include "service.h"
#include "blackmap.h"
#include "blackmap_rust.h"

#define MAX_BANNER_SIZE 1024

typedef struct {
    uint16_t port;
    const char *probe;
    const char *regex;
} service_signature_t;

static service_signature_t signatures[] = {
    {80, NULL, "HTTP/([0-9.]+)"},
    {443, NULL, NULL}, // For HTTPS, need SSL handshake
    {22, NULL, "SSH-([0-9.]+)"}, // Read banner
    {21, NULL, "220.*FTP"}, // Read banner
    {25, "EHLO test\r\n", "220.*SMTP"},
    {0, NULL, NULL}
};

int detect_service(uint32_t ip, uint16_t port, port_info_t *info) {
    printf("Calling detect_service for port %u\n", port);
    int sock;
    struct sockaddr_in addr;
    char buffer[MAX_BANNER_SIZE];
    int n;

    sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) return -1;

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = ip;
    addr.sin_port = htons(port);

    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        close(sock);
        return -1;
    }

    // Set recv timeout
    struct timeval tv;
    tv.tv_sec = 5;
    tv.tv_usec = 0;
    setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &tv, sizeof(tv));

    // Find signature
    service_signature_t *sig = NULL;
    for (int i = 0; signatures[i].port; i++) {
        if (signatures[i].port == port) {
            sig = &signatures[i];
            break;
        }
    }

    if (!sig) {
        close(sock);
        return 0; // No signature, but connected
    }

    if (sig->probe) {
        // Send probe
        send(sock, sig->probe, strlen(sig->probe), 0);
    }

    // Read response
    n = recv(sock, buffer, sizeof(buffer) - 1, 0);
    if (n > 0) {
        buffer[n] = '\0';
        // Use Rust module for advanced analysis
        const char* json_result = blackmap_analyze_banner(buffer);
        if (json_result) {
            // For now, just print or store; in full implementation, parse JSON
            printf("[*] Rust analysis: %s\n", json_result);
            // Free the string
            blackmap_free_string((char*)json_result);
        }
        // Fallback to old method
        strncpy(info->banner, buffer, sizeof(info->banner) - 1);

        if (sig->regex) {
            regex_t reg;
            regmatch_t match[2];
            if (regcomp(&reg, sig->regex, REG_EXTENDED) == 0) {
                if (regexec(&reg, buffer, 2, match, 0) == 0) {
                    char version[64];
                    int len = match[1].rm_eo - match[1].rm_so;
                    if (len < sizeof(version)) {
                        strncpy(version, buffer + match[1].rm_so, len);
                        version[len] = '\0';
                        strncpy(info->version, version, sizeof(info->version) - 1);
                    }
                }
                regfree(&reg);
            }
        }
    }

    close(sock);
    return 0;
}

int load_service_signatures(const char *filename) {
    // Stub: load from file
    return 0;
}