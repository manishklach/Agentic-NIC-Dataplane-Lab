#!/usr/bin/env bash
set -euo pipefail

PATH_KIND=""
WORKLOAD=""
IFACE=""
OUT=""
GENERATOR=""

usage() {
  cat <<'EOF'
Usage:
  ./scripts/benchmark-matrix.sh --path <tcp|io_uring|af_xdp|rdma> --workload <a|b|c> --iface <ifname> --out <file.json> [--generator <tool>]

Notes:
  - workload a = small agent RPC
  - workload b = retrieval / memory service
  - workload c = bulk east-west state
  - if --generator is omitted, a default tool is selected by path
EOF
}

default_generator() {
  case "$1" in
    tcp|io_uring) echo "sockperf" ;;
    af_xdp) echo "custom" ;;
    rdma) echo "ib_send_bw" ;;
    *) echo "" ;;
  esac
}

snapshot_ethtool() {
  local iface="$1"
  if command -v ethtool >/dev/null 2>&1; then
    ethtool -S "$iface" 2>/dev/null || true
  fi
}

snapshot_softirqs() {
  if [ -r /proc/softirqs ]; then
    cat /proc/softirqs
  fi
}

run_generator() {
  local generator="$1"
  case "$generator" in
    sockperf)
      if ! command -v sockperf >/dev/null 2>&1; then
        echo "sockperf not found; install it or pass --generator" >&2
        return 1
      fi
      echo "sockperf is available; wire your server/client command here"
      ;;
    ib_send_bw)
      if ! command -v ib_send_bw >/dev/null 2>&1; then
        echo "ib_send_bw not found; install perftest or pass --generator" >&2
        return 1
      fi
      echo "ib_send_bw is available; wire your peer configuration here"
      ;;
    custom)
      echo "custom generator selected; plug in the repo-specific sender here"
      ;;
    *)
      echo "unsupported generator: $generator" >&2
      return 1
      ;;
  esac
}

while [ "$#" -gt 0 ]; do
  case "$1" in
    --path)
      PATH_KIND="${2:-}"
      shift 2
      ;;
    --workload)
      WORKLOAD="${2:-}"
      shift 2
      ;;
    --iface)
      IFACE="${2:-}"
      shift 2
      ;;
    --out)
      OUT="${2:-}"
      shift 2
      ;;
    --generator)
      GENERATOR="${2:-}"
      shift 2
      ;;
    -h|--help)
      usage
      exit 0
      ;;
    *)
      echo "unknown argument: $1" >&2
      usage
      exit 1
      ;;
  esac
done

if [ -z "$PATH_KIND" ] || [ -z "$WORKLOAD" ] || [ -z "$IFACE" ] || [ -z "$OUT" ]; then
  usage
  exit 1
fi

if [ -z "$GENERATOR" ]; then
  GENERATOR="$(default_generator "$PATH_KIND")"
fi

mkdir -p "$(dirname "$OUT")"

BEFORE_SOFTIRQS="$(snapshot_softirqs)"
BEFORE_ETHTOOL="$(snapshot_ethtool "$IFACE")"
GENERATOR_STATUS="not-run"

if run_generator "$GENERATOR"; then
  GENERATOR_STATUS="invoked"
else
  GENERATOR_STATUS="failed"
fi

AFTER_SOFTIRQS="$(snapshot_softirqs)"
AFTER_ETHTOOL="$(snapshot_ethtool "$IFACE")"

cat >"$OUT" <<EOF
{
  "path": "$PATH_KIND",
  "workload": "$WORKLOAD",
  "iface": "$IFACE",
  "generator": "$GENERATOR",
  "generator_status": "$GENERATOR_STATUS",
  "kernel": "$(uname -r)",
  "hostname": "$(hostname)",
  "before_softirqs": $(printf '%s' "${BEFORE_SOFTIRQS:-}" | python3 -c 'import json,sys; print(json.dumps(sys.stdin.read()))'),
  "after_softirqs": $(printf '%s' "${AFTER_SOFTIRQS:-}" | python3 -c 'import json,sys; print(json.dumps(sys.stdin.read()))'),
  "before_ethtool": $(printf '%s' "${BEFORE_ETHTOOL:-}" | python3 -c 'import json,sys; print(json.dumps(sys.stdin.read()))'),
  "after_ethtool": $(printf '%s' "${AFTER_ETHTOOL:-}" | python3 -c 'import json,sys; print(json.dumps(sys.stdin.read()))')
}
EOF

echo "wrote benchmark envelope to $OUT"
