#!/usr/bin/env bash
set -euo pipefail

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUILD_DIR="$ROOT_DIR/build"
RESULTS_DIR="$ROOT_DIR/results"
TIMESTAMP="$(date +%Y%m%d-%H%M%S)"
RESULT_JSON="$RESULTS_DIR/local-baseline-$TIMESTAMP.json"
LATEST_JSON="$RESULTS_DIR/latest.json"
TMP_DIR="$(mktemp -d)"
PORT="${PORT:-$((20000 + RANDOM % 20000))}"
SERVER_JSON="$TMP_DIR/kernel_udp_server.json"
CLIENT_JSON="$TMP_DIR/kernel_udp_client.json"
AFXDP_JSON="$TMP_DIR/af_xdp_mock.json"
SERVER_LOG="$TMP_DIR/kernel_udp_server.log"

cleanup() {
  if [[ -n "${SERVER_PID:-}" ]]; then
    kill "$SERVER_PID" >/dev/null 2>&1 || true
    wait "$SERVER_PID" >/dev/null 2>&1 || true
  fi
  rm -rf "$TMP_DIR"
}
trap cleanup EXIT

require_binary() {
  local path="$1"
  local hint="$2"
  if [[ ! -x "$path" ]]; then
    echo "missing binary: $path" >&2
    echo "$hint" >&2
    exit 1
  fi
}

mkdir -p "$RESULTS_DIR"

if [[ ! -x "$BUILD_DIR/udp_echo_server" || ! -x "$BUILD_DIR/udp_client" || ! -x "$BUILD_DIR/af_xdp_main" ]]; then
  echo "build artifacts missing, running make all"
  make -C "$ROOT_DIR" all
fi

require_binary "$BUILD_DIR/udp_echo_server" "run 'make all' first"
require_binary "$BUILD_DIR/udp_client" "run 'make all' first"
require_binary "$BUILD_DIR/af_xdp_main" "run 'make all' first"

echo "starting kernel UDP echo server"
"$BUILD_DIR/udp_echo_server" \
  --host 127.0.0.1 \
  --port "$PORT" \
  --packet-size 256 \
  --json-out "$SERVER_JSON" >"$SERVER_LOG" 2>&1 &
SERVER_PID=$!
sleep 1

if ! kill -0 "$SERVER_PID" >/dev/null 2>&1; then
  echo "kernel UDP server failed to start on port $PORT" >&2
  if [[ -f "$SERVER_LOG" ]]; then
    cat "$SERVER_LOG" >&2
  fi
  exit 1
fi

echo "running kernel UDP baseline client"
"$BUILD_DIR/udp_client" \
  --host 127.0.0.1 \
  --port "$PORT" \
  --packet-size 256 \
  --count 2000 \
  --json-out "$CLIENT_JSON"

kill "$SERVER_PID" >/dev/null 2>&1 || true
wait "$SERVER_PID" >/dev/null 2>&1 || true
unset SERVER_PID

echo "running AF_XDP mock baseline"
"$BUILD_DIR/af_xdp_main" \
  --mock \
  --packet-size 256 \
  --count 2000 \
  --json-out "$AFXDP_JSON"

python3 - "$CLIENT_JSON" "$AFXDP_JSON" "$RESULT_JSON" <<'PY'
import json
import pathlib
import shutil
import sys
from datetime import datetime, timezone

client_path = pathlib.Path(sys.argv[1])
afxdp_path = pathlib.Path(sys.argv[2])
out_path = pathlib.Path(sys.argv[3])
client = json.loads(client_path.read_text(encoding="utf-8"))
afxdp = json.loads(afxdp_path.read_text(encoding="utf-8"))

combined = {
    "schema_version": 1,
    "created_at": datetime.now(timezone.utc).isoformat(),
    "host": "127.0.0.1",
    "purpose": "local workflow validation for Path A vs Path B starter benchmark",
    "notes": [
        "kernel_udp_baseline is a real localhost UDP echo round-trip measurement",
        "af_xdp_mock is a simulated starter path and does not claim hardware zero-copy performance",
    ],
    "results": [client, afxdp],
}

out_path.write_text(json.dumps(combined, indent=2) + "\n", encoding="utf-8")
latest = out_path.parent / "latest.json"
shutil.copyfile(out_path, latest)
print(f"wrote combined result to {out_path}")
print(f"updated latest result at {latest}")
PY

echo "local baseline complete"
echo "result file: $RESULT_JSON"
echo "server log: $SERVER_LOG"
