#include <errno.h>
#include <inttypes.h>
#include <linux/if_link.h>
#include <net/if.h>
#include <poll.h>
#include <stdbool.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <time.h>
#include <unistd.h>

#include <bpf/xsk.h>

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

static uint64_t monotonic_ms(void)
{
    struct timespec ts;

    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

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
    uint32_t reclaimed = 0;
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

static int configure_socket(struct afxdp_runtime *rt, const char *ifname, uint32_t queue_id, bool need_wakeup)
{
    struct xsk_socket_config cfg;

    memset(&cfg, 0, sizeof(cfg));
    cfg.rx_size = RX_RING_SIZE;
    cfg.tx_size = TX_RING_SIZE;
    cfg.xdp_flags = XDP_FLAGS_UPDATE_IF_NOEXIST;
    cfg.bind_flags = XDP_ZEROCOPY | (need_wakeup ? XDP_USE_NEED_WAKEUP : 0);
    cfg.libbpf_flags = 0;

    return xsk_socket__create(&rt->xsk, ifname, queue_id, rt->umem, &rt->rx, &rt->tx, &cfg);
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
    uint64_t deadline_ms = monotonic_ms() + (uint64_t)duration_ms;
    struct xdp_desc descs[BATCH_SIZE];

    memset(&pfd, 0, sizeof(pfd));
    pfd.fd = xsk_socket__fd(rt->xsk);
    pfd.events = POLLIN;

    while (monotonic_ms() < deadline_ms) {
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

        if (recycle_rx_frames(rt, descs, rcvd) != 0) {
            fprintf(stderr, "failed to recycle %u RX frames back to fill ring\n", rcvd);
            return 1;
        }

        if (drain_tx_completions(rt) != 0) {
            fprintf(stderr, "failed to recycle TX completions into fill ring\n");
            return 1;
        }
    }

    return 0;
}

static void usage(const char *prog)
{
    fprintf(stderr, "usage: %s [ifname] [queue_id] [duration_ms] [need_wakeup=0|1]\n", prog);
}

int main(int argc, char **argv)
{
    struct afxdp_runtime rt;
    const char *ifname = argc > 1 ? argv[1] : "eth0";
    uint32_t queue_id = argc > 2 ? (uint32_t)strtoul(argv[2], NULL, 10) : 0;
    int duration_ms = argc > 3 ? atoi(argv[3]) : 5000;
    bool need_wakeup = argc > 4 ? atoi(argv[4]) != 0 : true;
    unsigned int ifindex;
    int ret;

    memset(&rt, 0, sizeof(rt));

    if (argc > 5) {
        usage(argv[0]);
        return 1;
    }

    ifindex = if_nametoindex(ifname);
    if (ifindex == 0) {
        fprintf(stderr, "failed to resolve interface %s: %s\n", ifname, strerror(errno));
        return 1;
    }

    ret = configure_umem(&rt);
    if (ret != 0) {
        fprintf(stderr, "configure_umem failed: %d\n", ret);
        return 1;
    }

    ret = configure_socket(&rt, ifname, queue_id, need_wakeup);
    if (ret != 0) {
        fprintf(stderr, "configure_socket failed: %d\n", ret);
        xsk_umem__delete(rt.umem);
        munlock(rt.buffer, (size_t)NUM_FRAMES * FRAME_SIZE);
        free(rt.buffer);
        return 1;
    }

    printf(
        "AF_XDP socket ready on %s (ifindex=%u, queue=%u, duration_ms=%d, need_wakeup=%d)\n",
        ifname,
        ifindex,
        queue_id,
        duration_ms,
        need_wakeup ? 1 : 0
    );
    printf("UMEM configured: %u frames, frame_size=%u, RX ring=%u, FQ=%u, CQ=%u\n",
        NUM_FRAMES,
        FRAME_SIZE,
        RX_RING_SIZE,
        FQ_RING_SIZE,
        CQ_RING_SIZE
    );

    ret = run_rx_loop(&rt, duration_ms);

    printf(
        "rx_stats packets=%" PRIu64 " bytes=%" PRIu64 " wakeups=%" PRIu64 " tx_completions=%" PRIu64 "\n",
        rt.packets_rx,
        rt.bytes_rx,
        rt.rx_wakeups,
        rt.tx_completions
    );

    xsk_socket__delete(rt.xsk);
    xsk_umem__delete(rt.umem);
    munlock(rt.buffer, (size_t)NUM_FRAMES * FRAME_SIZE);
    free(rt.buffer);
    return ret == 0 ? 0 : 1;
}
