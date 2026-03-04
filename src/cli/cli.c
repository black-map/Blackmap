#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <getopt.h>
#include "cli.h"
#include "blackmap.h"
#include "logging.h"

void print_usage(const char *prog) {
    fprintf(stdout,
        "BlackMap v%s - Next-generation network scanner\n"
        "Usage: %s [options] <target>\n\n"
        "Target specification:\n"
        "  Can be a single host, IP range (127-129), CIDR (192.168.0.0/16), etc.\n\n"
        "Host discovery:\n"
        "  -Pn                        Treat all ports as if alive (skip ping)\n"
        "  -PS <ports>                TCP SYN ping\n"
        "  -PA <ports>                TCP ACK ping\n"
        "  -PU <ports>                UDP ping\n"
        "  -PR                        ARP ping\n"
        "  -PP                        ICMP ping\n\n"
        "Scan techniques:\n"
        "  -sS                        TCP SYN scan (default)\n"
        "  -sT                        TCP CONNECT scan\n"
        "  -sU                        UDP scan\n"
        "  -sY                        SCTP INIT scan\n"
        "  -sZ                        SCTP COOKIE-ECHO scan\n"
        "  -sA                        TCP ACK scan\n"
        "  -sW                        TCP Window scan\n"
        "  -sM                        TCP Maimon scan\n"
        "  -sF                        TCP FIN scan\n"
        "  -sN                        TCP Null scan\n"
        "  -sX                        TCP Xmas scan\n"
        "  -sO                        IP protocol scan\n"
        "  -sI <zombie>               Idle/zombie scan\n\n"
        "Port specification:\n"
        "  -p <ports>                 Only scan specified ports (e.g., 22,80,443 or 1-65535)\n"
        "  -p- <num>                  Scan all ports\n"
        "  -p <service>               Scan by service name\n"
        "  --top-ports <num>          Scan only top N ports shared\n\n"
        "Timing and performance:\n"
        "  -T<0-5>                    Timing template (paranoid, sneaky, polite, normal, aggressive, insane)\n"
        "  --min-rate <num>           Send packets no slower than <num> per second\n"
        "  --max-rate <num>           Send packets no faster than <num> per second\n"
        "  --scan-delay <time>        Delay between probes\n"
        "  --max-scan-delay <time>    Maximum scan delay\n"
        "  --initial-rtt-timeout <time> Set initial timeout\n\n"
        "Service/version detection:\n"
        "  -sV                        Version detection\n"
        "  --version-intensity <0-9>  Set version detection intensity\n"
        "  --version-all              Try every version probe\n\n"
        "OS detection:\n"
        "  -O                         Enable OS detection\n\n"
        "Script scanning:\n"
        "  -sC                        Run default scripts\n"
        "  --script <script>          Run custom script(s)\n"
        "  --script-args <args>       Pass arguments to scripts\n"
        "  --script-timeout <time>    Script timeout\n\n"
        "Firewall/IDS Evasion:\n"
        "  -f; --mtu <num>            Fragment packets (MTU value)\n"
        "  -D <decoy1,decoy2,...>     Cloak scan with decoys\n"
        "  -S <ip>                    Spoof source IP\n"
        "  -e <iface>                 Use specified network interface\n"
        "  --source-port <port>       Spoof source port\n"
        "  --data-length <len>        Append random data to packets\n"
        "  --data-string <string>     Append custom string to packets\n"
        "  --data-hex <hex>           Append custom hex to packets\n"
        "  --spoof-mac <mac>          Spoof MAC address\n"
        "  --ttl <val>                Set IP time-to-live\n"
        "  --randomize-hosts          Randomize target order\n\n"
        "Output:\n"
        "  -oN <file>                 Normal output\n"
        "  -oX <file>                 XML output\n"
        "  -oG <file>                 Grepable output\n"
        "  -oJ <file>                 JSON output\n"
        "  -oS <file>                 SQLite output\n"
        "  -oH <file>                 HTML output\n"
        "  -oM <file>                 Markdown output\n"
        "  -oA <file>                 Output in all formats\n\n"
        "Proxy and Tor:\n"
        "  --proxy-enforced           Enforce proxy usage\n"
        "  --dns-mode <mode>          DNS resolution mode (proxy|local|none)\n\n"
        "I/O Engine:\n"
        "  --io-engine <engine>       Select I/O engine (select, epoll, uring, xdp)\n\n"
        "Misc:\n"
        "  -v, --verbosity <level>    Increase verbosity level\n"
        "  --debug                    Enable debug mode\n"
        "  --log <file>               Structured logging to file\n"
        "  -h, --help                 Show this help\n\n",
        BLACKMAP_VERSION, prog
    );
}

