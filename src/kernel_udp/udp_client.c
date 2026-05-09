#include <arpa/inet.h>
#include <errno.h>
#include <inttypes.h>
#include <math.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <sys/time.h>

#define DEFAULT_HOST "127.0.0.1"
#define DEFAULT_PORT 9000
#define DEFAULT_PACKET_SIZE 256
#define DEFAULT_COUNT 1000

struct client_config {
    const char *host;
    const char *json_out;
    uint16_t port;
    uint32_t packet_size;
    uint64_t count;
};

struct client_stats {
    uint64_t packet_count;
    uint64_t payload_bytes;
    double elapsed_sec;
    double avg_latency_us;
    double p50_latency_us;
    double p95_latency_us;
    double p99_latency_us;
    double packets_per_sec;
    double mb_per_sec;
};

static uint64_t monotonic_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static int compare_doubles(const void *a, const void *b)
{
    const double da = *(const double *)a;
    const double db = *(const double *)b;

    if (da < db)
        return -1;
    if (da > db)
        return 1;
    return 0;
}

static double percentile(const double *values, uint64_t count, double p)
{
    double rank;
    uint64_t idx;

    if (count == 0)
        return 0.0;

    rank = p * (double)(count - 1);
    idx = (uint64_t)rank;
    return values[idx];
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "usage: %s [--host HOST] [--port PORT] [--packet-size BYTES] [--count N] [--json-out FILE]\n",
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

static int parse_u64(const char *value, uint64_t *out)
{
    char *end = NULL;
    unsigned long long parsed = strtoull(value, &end, 10);

    if (!value[0] || (end && *end != '\0'))
        return -1;

    *out = (uint64_t)parsed;
    return 0;
}

static int parse_args(int argc, char **argv, struct client_config *cfg)
{
    int i;

    cfg->host = DEFAULT_HOST;
    cfg->port = DEFAULT_PORT;
    cfg->packet_size = DEFAULT_PACKET_SIZE;
    cfg->count = DEFAULT_COUNT;
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
        } else if (strcmp(argv[i], "--count") == 0 && i + 1 < argc) {
            if (parse_u64(argv[++i], &cfg->count) != 0)
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

static int write_json(const char *path, const struct client_config *cfg, const struct client_stats *stats)
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
        "  \"mode\": \"kernel_udp_baseline\",\n"
        "  \"host\": \"%s\",\n"
        "  \"port\": %u,\n"
        "  \"packet_count\": %" PRIu64 ",\n"
        "  \"packet_size\": %u,\n"
        "  \"payload_bytes\": %" PRIu64 ",\n"
        "  \"elapsed_sec\": %.9f,\n"
        "  \"avg_latency_us\": %.3f,\n"
        "  \"p50_latency_us\": %.3f,\n"
        "  \"p95_latency_us\": %.3f,\n"
        "  \"p99_latency_us\": %.3f,\n"
        "  \"packets_per_sec\": %.3f,\n"
        "  \"mb_per_sec\": %.3f\n"
        "}\n",
        cfg->host,
        cfg->port,
        stats->packet_count,
        cfg->packet_size,
        stats->payload_bytes,
        stats->elapsed_sec,
        stats->avg_latency_us,
        stats->p50_latency_us,
        stats->p95_latency_us,
        stats->p99_latency_us,
        stats->packets_per_sec,
        stats->mb_per_sec);

    fclose(fp);
    return 0;
}

