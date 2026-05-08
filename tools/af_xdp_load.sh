#!/usr/bin/env bash
set -euo pipefail

IFACE="${1:-}"
OBJ="${2:-build/xdp_pass.o}"
SECTION="${3:-xdp}"
MODE="${XDP_MODE:-native}"

usage() {
  cat <<'EOF'
Usage:
  ./tools/af_xdp_load.sh <iface> [bpf_object] [section]

Examples:
  ./tools/af_xdp_load.sh eth0
  XDP_MODE=generic ./tools/af_xdp_load.sh eth0 build/xdp_pass.o xdp

Notes:
  - default object path: build/xdp_pass.o
  - XDP_MODE may be native, generic, or offload
  - detach with: sudo ip link set dev <iface> xdp off
EOF
}

if [ -z "$IFACE" ]; then
  usage
  exit 1
fi

if [ ! -f "$OBJ" ]; then
  echo "BPF object not found: $OBJ" >&2
  echo "Run 'make xdp_prog' first." >&2
  exit 1
fi

MODE_FLAGS=()
case "$MODE" in
  native)
    MODE_FLAGS=(-force)
    ;;
  generic)
    MODE_FLAGS=(xdpgeneric)
    ;;
  offload)
    MODE_FLAGS=(xdpoffload)
    ;;
  *)
    echo "Unsupported XDP_MODE: $MODE" >&2
    exit 1
    ;;
esac

sudo ip link set dev "$IFACE" xdp off 2>/dev/null || true

if [ "$MODE" = "native" ]; then
  sudo ip link set dev "$IFACE" xdp obj "$OBJ" sec "$SECTION"
else
  sudo ip link set dev "$IFACE" "${MODE_FLAGS[0]}" obj "$OBJ" sec "$SECTION"
fi

echo "Attached $OBJ:$SECTION to $IFACE using mode=$MODE"
echo "Next step: run build/af_xdp_main $IFACE <queue_id> <duration_ms> [need_wakeup]"