void print_version(void) {
    printf("BlackMap v%s\n", BLACKMAP_VERSION);
    printf("Next-generation network scanner\n");
}

int parse_command_line(int argc, char *argv[], blackmap_config_t *config) {
    int c;
    struct option long_opts[] = {
        {"help", no_argument, 0, 'h'},
        {"version", no_argument, 0, 'V'},
        {"verbose", no_argument, 0, 'v'},
        {"debug", no_argument, 0, 'D'},
        {"io-engine", required_argument, 0, 1000},
        {"top-ports", required_argument, 0, 1001},
        {"min-rate", required_argument, 0, 1002},
        {"max-rate", required_argument, 0, 1003},
        {"scan-delay", required_argument, 0, 1004},
        {"max-scan-delay", required_argument, 0, 1005},
        {"initial-rtt-timeout", required_argument, 0, 1006},
        {"version-intensity", required_argument, 0, 1007},
        {"timing", required_argument, 0, 'T'},
        {"script", required_argument, 0, 1009},
        {"script-args", required_argument, 0, 1010},
        {"script-timeout", required_argument, 0, 1011},
        {"mtu", required_argument, 0, 1012},
        {"source-port", required_argument, 0, 1013},
        {"data-length", required_argument, 0, 1014},
        {"data-string", required_argument, 0, 1015},
        {"data-hex", required_argument, 0, 1016},
        {"spoof-mac", required_argument, 0, 1017},
        {"ttl", required_argument, 0, 1018},
        {"randomize-hosts", no_argument, 0, 1019},
        {"proxy-enforced", no_argument, 0, 1020},
        {"dns-mode", required_argument, 0, 1021},
        {"log", required_argument, 0, 1022},
        {"slow-stealth", no_argument, 0, 1023},
        {0, 0, 0, 0}
    };

    // Set defaults
    config->io_engine = IO_ENGINE_SELECT;
    config->scan_type = SCAN_TYPE_SYN;
    config->timing = TIMING_NORMAL;
    config->num_threads = 16;
    config->timeout_ms = 5000;
    config->retries = 2;
    config->require_root = true;
    config->verbosity = 0;
    config->dns_mode = 0; // local by default

    while ((c = getopt_long(argc, argv, "hVvDo:p:s::S::t:T::OPn", long_opts, NULL)) != -1) {
        switch (c) {
            case 'h':
                print_usage(argv[0]);
                return 1; // exit
            case 'V':
                print_version();
                return 1;
            case 'v':
                config->verbosity++;
                break;
            case 'D':
                config->debug = true;
                break;
            case 'o':
                if (optarg) {
                    if (strcmp(optarg, "N") == 0) config->output_normal = true;
                    else if (strcmp(optarg, "X") == 0) config->output_xml = true;
                    else if (strcmp(optarg, "G") == 0) config->output_grep = true;
                    else if (strcmp(optarg, "J") == 0) config->output_json = true;
                    else if (strcmp(optarg, "S") == 0) config->output_sqlite = true;
                    else if (strcmp(optarg, "H") == 0) config->output_html = true;
                    else if (strcmp(optarg, "M") == 0) config->output_markdown = true;
                    else if (strcmp(optarg, "A") == 0) {
                        config->output_normal = config->output_xml = config->output_grep = config->output_json = config->output_sqlite = config->output_html = config->output_markdown = true;
                    } else {
                        fprintf(stderr, "Invalid output format: %s\n", optarg);
                        return -1;
                    }
                }
                break;
            case 'p':
                if (parse_ports(optarg) != 0) {
                    fprintf(stderr, "[-] Failed to parse ports\n");
                    return -1;
                }
                break;
            case 's':
                if (optarg) {
                    if (strcmp(optarg, "S") == 0) config->scan_type = SCAN_TYPE_SYN;
                    else if (strcmp(optarg, "T") == 0) config->scan_type = SCAN_TYPE_CONNECT;
                    else if (strcmp(optarg, "U") == 0) config->scan_type = SCAN_TYPE_UDP;
                    else if (strcmp(optarg, "Y") == 0) config->scan_type = SCAN_TYPE_SCTP_INIT;
                    else if (strcmp(optarg, "Z") == 0) config->scan_type = SCAN_TYPE_SCTP_COOKIE;
                    else if (strcmp(optarg, "A") == 0) config->scan_type = SCAN_TYPE_ACK;
                    else if (strcmp(optarg, "W") == 0) config->scan_type = SCAN_TYPE_WINDOW;
                    else if (strcmp(optarg, "M") == 0) config->scan_type = SCAN_TYPE_MAIMON;
                    else if (strcmp(optarg, "F") == 0) config->scan_type = SCAN_TYPE_FIN;
                    else if (strcmp(optarg, "N") == 0) config->scan_type = SCAN_TYPE_NULL;
                    else if (strcmp(optarg, "X") == 0) config->scan_type = SCAN_TYPE_XMAS;
                    else if (strcmp(optarg, "O") == 0) config->scan_type = SCAN_TYPE_IP_PROTO;
                    else if (strcmp(optarg, "I") == 0) config->scan_type = SCAN_TYPE_IDLE;
                    else if (strcmp(optarg, "V") == 0) config->version_detection = true;
                    else if (strcmp(optarg, "C") == 0) config->script_scan = true;
                    else {
                        fprintf(stderr, "Invalid scan type: -s%s\n", optarg);
                        return -1;
                    }
                }
                break;
            case 'S':
                // Spoof IP, but for now stub
                break;
            case 't':
                // top ports
                break;
            case 'T':
                if (optarg && strlen(optarg) == 1 && optarg[0] >= '0' && optarg[0] <= '5') {
                    config->timing = optarg[0] - '0';
                } else {
                    fprintf(stderr, "Invalid timing template: %s\n", optarg);
                    return -1;
                }
                break;
            case 'O':
                config->os_detection = true;
                break;
            case 'P':
                // Ping types, stub
                break;
            case 'n':
                // Skip DNS, stub
                break;
            case 1000: // --io-engine
                if (strcmp(optarg, "select") == 0) config->io_engine = IO_ENGINE_SELECT;
                else if (strcmp(optarg, "epoll") == 0) config->io_engine = IO_ENGINE_EPOLL;
                else if (strcmp(optarg, "uring") == 0) config->io_engine = IO_ENGINE_URING;
                else if (strcmp(optarg, "xdp") == 0) config->io_engine = IO_ENGINE_XDP;
                else {
                    fprintf(stderr, "Invalid IO engine: %s\n", optarg);
                    return -1;
                }
                break;
            case 1001: // --top-ports
                // stub
                break;
            case 1002: // --min-rate
                config->min_rate = atoi(optarg);
                break;
            case 1003: // --max-rate
                config->max_rate = atoi(optarg);
                break;
            case 1004: // --scan-delay
                config->scan_delay_ms = atoi(optarg);
                break;
            case 1005: // --max-scan-delay
                config->max_scan_delay_ms = atoi(optarg);
                break;
            case 1006: // --initial-rtt-timeout
                config->timeout_ms = atoi(optarg);
                break;
            case 1007: // --version-intensity
                config->version_intensity = atoi(optarg);
                break;
            case 1008: // --version-all
                config->version_all = true;
                break;
            case 1009: // --script
                strncpy(config->script_names, optarg, sizeof(config->script_names)-1);
                break;
            case 1010: // --script-args
                strncpy(config->script_args, optarg, sizeof(config->script_args)-1);
                break;
            case 1011: // --script-timeout
                config->script_timeout_ms = atoi(optarg);
                break;
            case 1012: // --mtu
                config->mtu = atoi(optarg);
                break;
            case 1013: // --source-port
                config->source_port = atoi(optarg);
                break;
            case 1014: // --data-length
                // stub
                break;
            case 1015: // --data-string
                // stub
                break;
            case 1016: // --data-hex
                // stub
                break;
            case 1017: // --spoof-mac
                strncpy(config->spoof_mac, optarg, sizeof(config->spoof_mac)-1);
                break;
            case 1018: // --ttl
                config->ttl = atoi(optarg);
                break;
            case 1019: // --randomize-hosts
                config->randomize_hosts = true;
                break;
            case 1020: // --proxy-enforced
                config->proxy_enforced = true;
                break;
            case 1021: // --dns-mode
                if (strcmp(optarg, "proxy") == 0) config->dns_mode = 1;
                else if (strcmp(optarg, "local") == 0) config->dns_mode = 0;
                else if (strcmp(optarg, "none") == 0) config->dns_mode = 2;
                else {
                    fprintf(stderr, "Invalid DNS mode: %s\n", optarg);
                    return -1;
                }
                break;
            case 1022: // --log
                log_init(optarg);
                break;
            case '?':
            default:
                fprintf(stderr, "Invalid option. Use -h for help.\n");
                return -1;
        }
    }

    if (optind >= argc) {
        fprintf(stderr, "Error: No target specified\n");
        print_usage(argv[0]);
        return -1;
    }

    strncpy(config->targets_str, argv[optind], sizeof(config->targets_str)-1);

    return 0;
}