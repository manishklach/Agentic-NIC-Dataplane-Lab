#include <errno.h>
#include <inttypes.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifndef AFXDP_MOCK_ONLY
#include <linux/if_link.h>
#include <net/if.h>
#include <poll.h>
#include <sys/mman.h>
#include <unistd.h>

#if defined(__has_include)
#  if __has_include(<xdp/xsk.h>)
#    include <xdp/xsk.h>
#  elif __has_include(<bpf/xsk.h>)
#    include <bpf/xsk.h>
#  else
#    define AFXDP_MOCK_ONLY 1
#  endif
#else
#  include <bpf/xsk.h>
#endif
#endif

#define DEFAULT_PACKET_SIZE 256
#define DEFAULT_PACKET_COUNT 100000
#define DEFAULT_DURATION_MS 2000

#ifndef AFXDP_MOCK_ONLY
#define FRAME_SIZE XSK_UMEM__DEFAULT_FRAME_SIZE
#define NUM_FRAMES 4096
#define RX_RING_SIZE 1024
#define TX_RING_SIZE 1024
#define FQ_RING_SIZE 2048
#define CQ_RING_SIZE 2048
#define BATCH_SIZE 64

struct afxdp_runtime {
    struct xsk_ring_prod fq;
    struct xsk_ring_cons cq;
    struct xsk_ring_cons rx;
    struct xsk_ring_prod tx;
    struct xsk_umem *umem;
    struct xsk_socket *xsk;
    void *buffer;
    uint64_t packets_rx;
    uint64_t bytes_rx;
    uint64_t rx_wakeups;
    uint64_t tx_completions;
};
#endif

struct app_config {
    const char *ifname;
    const char *json_out;
    uint32_t queue_id;
    uint32_t packet_size;
    uint64_t packet_count;
    int duration_ms;
    bool need_wakeup;
    bool mock_mode;
};

struct benchmark_result {
    const char *mode;
    uint64_t packet_count;
    uint32_t packet_size;
    uint64_t payload_bytes;
    double elapsed_sec;
    double packets_per_sec;
    double mb_per_sec;
    bool estimated_copy_avoidance;
    bool real_af_xdp;
};

static uint64_t monotonic_ns(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

static double bytes_to_mb(uint64_t bytes)
{
    return (double)bytes / (1024.0 * 1024.0);
}

static void usage(const char *prog)
{
    fprintf(stderr,
        "usage: %s [--mock] [--ifname IFACE] [--queue-id N] [--duration-ms N]\\n"
        "          [--packet-size BYTES] [--count N] [--need-wakeup 0|1] [--json-out FILE]\\n",
        prog);
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

static int parse_bool_flag(const char *value, bool *out)
{
    if (strcmp(value, "1") == 0 || strcmp(value, "true") == 0) {
        *out = true;
        return 0;
    }
    if (strcmp(value, "0") == 0 || strcmp(value, "false") == 0) {
        *out = false;
        return 0;
    }
    return -1;
}

static int parse_args(int argc, char **argv, struct app_config *cfg)
{
    int i;

    cfg->ifname = "eth0";
    cfg->queue_id = 0;
    cfg->packet_size = DEFAULT_PACKET_SIZE;
    cfg->packet_count = DEFAULT_PACKET_COUNT;
    cfg->duration_ms = DEFAULT_DURATION_MS;
    cfg->need_wakeup = true;
    cfg->mock_mode = false;
    cfg->json_out = NULL;

    for (i = 1; i < argc; i++) {
        const char *arg = argv[i];

        if (strcmp(arg, "--mock") == 0) {
            cfg->mock_mode = true;
        } else if (strcmp(arg, "--ifname") == 0 && i + 1 < argc) {
            cfg->ifname = argv[++i];
        } else if (strcmp(arg, "--queue-id") == 0 && i + 1 < argc) {
            if (parse_u32(argv[++i], &cfg->queue_id) != 0)
                return -1;
        } else if (strcmp(arg, "--duration-ms") == 0 && i + 1 < argc) {
            cfg->duration_ms = atoi(argv[++i]);
        } else if (strcmp(arg, "--packet-size") == 0 && i + 1 < argc) {
            if (parse_u32(argv[++i], &cfg->packet_size) != 0)
                return -1;
        } else if (strcmp(arg, "--count") == 0 && i + 1 < argc) {
            if (parse_u64(argv[++i], &cfg->packet_count) != 0)
                return -1;
        } else if (strcmp(arg, "--need-wakeup") == 0 && i + 1 < argc) {
            if (parse_bool_flag(argv[++i], &cfg->need_wakeup) != 0)
                return -1;
        } else if (strcmp(arg, "--json-out") == 0 && i + 1 < argc) {
            cfg->json_out = argv[++i];
        } else if (strcmp(arg, "-h") == 0 || strcmp(arg, "--help") == 0) {
            usage(argv[0]);
            exit(0);
        } else {
            return -1;
        }
    }

    if (cfg->packet_size == 0 || cfg->packet_count == 0 || cfg->duration_ms <= 0)
        return -1;

    return 0;
}

static int write_json(const char *path, const struct benchmark_result *result)
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
        "  \"mode\": \"%s\",\n"
        "  \"packet_count\": %" PRIu64 ",\n"
        "  \"packet_size\": %u,\n"
        "  \"payload_bytes\": %" PRIu64 ",\n"
        "  \"elapsed_sec\": %.9f,\n"
        "  \"packets_per_sec\": %.3f,\n"
        "  \"mb_per_sec\": %.3f,\n"
        "  \"estimated_copy_avoidance\": %s,\n"
        "  \"real_af_xdp\": %s\n"
        "}\n",
        result->mode,
        result->packet_count,
        result->packet_size,
        result->payload_bytes,
        result->elapsed_sec,
        result->packets_per_sec,
        result->mb_per_sec,
        result->estimated_copy_avoidance ? "true" : "false",
        result->real_af_xdp ? "true" : "false");

    fclose(fp);
    return 0;
}

