CC ?= cc
CLANG ?= clang
CFLAGS ?= -Wall -Wextra -Werror -O2 -g
PKG_CONFIG ?= pkg-config

BUILD_DIR := build
MULTIARCH_INC := /usr/include/$(shell $(CC) -print-multiarch 2>/dev/null)

LIBBPF_CFLAGS := $(shell $(PKG_CONFIG) --cflags libbpf 2>/dev/null)
LIBBPF_LIBS := $(shell $(PKG_CONFIG) --libs libbpf 2>/dev/null)
LIBIBVERBS_CFLAGS := $(shell $(PKG_CONFIG) --cflags libibverbs 2>/dev/null)
LIBIBVERBS_LIBS := $(shell $(PKG_CONFIG) --libs libibverbs 2>/dev/null)
LIBURING_CFLAGS := $(shell $(PKG_CONFIG) --cflags liburing 2>/dev/null)
LIBURING_LIBS := $(shell $(PKG_CONFIG) --libs liburing 2>/dev/null)

.PHONY: all af_xdp io_uring rdma xdp_prog clean

all: af_xdp io_uring rdma xdp_prog

$(BUILD_DIR):
	mkdir -p $(BUILD_DIR)

check-pkg-config:
	@command -v $(PKG_CONFIG) >/dev/null 2>&1 || (echo "missing dependency: pkg-config"; exit 1)

check-libbpf: check-pkg-config
	@$(PKG_CONFIG) --exists libbpf || (echo "missing dependency: libbpf (install libbpf-dev)"; exit 1)

check-liburing: check-pkg-config
	@$(PKG_CONFIG) --exists liburing || (echo "missing dependency: liburing (install liburing-dev)"; exit 1)

check-libibverbs: check-pkg-config
	@$(PKG_CONFIG) --exists libibverbs || (echo "missing dependency: libibverbs (install libibverbs-dev)"; exit 1)

check-clang:
	@command -v $(CLANG) >/dev/null 2>&1 || (echo "missing dependency: clang"; exit 1)

af_xdp: $(BUILD_DIR) check-libbpf
	$(CC) $(CFLAGS) $(LIBBPF_CFLAGS) src/af_xdp/main.c -o $(BUILD_DIR)/af_xdp_main $(LIBBPF_LIBS)

io_uring: $(BUILD_DIR) check-liburing
	$(CC) $(CFLAGS) $(LIBURING_CFLAGS) src/io_uring/recv_zc.c -o $(BUILD_DIR)/recv_zc $(LIBURING_LIBS)

rdma: $(BUILD_DIR) check-libibverbs
	$(CC) $(CFLAGS) $(LIBIBVERBS_CFLAGS) src/rdma/verbs_ping.c -o $(BUILD_DIR)/verbs_ping $(LIBIBVERBS_LIBS)

xdp_prog: $(BUILD_DIR) check-clang
	$(CLANG) -O2 -g -target bpf -I$(MULTIARCH_INC) -c src/af_xdp/xdp_pass.c -o $(BUILD_DIR)/xdp_pass.o

clean:
	rm -rf $(BUILD_DIR)
