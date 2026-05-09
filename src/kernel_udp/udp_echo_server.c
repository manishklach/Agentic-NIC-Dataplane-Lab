#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <signal.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 9000
#define DEFAULT_PACKET_SIZE 256

static volatile sig_atomic_t keep_running = 1;

struct server_config {
    const char *host;
    const char *json_out;
    uint16_t port;
    uint32_t packet_size;
};

struct server_stats {
    uint64_t packet_count;
    uint64_t payload_bytes;
    double elapsed_sec;
};

static uint64_t monotonic_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static void handle_signal(int signo)
{
    (void)signo;
    keep_running = 0;
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "usage: %s [--host HOST] [--port PORT] [--packet-size BYTES] [--json-out FILE]\n",
        prog);
}

static int parse_u16(const char *value, uint16_t *out)
{
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);

    if (!value[0] || (end && *end != '\0') || parsed > UINT16_MAX)
        return -1;

    *out = (uint16_t)parsed;
    return 0;
}

static int parse_u32(const char *value, uint32_t *out)
{
    char *end = NULL;
    unsigned long parsed = strtoul(value, &end, 10);

    if (!value[0] || (end && *end != '\0') || parsed > UINT32_MAX)
        return -1;

    *out = (uint32_t)parsed;
    return 0;
}

static int parse_args(int argc, char **argv, struct server_config *cfg)
{
    int i;

    cfg->host = DEFAULT_HOST;
    cfg->port = DEFAULT_PORT;
    cfg->packet_size = DEFAULT_PACKET_SIZE;
    cfg->json_out = NULL;

    for (i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--host") == 0 && i + 1 < argc) {
            cfg->host = argv[++i];
        } else if (strcmp(argv[i], "--port") == 0 && i + 1 < argc) {
            if (parse_u16(argv[++i], &cfg->port) != 0)
                return -1;
        } else if (strcmp(argv[i], "--packet-size") == 0 && i + 1 < argc) {
            if (parse_u32(argv[++i], &cfg->packet_size) != 0)
                return -1;
        } else if (strcmp(argv[i], "--json-out") == 0 && i + 1 < argc) {
            cfg->json_out = argv[++i];
        } else if (strcmp(argv[i], "-h") == 0 || strcmp(argv[i], "--help") == 0) {
            usage(argv[0]);
            exit(0);
        } else {
            return -1;
        }
    }

    return 0;
}

static int write_json(const char *path, const struct server_config *cfg, const struct server_stats *stats)
{
    FILE *fp;

    if (path == NULL)
        return 0;

    fp = fopen(path, "w");
    if (fp == NULL) {
        perror("fopen");
        return -1;
    }

    fprintf(fp,
        "{\n"
        "  \"mode\": \"kernel_udp_server\",\n"
        "  \"host\": \"%s\",\n"
        "  \"port\": %u,\n"
        "  \"packet_size\": %u,\n"
        "  \"packet_count\": %" PRIu64 ",\n"
        "  \"payload_bytes\": %" PRIu64 ",\n"
        "  \"elapsed_sec\": %.9f\n"
        "}\n",
        cfg->host,
        cfg->port,
        cfg->packet_size,
        stats->packet_count,
        stats->payload_bytes,
        stats->elapsed_sec);

    fclose(fp);
    return 0;
}

int main(int argc, char **argv)
{
    struct server_config cfg;
    struct server_stats stats;
    struct sockaddr_in addr;
    uint8_t *buffer = NULL;
    uint64_t start_ns;
    uint64_t end_ns;
    int sockfd;

    if (parse_args(argc, argv, &cfg) != 0) {
        usage(argv[0]);
        return 1;
    }

    memset(&stats, 0, sizeof(stats));
    memset(&addr, 0, sizeof(addr));

    buffer = malloc(cfg.packet_size);
    if (buffer == NULL) {
        perror("malloc");
        return 1;
    }

    signal(SIGINT, handle_signal);
    signal(SIGTERM, handle_signal);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        free(buffer);
        return 1;
    }

    addr.sin_family = AF_INET;
    addr.sin_port = htons(cfg.port);
    if (inet_pton(AF_INET, cfg.host, &addr.sin_addr) != 1) {
        fprintf(stderr, "invalid host: %s\n", cfg.host);
        close(sockfd);
        free(buffer);
        return 1;
    }

    if (bind(sockfd, (struct sockaddr *)&addr, sizeof(addr)) != 0) {
        perror("bind");
        close(sockfd);
        free(buffer);
        return 1;
    }

    fprintf(stdout, "udp_echo_server listening on %s:%u\n", cfg.host, cfg.port);
    start_ns = monotonic_ns();

    while (keep_running) {
        struct sockaddr_in peer_addr;
        socklen_t peer_len = sizeof(peer_addr);
        ssize_t received;

        received = recvfrom(sockfd, buffer, cfg.packet_size, 0, (struct sockaddr *)&peer_addr, &peer_len);
        if (received < 0) {
            if (errno == EINTR)
                continue;
            perror("recvfrom");
            break;
        }

        if (sendto(sockfd, buffer, (size_t)received, 0, (struct sockaddr *)&peer_addr, peer_len) < 0) {
            perror("sendto");
            break;
        }

        stats.packet_count++;
        stats.payload_bytes += (uint64_t)received;
    }

    end_ns = monotonic_ns();
    stats.elapsed_sec = (double)(end_ns - start_ns) / 1000000000.0;

    fprintf(stdout,
        "udp_echo_server exiting packets=%" PRIu64 " bytes=%" PRIu64 " elapsed=%.6fs\n",
        stats.packet_count,
        stats.payload_bytes,
        stats.elapsed_sec);

    (void)write_json(cfg.json_out, &cfg, &stats);

    close(sockfd);
    free(buffer);
    return 0;
}
