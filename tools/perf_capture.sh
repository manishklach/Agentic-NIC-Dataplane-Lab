#!/usr/bin/env bash
set -euo pipefail

usage() {
  cat <<'EOF'
usage: perf_capture.sh [--output-dir DIR] [--flamegraph-dir DIR] -- COMMAND [ARGS...]

Examples:
  sudo ./tools/perf_capture.sh --output-dir perf/kernel-udp -- ./build/udp_client --host 127.0.0.1 --port 9000 --packet-size 256 --count 2000
  sudo ./tools/perf_capture.sh --output-dir perf/af-xdp-mock -- ./build/af_xdp_main --mock --packet-size 256 --count 2000

Notes:
  - This helper requires Linux `perf`.
  - Flamegraph generation is optional. If --flamegraph-dir points to a clone of Brendan Gregg's FlameGraph repo,
    the script will also emit folded stacks and an SVG flamegraph.
EOF
}

OUTPUT_DIR="perf-output"
FLAMEGRAPH_DIR="${FLAMEGRAPH_DIR:-}"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --output-dir)
      OUTPUT_DIR="$2"
      shift 2
      ;;
    --flamegraph-dir)
      FLAMEGRAPH_DIR="$2"
      shift 2
      ;;
    --help|-h)
      usage
      exit 0
      ;;
    --)
      shift
      break
      ;;
    *)
      echo "unknown argument: $1" >&2
      usage >&2
      exit 1
      ;;
  esac
done

if [[ $# -eq 0 ]]; then
  echo "missing command to profile" >&2
  usage >&2
  exit 1
fi

if ! command -v perf >/dev/null 2>&1; then
  echo "missing dependency: perf" >&2
  exit 1
fi

mkdir -p "$OUTPUT_DIR"

PERF_DATA="$OUTPUT_DIR/perf.data"
REPORT_TXT="$OUTPUT_DIR/perf-report.txt"
SCRIPT_TXT="$OUTPUT_DIR/perf-script.txt"
FOLDED_TXT="$OUTPUT_DIR/perf.folded"
FLAMEGRAPH_SVG="$OUTPUT_DIR/flamegraph.svg"

echo "recording perf profile to $PERF_DATA"
perf record -F 199 -g --output "$PERF_DATA" -- "$@"

echo "writing perf report to $REPORT_TXT"
perf report --stdio --input "$PERF_DATA" >"$REPORT_TXT"

echo "writing perf script output to $SCRIPT_TXT"
perf script --input "$PERF_DATA" >"$SCRIPT_TXT"

if [[ -n "$FLAMEGRAPH_DIR" ]]; then
  STACK_COLLAPSE="$FLAMEGRAPH_DIR/stackcollapse-perf.pl"
  FLAMEGRAPH_PL="$FLAMEGRAPH_DIR/flamegraph.pl"
  if [[ -x "$STACK_COLLAPSE" && -x "$FLAMEGRAPH_PL" ]]; then
    echo "writing folded stacks to $FOLDED_TXT"
    "$STACK_COLLAPSE" "$SCRIPT_TXT" >"$FOLDED_TXT"
    echo "writing flamegraph SVG to $FLAMEGRAPH_SVG"
    "$FLAMEGRAPH_PL" "$FOLDED_TXT" >"$FLAMEGRAPH_SVG"
  else
    echo "skipping flamegraph generation: expected executable scripts in $FLAMEGRAPH_DIR" >&2
  fi
else
  echo "skipping flamegraph generation: pass --flamegraph-dir /path/to/FlameGraph to enable it"
fi

echo "perf capture complete"
echo "  perf data:      $PERF_DATA"
echo "  perf report:    $REPORT_TXT"
echo "  perf script:    $SCRIPT_TXT"
if [[ -f "$FOLDED_TXT" ]]; then
  echo "  folded stacks:  $FOLDED_TXT"
fi
if [[ -f "$FLAMEGRAPH_SVG" ]]; then
  echo "  flamegraph SVG: $FLAMEGRAPH_SVG"
fi