static int run_mock_mode(const struct app_config *cfg, struct benchmark_result *result)
{
    const uint64_t total_ops = cfg->packet_count;
    const uint64_t total_bytes = total_ops * (uint64_t)cfg->packet_size;
    volatile uint64_t accumulator = 0;
    uint64_t start_ns;
    uint64_t end_ns;
    uint64_t i;

    start_ns = monotonic_ns();
    for (i = 0; i < total_ops; i++) {
        /* Simulate per-packet queue-touch work without claiming real zero-copy. */
        accumulator += (i * 1315423911ULL) ^ (uint64_t)cfg->packet_size;
        accumulator ^= accumulator >> 7;
    }
    end_ns = monotonic_ns();

    result->mode = "af_xdp_mock";
    result->packet_count = total_ops;
    result->packet_size = cfg->packet_size;
    result->payload_bytes = total_bytes;
    result->elapsed_sec = (double)(end_ns - start_ns) / 1000000000.0;
    result->packets_per_sec = result->packet_count / result->elapsed_sec;
    result->mb_per_sec = bytes_to_mb(total_bytes) / result->elapsed_sec;
    result->estimated_copy_avoidance = true;
    result->real_af_xdp = false;

    fprintf(stdout,
        "AF_XDP mock complete: packets=%" PRIu64 " bytes=%" PRIu64 " elapsed=%.6fs pps=%.3f MB/s=%.3f checksum=%" PRIu64 "\n",
        result->packet_count,
        result->payload_bytes,
        result->elapsed_sec,
        result->packets_per_sec,
        result->mb_per_sec,
        accumulator);
    return 0;
}

#ifndef AFXDP_MOCK_ONLY
static int prefill_umem(struct xsk_ring_prod *fq)
{
    uint32_t idx;
    uint32_t i;
    int ret;

    ret = xsk_ring_prod__reserve(fq, FQ_RING_SIZE, &idx);
    if (ret != FQ_RING_SIZE)
        return -ENOSPC;

    for (i = 0; i < FQ_RING_SIZE; i++)
        *xsk_ring_prod__fill_addr(fq, idx + i) = (uint64_t)i * FRAME_SIZE;

    xsk_ring_prod__submit(fq, FQ_RING_SIZE);
    return 0;
}

