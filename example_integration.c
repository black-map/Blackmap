#include <stdio.h>
#include <stdlib.h>
#include "blackmap_rust.h"

int main() {
    const char* banner = "HTTP/1.1 200 OK\r\nServer: Apache/2.4.41\r\n";
    const char* result = blackmap_analyze_banner(banner);
    if (result) {
        printf("Analysis result: %s\n", result);
        blackmap_free_string((char*)result);
    } else {
        printf("Analysis failed\n");
    }
    return 0;
}