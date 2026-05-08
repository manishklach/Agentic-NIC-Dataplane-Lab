#include <infiniband/verbs.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

int main(void)
{
    int num_devices = 0;
    struct ibv_device **list = ibv_get_device_list(&num_devices);
    struct ibv_context *ctx;
    struct ibv_pd *pd;
    struct ibv_cq *cq;
    struct ibv_qp *qp;
    struct ibv_qp_init_attr attr;
    char buf[4096];
    struct ibv_mr *mr;

    if (!list || num_devices == 0) {
        fprintf(stderr, "no RDMA devices found\n");
        return 1;
    }

    ctx = ibv_open_device(list[0]);
    if (!ctx) {
        fprintf(stderr, "failed to open RDMA device\n");
        ibv_free_device_list(list);
        return 1;
    }

    pd = ibv_alloc_pd(ctx);
    cq = ibv_create_cq(ctx, 128, NULL, NULL, 0);
    if (!pd || !cq) {
        fprintf(stderr, "failed to allocate PD or CQ\n");
        return 1;
    }

    memset(&attr, 0, sizeof(attr));
    attr.send_cq = cq;
    attr.recv_cq = cq;
    attr.qp_type = IBV_QPT_RC;
    attr.cap.max_send_wr = 128;
    attr.cap.max_recv_wr = 128;
    attr.cap.max_send_sge = 1;
    attr.cap.max_recv_sge = 1;

    qp = ibv_create_qp(pd, &attr);
    if (!qp) {
        fprintf(stderr, "failed to create QP\n");
        return 1;
    }

    memset(buf, 0, sizeof(buf));
    strcpy(buf, "rdma starter");
    mr = ibv_reg_mr(
        pd,
        buf,
        sizeof(buf),
        IBV_ACCESS_LOCAL_WRITE | IBV_ACCESS_REMOTE_READ | IBV_ACCESS_REMOTE_WRITE
    );
    if (!mr) {
        fprintf(stderr, "failed to register MR\n");
        return 1;
    }

    printf("RDMA context ready, device=%s lkey=%u\n", ibv_get_device_name(list[0]), mr->lkey);
    printf("Next steps: RESET->INIT->RTR->RTS, peer exchange, post send/recv, and CQ polling\n");

    ibv_dereg_mr(mr);
    ibv_destroy_qp(qp);
    ibv_destroy_cq(cq);
    ibv_dealloc_pd(pd);
    ibv_close_device(ctx);
    ibv_free_device_list(list);
    return 0;
}