static int drain_tx_completions(struct afxdp_runtime *rt)
{
    uint32_t idx_cq = 0;
    uint32_t idx_fq = 0;
    uint32_t reclaimed;
    uint32_t i;
    int ret;

    reclaimed = xsk_ring_cons__peek(&rt->cq, BATCH_SIZE, &idx_cq);
    if (reclaimed == 0)
        return 0;

    ret = xsk_ring_prod__reserve(&rt->fq, reclaimed, &idx_fq);
    if (ret != (int)reclaimed)
        return -ENOSPC;

    for (i = 0; i < reclaimed; i++)
        *xsk_ring_prod__fill_addr(&rt->fq, idx_fq + i) = *xsk_ring_cons__comp_addr(&rt->cq, idx_cq + i);

    xsk_ring_prod__submit(&rt->fq, reclaimed);
    xsk_ring_cons__release(&rt->cq, reclaimed);
    rt->tx_completions += reclaimed;
    return 0;
}

static int configure_umem(struct afxdp_runtime *rt)
{
    struct xsk_umem_config cfg;
    size_t umem_size = (size_t)NUM_FRAMES * FRAME_SIZE;
    int ret;

    memset(&cfg, 0, sizeof(cfg));
    cfg.fill_size = FQ_RING_SIZE;
    cfg.comp_size = CQ_RING_SIZE;
    cfg.frame_size = FRAME_SIZE;
    cfg.frame_headroom = 0;
    cfg.flags = 0;

    ret = posix_memalign(&rt->buffer, getpagesize(), umem_size);
    if (ret != 0)
        return -ret;

    memset(rt->buffer, 0, umem_size);

    ret = mlock(rt->buffer, umem_size);
    if (ret != 0) {
        perror("mlock");
        free(rt->buffer);
        rt->buffer = NULL;
        return -errno;
    }

    ret = xsk_umem__create(&rt->umem, rt->buffer, umem_size, &rt->fq, &rt->cq, &cfg);
    if (ret != 0)
        return ret;

    return prefill_umem(&rt->fq);
}

static int configure_socket(struct afxdp_runtime *rt, const struct app_config *cfg)
{
    struct xsk_socket_config socket_cfg;

    memset(&socket_cfg, 0, sizeof(socket_cfg));
    socket_cfg.rx_size = RX_RING_SIZE;
    socket_cfg.tx_size = TX_RING_SIZE;
    socket_cfg.xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST;
    socket_cfg.bind_flags = XDP_ZEROCOPY | (cfg->need_wakeup ? XDP_USE_NEED_WAKEUP : 0);
    socket_cfg.libbpf_flags = 0;

    return xsk_socket__create(&rt->xsk, cfg->ifname, cfg->queue_id, rt->umem, &rt->rx, &rt->tx, &socket_cfg);
}

static int recycle_rx_frames(struct afxdp_runtime *rt, struct xdp_desc *descs, uint32_t count)
{
    uint32_t idx_fq;
    uint32_t i;
    int ret;

    ret = xsk_ring_prod__reserve(&rt->fq, count, &idx_fq);
    if (ret != (int)count)
        return -ENOSPC;

    for (i = 0; i < count; i++)
        *xsk_ring_prod__fill_addr(&rt->fq, idx_fq + i) = descs[i].addr;

    xsk_ring_prod__submit(&rt->fq, count);
    return 0;
}

static int run_rx_loop(struct afxdp_runtime *rt, int duration_ms)
{
    struct pollfd pfd;
    uint64_t deadline_ns = monotonic_ns() + (uint64_t)duration_ms * 1000000ULL;
    struct xdp_desc descs[BATCH_SIZE];

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = xsk_socket__fd(rt->xsk);
    pfd.events = POLLIN;

    while (monotonic_ns() < deadline_ns) {
        uint32_t idx_rx = 0;
        uint32_t rcvd;
        uint32_t i;

        rcvd = xsk_ring_cons__peek(&rt->rx, BATCH_SIZE, &idx_rx);
        if (rcvd == 0) {
            if (xsk_ring_prod__needs_wakeup(&rt->fq)) {
                rt->rx_wakeups++;
                (void)poll(&pfd, 1, 100);
            } else {
                (void)poll(&pfd, 1, 10);
            }
            continue;
        }

        for (i = 0; i < rcvd; i++) {
            const struct xdp_desc *desc = xsk_ring_cons__rx_desc(&rt->rx, idx_rx + i);
            descs[i] = *desc;
            rt->packets_rx++;
            rt->bytes_rx += desc->len;
        }

        xsk_ring_cons__release(&rt->rx, rcvd);

        if (recycle_rx_frames(rt, descs, rcvd) != 0)
            return -1;

        if (drain_tx_completions(rt) != 0)
            return -1;
    }

    return 0;
}

