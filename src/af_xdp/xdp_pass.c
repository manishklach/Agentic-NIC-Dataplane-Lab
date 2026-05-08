#include <linux/bpf.h>
#include <bpf/bpf_helpers.h>

/*
 * Minimal XDP companion program for the AF_XDP path.
 *
 * This program intentionally only passes traffic today. The next step is to
 * redirect selected flows into an XSK map once the userspace socket path and
 * queue ownership model are in place.
 */

SEC("xdp")
int xdp_pass_prog(struct xdp_md *ctx)
{
    (void)ctx;
    return XDP_PASS;
}

char LICENSE[] SEC("license") = "GPL";
