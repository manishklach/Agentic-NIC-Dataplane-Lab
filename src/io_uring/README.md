# io_uring Starter

This directory holds a minimal `io_uring` receive skeleton for the `Path A` kernel-RPC optimization path.

The starter focuses on:

- ring setup
- socket preparation
- `IORING_OP_RECV_ZC` submission shape
- the difference between payload and notification completions

It does not yet implement a complete benchmark-ready event loop.