int main(int argc, char **argv)
{
    struct client_config cfg;
    struct client_stats stats;
    struct sockaddr_in server_addr;
    uint8_t *send_buf = NULL;
    uint8_t *recv_buf = NULL;
    double *latencies = NULL;
    uint64_t total_latency_ns = 0;
    uint64_t start_ns;
    uint64_t end_ns;
    uint64_t i;
    int sockfd;
    struct timeval recv_timeout = {
        .tv_sec = 2,
        .tv_usec = 0,
    };

    if (parse_args(argc, argv, &cfg) != 0) {
        usage(argv[0]);
        return 1;
    }

    memset(&stats, 0, sizeof(stats));
    memset(&server_addr, 0, sizeof(server_addr));

    send_buf = malloc(cfg.packet_size);
    recv_buf = malloc(cfg.packet_size);
    latencies = malloc(sizeof(double) * cfg.count);
    if (send_buf == NULL || recv_buf == NULL || latencies == NULL) {
        perror("malloc");
        free(send_buf);
        free(recv_buf);
        free(latencies);
        return 1;
    }

    memset(send_buf, 'A', cfg.packet_size);

    sockfd = socket(AF_INET, SOCK_DGRAM, 0);
    if (sockfd < 0) {
        perror("socket");
        free(send_buf);
        free(recv_buf);
        free(latencies);
        return 1;
    }

    if (setsockopt(sockfd, SOL_SOCKET, SO_RCVTIMEO, &recv_timeout, sizeof(recv_timeout)) != 0) {
        perror("setsockopt(SO_RCVTIMEO)");
        close(sockfd);
        free(send_buf);
        free(recv_buf);
        free(latencies);
        return 1;
    }

    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(cfg.port);
    if (inet_pton(AF_INET, cfg.host, &server_addr.sin_addr) != 1) {
        fprintf(stderr, "invalid host: %s\n", cfg.host);
        close(sockfd);
        free(send_buf);
        free(recv_buf);
        free(latencies);
        return 1;
    }

    start_ns = monotonic_ns();
    for (i = 0; i < cfg.count; i++) {
        uint64_t send_ns;
        uint64_t recv_ns;
        ssize_t sent;
        ssize_t received;

        send_ns = monotonic_ns();
        sent = sendto(sockfd, send_buf, cfg.packet_size, 0, (struct sockaddr *)&server_addr, sizeof(server_addr));
        if (sent < 0) {
            perror("sendto");
            close(sockfd);
            free(send_buf);
            free(recv_buf);
            free(latencies);
            return 1;
        }

        received = recvfrom(sockfd, recv_buf, cfg.packet_size, 0, NULL, NULL);
        if (received < 0) {
            if (errno == EAGAIN || errno == EWOULDBLOCK)
                fprintf(stderr, "recvfrom timeout after %" PRIu64 " packets; is the echo server running on %s:%u?\n",
                    stats.packet_count, cfg.host, cfg.port);
            perror("recvfrom");
            close(sockfd);
            free(send_buf);
            free(recv_buf);
            free(latencies);
            return 1;
        }

        recv_ns = monotonic_ns();
        latencies[i] = (double)(recv_ns - send_ns) / 1000.0;
        total_latency_ns += recv_ns - send_ns;
        stats.packet_count++;
        stats.payload_bytes += (uint64_t)received;
    }
    end_ns = monotonic_ns();

    stats.elapsed_sec = (double)(end_ns - start_ns) / 1000000000.0;
    stats.avg_latency_us = ((double)total_latency_ns / (double)cfg.count) / 1000.0;
    stats.packets_per_sec = stats.packet_count / stats.elapsed_sec;
    stats.mb_per_sec = ((double)stats.payload_bytes / (1024.0 * 1024.0)) / stats.elapsed_sec;

    qsort(latencies, (size_t)cfg.count, sizeof(double), compare_doubles);
    stats.p50_latency_us = percentile(latencies, cfg.count, 0.50);
    stats.p95_latency_us = percentile(latencies, cfg.count, 0.95);
    stats.p99_latency_us = percentile(latencies, cfg.count, 0.99);

    fprintf(stdout,
        "udp_client complete packets=%" PRIu64 " bytes=%" PRIu64 " elapsed=%.6fs pps=%.3f MB/s=%.3f avg_us=%.3f p50=%.3f p95=%.3f p99=%.3f\n",
        stats.packet_count,
        stats.payload_bytes,
        stats.elapsed_sec,
        stats.packets_per_sec,
        stats.mb_per_sec,
        stats.avg_latency_us,
        stats.p50_latency_us,
        stats.p95_latency_us,
        stats.p99_latency_us);

    (void)write_json(cfg.json_out, &cfg, &stats);

    close(sockfd);
    free(send_buf);
    free(recv_buf);
    free(latencies);
    return 0;
}
