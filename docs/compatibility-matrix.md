# Compatibility Matrix

This table is intentionally conservative. It is meant to help contributors decide whether a target host is in-range before they spend time debugging feature gaps.

| Feature | Minimum kernel guidance | Intel path | Broadcom path | Notes |
| --- | --- | --- | --- | --- |
| `AF_XDP` baseline bring-up | `5.11+` | `ice` with XDP and AF_XDP support | `bnxt_en` if XDP path is validated on target stack | Driver support matters more than API headers alone |
| `XDP_USE_NEED_WAKEUP` | `5.3+` | test on `ice` queue model | test on `bnxt_en` queue model | Reduces needless syscalls when queue state is known |
| `io_uring` `IORING_OP_RECV_ZC` | `6.0+` | validate NIC header/data split and steering support | validate stack support case-by-case | This is the key Path A optimization |
| Userspace RDMA verbs | distro and `rdma-core` dependent | `irdma` | `bnxt_re` | Firmware and RoCE configuration can dominate bring-up pain |
| eBPF/XDP program build | modern LLVM/clang | supported | supported | Keep clang and kernel headers aligned |
| RoCE bulk path | platform dependent | `irdma` with compatible `ice` stack | `bnxt_re` with compatible `bnxt_en` stack | Validate congestion behavior on the real fabric |

## Recommended Recording Fields

For every host and NIC combination, record:

- kernel version
- distro version
- NIC model
- firmware version
- Ethernet driver version
- RDMA driver version
- `rdma-core` version
- whether `AF_XDP` zero-copy works
- whether `RECV_ZC` is available and stable

## Why This Doc Exists

The biggest wasted effort in dataplane experimentation usually comes from trying to debug a missing feature that was never supported by the target combination in the first place. This file is here to make that explicit early.
