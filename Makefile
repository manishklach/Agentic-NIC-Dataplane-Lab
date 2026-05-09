CC ?= cc
CLANG ?= clang
PYTHON ?= python3
CFLAGS ?= -Wall -Wextra -Werror -O2 -g
PKG_CONFIG ?= pkg-config

BUILD_DIR := build
MULTIARCH_INC := /usr/include/$(shell $(CC) -print-multiarch 2>/dev/null)

LIBBPF_CFLAGS := $(shell $(PKG_CONFIG) --cflags libbpf 2>/dev/null)
LIBBPF_LIBS := $(shell $(PKG_CONFIG) --libs libbpf 2>/dev/null)
LIBXDP_CFLAGS := $(shell $(PKG_CONFIG) --cflags libxdp 2>/dev/null)
LIBXDP_LIBS := $(shell $(PKG_CONFIG) --libs libxdp 2>/dev/null)
LIBIBVERBS_CFLAGS := $(shell $(PKG_CONFIG) --cflags libibverbs 2>/dev/null)
LIBIBVERBS_LIBS := $(shell $(PKG_CONFIG) --libs libibverbs 2>/dev/null)
LIBURING_CFLAGS := $(shell $(PKG_CONFIG) --cflags liburing 2>/dev/null)
LIBURING_LIBS := $(shell $(PKG_CONFIG) --libs liburing 2>/dev/null)

ifeq ($(strip $(LIBXDP_LIBS)),)
ifeq ($(strip $(LIBBPF_LIBS)),)
AFXDP_CFLAGS :=
AFXDP_LIBS :=
AFXDP_DEFINES := -DAFXDP_MOCK_ONLY=1
else
AFXDP_CFLAGS := $(LIBBPF_CFLAGS)
AFXDP_LIBS := $(LIBBPF_LIBS)
AFXDP_DEFINES :=
endif
else
AFXDP_CFLAGS := $(LIBXDP_CFLAGS)
AFXDP_LIBS := $(LIBXDP_LIBS)
AFXDP_DEFINES :=
endif

.PHONY: all kernel_udp af_xdp io_uring rdma xdp_prog clean demo

all: kernel_udp af_xdp io_uring rdma xdp_prog

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

check-pkg-config:
	@command -v $(PKG_CONFIG) >/dev/null 2>&1 || (echo "missing dependency: pkg-config"; exit 1)

check-liburing: check-pkg-config
	@$(PKG_CONFIG) --exists liburing || (echo "missing dependency: liburing (install liburing-dev)"; exit 1)

check-libibverbs: check-pkg-config
	@$(PKG_CONFIG) --exists libibverbs || (echo "missing dependency: libibverbs (install libibverbs-dev)"; exit 1)

check-clang:
	@command -v $(CLANG) >/dev/null 2>&1 || (echo "missing dependency: clang"; exit 1)

kernel_udp: $(BUILD_DIR)
	$(CC) $(CFLAGS) src/kernel_udp/udp_echo_server.c -o $(BUILD_DIR)/udp_echo_server
	$(CC) $(CFLAGS) src/kernel_udp/udp_client.c -o $(BUILD_DIR)/udp_client -lm

af_xdp: $(BUILD_DIR)
	$(CC) $(CFLAGS) $(AFXDP_DEFINES) $(AFXDP_CFLAGS) src/af_xdp/main.c -o $(BUILD_DIR)/af_xdp_main $(AFXDP_LIBS)

io_uring: $(BUILD_DIR)
	@if command -v $(PKG_CONFIG) >/dev/null 2>&1 && $(PKG_CONFIG) --exists liburing; then \
		$(CC) $(CFLAGS) $(LIBURING_CFLAGS) src/io_uring/recv_zc.c -o $(BUILD_DIR)/recv_zc $(LIBURING_LIBS); \
	else \
		echo "skipping io_uring build: liburing-dev not installed"; \
	fi

rdma: $(BUILD_DIR)
	@if command -v $(PKG_CONFIG) >/dev/null 2>&1 && $(PKG_CONFIG) --exists libibverbs; then \
		$(CC) $(CFLAGS) $(LIBIBVERBS_CFLAGS) src/rdma/verbs_ping.c -o $(BUILD_DIR)/verbs_ping $(LIBIBVERBS_LIBS); \
	else \
		echo "skipping RDMA build: libibverbs-dev not installed"; \
	fi

xdp_prog: $(BUILD_DIR)
	@if command -v $(CLANG) >/dev/null 2>&1 && [ -f src/af_xdp/xdp_pass.c ]; then \
		$(CLANG) -O2 -g -target bpf -I$(MULTIARCH_INC) -c src/af_xdp/xdp_pass.c -o $(BUILD_DIR)/xdp_pass.o; \
	else \
		echo "skipping XDP program build: clang not installed"; \
	fi

clean:
	rm -rf $(BUILD_DIR)

demo: all
	./scripts/run_local_baseline.sh
