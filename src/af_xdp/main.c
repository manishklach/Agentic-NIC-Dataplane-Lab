#include <errno.h>
#include <net/if.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>
#include <linux/if_xdp.h>

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
     * - fill/completion ring setup
     * - RX/TX ring mmap
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