static int run_real_mode(const struct app_config *cfg, struct benchmark_result *result)
{
    struct afxdp_runtime rt;
    unsigned int ifindex;
    uint64_t start_ns;
    uint64_t end_ns;
    int ret;

    memset(&rt, 0, sizeof(rt));
    ifindex = if_nametoindex(cfg->ifname);
    if (ifindex == 0) {
        fprintf(stderr, "failed to resolve interface %s: %s\n", cfg->ifname, strerror(errno));
        return 1;
    }

    ret = configure_umem(&rt);
    if (ret != 0) {
        fprintf(stderr, "configure_umem failed: %d\n", ret);
        return 1;
    }

    ret = configure_socket(&rt, cfg);
    if (ret != 0) {
        fprintf(stderr, "configure_socket failed: %d\n", ret);
        xsk_umem__delete(rt.umem);
        munlock(rt.buffer, (size_t)NUM_FRAMES * FRAME_SIZE);
        free(rt.buffer);
        return 1;
    }

    fprintf(stdout,
        "AF_XDP socket ready on %s (ifindex=%u queue=%u duration_ms=%d need_wakeup=%d)\n",
        cfg->ifname,
        ifindex,
        cfg->queue_id,
        cfg->duration_ms,
        cfg->need_wakeup ? 1 : 0);

    start_ns = monotonic_ns();
    ret = run_rx_loop(&rt, cfg->duration_ms);
    end_ns = monotonic_ns();

    result->mode = "af_xdp_real";
    result->packet_count = rt.packets_rx;
    result->packet_size = cfg->packet_size;
    result->payload_bytes = rt.bytes_rx;
    result->elapsed_sec = (double)(end_ns - start_ns) / 1000000000.0;
    result->packets_per_sec = result->elapsed_sec > 0.0 ? rt.packets_rx / result->elapsed_sec : 0.0;
    result->mb_per_sec = result->elapsed_sec > 0.0 ? bytes_to_mb(rt.bytes_rx) / result->elapsed_sec : 0.0;
    result->estimated_copy_avoidance = true;
    result->real_af_xdp = true;

    fprintf(stdout,
        "AF_XDP real run stats: packets=%" PRIu64 " bytes=%" PRIu64 " wakeups=%" PRIu64 " tx_completions=%" PRIu64 "\n",
        rt.packets_rx,
        rt.bytes_rx,
        rt.rx_wakeups,
        rt.tx_completions);

    xsk_socket__delete(rt.xsk);
    xsk_umem__delete(rt.umem);
    munlock(rt.buffer, (size_t)NUM_FRAMES * FRAME_SIZE);
    free(rt.buffer);
    return ret == 0 ? 0 : 1;
}
#endif

int main(int argc, char **argv)
{
    struct app_config cfg;
    struct benchmark_result result;
    int ret;

    if (parse_args(argc, argv, &cfg) != 0) {
        usage(argv[0]);
        return 1;
    }

    memset(&result, 0, sizeof(result));

#ifdef AFXDP_MOCK_ONLY
    cfg.mock_mode = true;
#endif

    if (cfg.mock_mode) {
        ret = run_mock_mode(&cfg, &result);
    } else {
#ifdef AFXDP_MOCK_ONLY
        fprintf(stderr, "real AF_XDP requested, but this build only supports mock mode\n");
        return 1;
#else
        ret = run_real_mode(&cfg, &result);
#endif
    }

    if (ret != 0)
        return ret;

    if (write_json(cfg.json_out, &result) != 0)
        return 1;

    return 0;
}
