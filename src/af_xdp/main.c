#include <errno.h>
#include <linux/if_link.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <linux/if_xdp.h>

/*
 * Minimal AF_XDP userspace skeleton.
 *
 * This sample intentionally stops after socket bring-up so contributors can
 * focus on one concern at a time. The next implementation step should add:
 * - UMEM allocation and registration
 * - fill and completion ring sizing
 * - RX/TX ring setup
 * - XDP_USE_NEED_WAKEUP evaluation
 * - packet drain and refill loop
 */

int main(int argc, char **argv)
{
    const char *ifname = argc > 1 ? argv[1] : "eth0";
    unsigned int ifindex = if_nametoindex(ifname);
    int fd;
    struct sockaddr_xdp addr;

    if (ifindex == 0) {
        fprintf(stderr, "failed to resolve interface %s: %s\n", ifname, strerror(errno));
        return 1;
    }

    fd = socket(AF_XDP, SOCK_RAW, 0);
    if (fd < 0) {
        perror("socket(AF_XDP)");
        return 1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sxdp_family = AF_XDP;
    addr.sxdp_ifindex = ifindex;
    addr.sxdp_queue_id = 0;
    addr.sxdp_flags = XDP_ZEROCOPY;

    /*
     * Real bring-up still needs:
     * - UMEM allocation and registration
     * - frame size and headroom decisions
     * - fill/completion ring setup
     * - RX/TX ring mmap
     * - optional XDP_USE_NEED_WAKEUP testing
     * - an attached XDP program that redirects into this socket
     */
    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind(AF_XDP)");
        close(fd);
        return 1;
    }

    printf("AF_XDP socket bound on %s (ifindex=%u)\n", ifname, ifindex);
    close(fd);
    return 0;
}
