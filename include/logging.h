#ifndef LOGGING_H
#define LOGGING_H

#include <stdio.h>

/* Structured logging */
void log_init(const char *filename);
void log_close(void);
void log_info(const char *format, ...);
void log_error(const char *format, ...);
void log_debug(const char *format, ...);

#endif /* LOGGING_H */