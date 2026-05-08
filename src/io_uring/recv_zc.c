#include <arpa/inet.h>
#include <errno.h>
#include <liburing.h>
#include <netinet/in.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

/*
 * Minimal io_uring RECV_ZC skeleton for Path A.
 *
 * This sample focuses on:
 * - ring creation
 * - socket setup
 * - RECV_ZC submission shape
 * - CQE handling distinctions
 *
 * It is not yet a complete benchmark loop.
 */

static int make_udp_socket(unsigned short port)
{
    int fd;
    struct sockaddr_in addr;

    fd = socket(AF_INET, SOCK_DGRAM, 0);
    if (fd < 0) {
        perror("socket");
        return -1;
    }

    memset(&addr, 0, sizeof(addr));
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(port);

    if (bind(fd, (struct sockaddr *)&addr, sizeof(addr)) < 0) {
        perror("bind");
        close(fd);
        return -1;
    }

    return fd;
}

int main(int argc, char **argv)
{
    struct io_uring ring;
    struct io_uring_params params;
    struct io_uring_sqe *sqe;
    struct io_uring_cqe *cqe;
    char buf[4096];
    int sockfd;
    int ret;
    bool using_recv_zc = false;
    unsigned short port = argc > 1 ? (unsigned short)atoi(argv[1]) : 9000;

    memset(&params, 0, sizeof(params));
    params.flags = IORING_SETUP_SQPOLL;

    ret = io_uring_queue_init_params(8, &ring, &params);
    if (ret < 0) {
        fprintf(stderr, "io_uring_queue_init_params: %s\n", strerror(-ret));
        return 1;
    }

    sockfd = make_udp_socket(port);
    if (sockfd < 0) {
        io_uring_queue_exit(&ring);
        return 1;
    }

    memset(buf, 0, sizeof(buf));
    sqe = io_uring_get_sqe(&ring);
    if (!sqe) {
        fprintf(stderr, "failed to acquire SQE\n");
        close(sockfd);
        io_uring_queue_exit(&ring);
        return 1;
    }

    /*
     * Some CI environments ship a liburing that has RECV_ZC opcode support in
     * the kernel headers but not the newer io_uring_prep_recv_zc() helper.
     * Build the SQE using io_uring_prep_recv() first, then upgrade the opcode
     * when RECV_ZC is available at compile time.
     */
    io_uring_prep_recv(sqe, sockfd, buf, sizeof(buf), 0);
#ifdef IORING_OP_RECV_ZC
    sqe->opcode = IORING_OP_RECV_ZC;
    using_recv_zc = true;
#endif
    sqe->user_data = 1;

    ret = io_uring_submit(&ring);
    if (ret < 0) {
        fprintf(stderr, "io_uring_submit: %s\n", strerror(-ret));
        close(sockfd);
        io_uring_queue_exit(&ring);
        return 1;
    }

    ret = io_uring_wait_cqe(&ring, &cqe);
    if (ret < 0) {
        fprintf(stderr, "io_uring_wait_cqe: %s\n", strerror(-ret));
        close(sockfd);
        io_uring_queue_exit(&ring);
        return 1;
    }

    /*
     * In a full implementation:
     * - one CQE carries the receive result
     * - a later notification CQE reports zero-copy buffer lifecycle status
     * - both must be consumed correctly
     */
    printf(
        "%s completion: res=%d flags=0x%x\n",
        using_recv_zc ? "recv_zc" : "recv",
        cqe->res,
        cqe->flags
    );
    io_uring_cqe_seen(&ring, cqe);

    close(sockfd);
    io_uring_queue_exit(&ring);
    return 0;
}
