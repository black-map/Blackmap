#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

/* Stub for socket operations */

int socket_init(void) {
    if (getuid() != 0) {
        fprintf(stderr, "[-] Warning: Not running as root. Some features unavailable.\n");
    }
    return 0;
}

void socket_cleanup(void) {
}
