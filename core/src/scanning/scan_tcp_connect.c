#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/time.h>
#include <errno.h>
#include <fcntl.h>
#include "blackmap.h"

/* TCP CONNECT scan - functional implementation */

static int set_nonblocking(int sock) {
    int flags = fcntl(sock, F_GETFL, 0);
    if (flags == -1) return -1;
    return fcntl(sock, F_SETFL, flags | O_NONBLOCK);
}

int tcp_connect_scan(uint32_t target_ip, uint16_t port, int timeout_ms) {
    struct sockaddr_in addr;
    int sock;
    int result = PORT_CLOSED;
    
    sock = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (sock == -1) {
        perror("socket");
        return PORT_UNKNOWN;
    }
    
    /* Set non-blocking */
    if (set_nonblocking(sock) == -1) {
        close(sock);
        return PORT_UNKNOWN;
    }
    
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port);
    addr.sin_addr.s_addr = htonl(target_ip);
    
    /* Attempt connection */
    if (connect(sock, (struct sockaddr *)&addr, sizeof(addr)) == -1) {
        if (errno == EINPROGRESS) {
            /* Connection in progress - wait for completion */
            fd_set writefds;
            struct timeval tv;
            
            FD_ZERO(&writefds);
            FD_SET(sock, &writefds);
            
            tv.tv_sec = timeout_ms / 1000;
            tv.tv_usec = (timeout_ms % 1000) * 1000;
            
            int select_ret = select(sock + 1, NULL, &writefds, NULL, &tv);
            
            if (select_ret > 0 && FD_ISSET(sock, &writefds)) {
                /* Check connection status */
                int error = 0;
                socklen_t len = sizeof(error);
                
                if (getsockopt(sock, SOL_SOCKET, SO_ERROR, &error, &len) == -1) {
                    result = PORT_UNKNOWN;
                } else if (error == 0) {
                    result = PORT_OPEN;
                } else {
                    result = PORT_CLOSED;
                }
            } else if (select_ret == 0) {
                result = PORT_FILTERED;  /* Timeout = filtered */
            } else {
                result = PORT_UNKNOWN;
            }
        } else if (errno == ECONNREFUSED) {
            result = PORT_CLOSED;
        } else {
            result = PORT_FILTERED;
        }
    } else {
        /* Immediate connection (shouldn't happen with non-blocking) */
        result = PORT_OPEN;
    }
    
    close(sock);
    return result;
}

int tcp_syn_scan_stub(uint32_t target_ip, uint16_t port) {
    if (geteuid() != 0) {
        /* SYN scan requires root - fall back to CONNECT */
        return tcp_connect_scan(target_ip, port, g_config->timeout_ms);
    }
    
    /* TODO: Implement raw socket SYN scan */
    return tcp_connect_scan(target_ip, port, g_config->timeout_ms);
}
