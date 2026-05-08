# Building

This repo is designed for Linux. The code is not expected to build on Windows hosts directly.

## Minimum Kernel Guidance

- `AF_XDP` practical support: `5.11+` recommended for a usable baseline across common distros
- `XDP_USE_NEED_WAKEUP`: available since `5.3`, but test your driver behavior
- `io_uring` zero-copy receive (`IORING_OP_RECV_ZC`): `6.0+`
- `RDMA` userspace verbs: depends more on NIC driver, firmware, and `rdma-core` than on a single kernel feature flag

## Required Packages

Debian or Ubuntu:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential \
  clang \
  llvm \
  pkg-config \
  libbpf-dev \
  libibverbs-dev \
  liburing-dev \
  linux-libc-dev \
  linux-tools-common
```

You may also want:

```bash
sudo apt-get install -y ethtool jq iperf3 linux-tools-$(uname -r)
```

## Build Targets

```bash
make all
make af_xdp
make io_uring
make rdma
make xdp_prog
make clean
```

Artifacts are written to `./build/`.

## Notes

- `AF_XDP` userspace code links against `libbpf`
- the `RDMA` userspace sample links against `libibverbs`
- the `io_uring` sample links against `liburing`
- the XDP sample compiles as an eBPF object with `clang -target bpf`

## What Builds Today

The code in this repo is intentionally starter-grade:

- it should compile cleanly
- it is structured to teach the initialization flow
- it does not yet claim to be a complete dataplane or benchmark suite

## Recommended Host Validation

Before running experiments, record:

- `uname -r`
- `clang --version`
- `pkg-config --modversion libbpf`
- `ibv_devinfo`
- `ethtool -i <iface>`
