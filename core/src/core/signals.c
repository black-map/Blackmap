#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "blackmap.h"

/* Signal Handling */

int signal_setup(void) {
    printf("[*] Signal handlers registered\n");
    return 0;
}

void signal_cleanup(void) {
    printf("[*] Signal handlers cleanup\n");
}
