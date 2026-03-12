#ifndef SCANNERS_H
#define SCANNERS_H

#include "../include/common.h"
#include <stddef.h>

typedef port_state_t (*scan_func_t)(const char *target_ip, int port, int timeout);

port_state_t syn_scan(const char *target_ip, int port, int timeout);
port_state_t connect_scan(const char *target_ip, int port, int timeout);
port_state_t fin_scan(const char *target_ip, int port, int timeout);
port_state_t xmas_scan(const char *target_ip, int port, int timeout);
port_state_t null_scan(const char *target_ip, int port, int timeout);
port_state_t ack_scan(const char *target_ip, int port, int timeout);
port_state_t udp_scan(const char *target_ip, int port, int timeout);

scan_func_t get_scan_function(scan_type_t scan_type);

#endif
