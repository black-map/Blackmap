#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "blackmap.h"

/* Output Formatters */

int output_normal(FILE *fp, host_info_t *host) {
    if (!fp || !host) return -1;
    
    fprintf(fp, "Host: %s\n", host->hostname);
    fprintf(fp, "State: %s\n", host->state == HOST_UP ? "Up" : "Down");
    
    for (uint32_t i = 0; i < host->num_ports; i++) {
        port_info_t *port = &host->ports[i];
        fprintf(fp, "Port %u/%s\tState: %d\tService: %s\n", 
                port->port, port->protocol, port->state, port->service);
    }
    
    fprintf(fp, "\n");
    return 0;
}

int output_xml(FILE *fp, host_info_t *host) {
    if (!fp || !host) return -1;
    
    fprintf(fp, "  <host starttime=\"0\" endtime=\"0\">\n");
    fprintf(fp, "    <status state=\"%s\"/>\n", 
            host->state == HOST_UP ? "up" : "down");
    fprintf(fp, "    <address addr=\"TODO\" addrtype=\"ipv4\"/>\n");
    fprintf(fp, "    <hostnames>\n");
    fprintf(fp, "      <hostname name=\"%s\"/>\n", host->hostname);
    fprintf(fp, "    </hostnames>\n");
    fprintf(fp, "    <ports>\n");
    
    for (uint32_t i = 0; i < host->num_ports; i++) {
        port_info_t *port = &host->ports[i];
        fprintf(fp, "      <port protocol=\"%s\" portid=\"%u\">\n",
                port->protocol, port->port);
        fprintf(fp, "        <state state=\"%d\"/>\n", port->state);
        fprintf(fp, "        <service name=\"%s\"/>\n", port->service);
        fprintf(fp, "      </port>\n");
    }
    
    fprintf(fp, "    </ports>\n");
    fprintf(fp, "  </host>\n");
    return 0;
}

int output_grep(FILE *fp, host_info_t *host) {
    if (!fp || !host) return -1;
    
    fprintf(fp, "%s\t%s", "Host", host->hostname);
    
    for (uint32_t i = 0; i < host->num_ports; i++) {
        port_info_t *port = &host->ports[i];
        fprintf(fp, "\t%u/%s/%d/%s", port->port, port->protocol, 
                port->state, port->service);
    }
    
    fprintf(fp, "\n");
    return 0;
}

int output_json(FILE *fp, host_info_t *host) {
    if (!fp || !host) return -1;
    
    fprintf(fp, "  {\n");
    fprintf(fp, "    \"hostname\": \"%s\",\n", host->hostname);
    fprintf(fp, "    \"state\": \"%s\",\n", host->state == HOST_UP ? "up" : "down");
    fprintf(fp, "    \"ports\": [\n");
    
    for (uint32_t i = 0; i < host->num_ports; i++) {
        port_info_t *port = &host->ports[i];
        fprintf(fp, "      {\n");
        fprintf(fp, "        \"port\": %u,\n", port->port);
        fprintf(fp, "        \"protocol\": \"%s\",\n", port->protocol);
        fprintf(fp, "        \"state\": %d,\n", port->state);
        fprintf(fp, "        \"service\": \"%s\"\n", port->service);
        fprintf(fp, "      }%s\n", i < host->num_ports - 1 ? "," : "");
    }
    
    fprintf(fp, "    ]\n");
    fprintf(fp, "  }%s\n", "");
    return 0;
}

int output_sqlite(FILE *fp, host_info_t *host) {
    if (!fp || !host) return -1;
    
    /* SQL insertion statements */
    fprintf(fp, "INSERT INTO hosts (hostname, state) VALUES ('%s', %d);\n",
            host->hostname, host->state);
    
    for (uint32_t i = 0; i < host->num_ports; i++) {
        port_info_t *port = &host->ports[i];
        fprintf(fp, "INSERT INTO ports (host_id, port, protocol, state, service) "
                "VALUES (last_insert_id(), %u, '%s', %d, '%s');\n",
                port->port, port->protocol, port->state, port->service);
    }
    
    return 0;
}

int output_html(FILE *fp, host_info_t *host) {
    if (!fp || !host) return -1;
    
    fprintf(fp, "<tr>\n");
    fprintf(fp, "  <td>%s</td>\n", host->hostname);
    fprintf(fp, "  <td>%s</td>\n", host->state == HOST_UP ? "Up" : "Down");
    
    fprintf(fp, "  <td>\n");
    for (uint32_t i = 0; i < host->num_ports; i++) {
        port_info_t *port = &host->ports[i];
        fprintf(fp, "    %u/%s (%s)<br/>\n", port->port, port->protocol, port->service);
    }
    fprintf(fp, "  </td>\n");
    fprintf(fp, "</tr>\n");
    
    return 0;
}

int output_markdown(FILE *fp, host_info_t *host) {
    if (!fp || !host) return -1;
    
    fprintf(fp, "## %s\n\n", host->hostname);
    fprintf(fp, "**State:** %s\n\n", host->state == HOST_UP ? "Up" : "Down");
    fprintf(fp, "### Ports\n\n");
    fprintf(fp, "| Port | Protocol | State | Service |\n");
    fprintf(fp, "|------|----------|-------|----------|\n");
    
    for (uint32_t i = 0; i < host->num_ports; i++) {
        port_info_t *port = &host->ports[i];
        fprintf(fp, "| %u | %s | %d | %s |\n", 
                port->port, port->protocol, port->state, port->service);
    }
    
    fprintf(fp, "\n");
    return 0;
}

int output_write_results(host_info_t **hosts, uint32_t num_hosts,
                         const blackmap_config_t *config) {
    if (!hosts || !config) return -1;
    
    if (g_config->output_normal) {
        FILE *fp = config->output_file[0] ? 
                   fopen(config->output_file, "a") : stdout;
        
        for (uint32_t i = 0; i < num_hosts; i++) {
            output_normal(fp, hosts[i]);
        }
        
        if (fp != stdout) fclose(fp);
    }
    
    return 0;
}
