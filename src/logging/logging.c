#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <time.h>
#include "logging.h"

static FILE *log_file = NULL;

void log_init(const char *filename) {
    if (filename) {
        log_file = fopen(filename, "a");
        if (!log_file) {
            fprintf(stderr, "Failed to open log file: %s\n", filename);
        }
    }
}

void log_close(void) {
    if (log_file) {
        fclose(log_file);
        log_file = NULL;
    }
}

static void log_message(const char *level, const char *format, va_list args) {
    time_t now = time(NULL);
    char timestamp[20];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", localtime(&now));

    fprintf(stderr, "[%s] %s: ", timestamp, level);
    vfprintf(stderr, format, args);
    fprintf(stderr, "\n");

    if (log_file) {
        fprintf(log_file, "[%s] %s: ", timestamp, level);
        vfprintf(log_file, format, args);
        fprintf(log_file, "\n");
        fflush(log_file);
    }
}

void log_info(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_message("INFO", format, args);
    va_end(args);
}

void log_error(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_message("ERROR", format, args);
    va_end(args);
}

void log_debug(const char *format, ...) {
    va_list args;
    va_start(args, format);
    log_message("DEBUG", format, args);
    va_end(args);
}