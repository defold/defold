#!/usr/bin/env bash
set -euo pipefail

if [[ $# -lt 1 ]]; then
  echo "Usage: $0 <local-binary> [remote-binary-path] [-- arg1 arg2 ...]"
  exit 1
fi

LOCAL_BIN="$1"
shift

REMOTE_BIN="${1:-}"
if [[ $# -gt 0 && "$1" != "--" ]]; then
  shift
fi

ARGS=()
if [[ $# -gt 0 && "$1" == "--" ]]; then
  shift
  ARGS=("$@")
fi

: "${ANDROID_NDK:?Please set ANDROID_NDK to your NDK root}"
ADB="${ADB:-adb}"
PORT="${LLDB_PORT:-5039}"

if [[ ! -f "$LOCAL_BIN" ]]; then
  echo "Local binary not found: $LOCAL_BIN"
  exit 1
fi

ABI="$($ADB shell getprop ro.product.cpu.abi 2>/dev/null | tr -d '\r')"

case "$ABI" in
  arm64-v8a)   LLDB_ARCH="aarch64" ;;
  armeabi-v7a) LLDB_ARCH="arm" ;;
  x86_64)      LLDB_ARCH="x86_64" ;;
  x86)         LLDB_ARCH="i386" ;;
  *)
    echo "Unsupported/unknown device ABI: $ABI"
    exit 1
    ;;
esac

HOST_TAG=""
case "$(uname -s)-$(uname -m)" in
  Linux-x86_64)  HOST_TAG="linux-x86_64" ;;
  Darwin-x86_64) HOST_TAG="darwin-x86_64" ;;
  Darwin-arm64)  HOST_TAG="darwin-x86_64" ;; # NDK commonly ships darwin-x86_64 or darwin-arm64 depending on version
  *)
    echo "Unsupported host platform: $(uname -s)-$(uname -m)"
    exit 1
    ;;
esac

LLDB_SERVER="$(find "$ANDROID_NDK/toolchains/llvm/prebuilt/$HOST_TAG" -path "*/lib/linux/$LLDB_ARCH/lldb-server" | head -n 1)"
if [[ -z "$LLDB_SERVER" ]]; then
  LLDB_SERVER="$(find "$ANDROID_NDK/toolchains/llvm/prebuilt" -path "*/lib/linux/$LLDB_ARCH/lldb-server" | head -n 1)"
fi
if [[ -z "$LLDB_SERVER" ]]; then
  echo "Could not locate lldb-server in NDK"
  exit 1
fi

if [[ -z "$REMOTE_BIN" ]]; then
  REMOTE_BIN="/data/local/tmp/$(basename "$LOCAL_BIN")"
fi

LLDB_BIN=${ANDROID_NDK}/toolchains/llvm/prebuilt/darwin-x86_64/bin/lldb
#LLDB_BIN="${LLDB_BIN:-lldb}"

echo "ABI:         $ABI"
echo "lldb-server: $LLDB_SERVER"
echo "remote bin:  $REMOTE_BIN"
echo "port:        $PORT"
echo "NDK:"        $ANDROID_NDK
echo "LLDB:"       $LLDB_BIN

$ADB push "$LOCAL_BIN" "$REMOTE_BIN"
$ADB shell "chmod 755 '$REMOTE_BIN'"

$ADB push "$LLDB_SERVER" /data/local/tmp/lldb-server
$ADB shell "chmod 755 /data/local/tmp/lldb-server"

$ADB forward "tcp:$PORT" "tcp:$PORT"

echo "Starting lldb-server on device..."
#$ADB shell "pkill -f lldb-server || true; nohup /data/local/tmp/lldb-server platform --listen '*:$PORT' --server >/data/local/tmp/lldb-server.log 2>&1 </dev/null &"
$ADB shell "nohup /data/local/tmp/lldb-server platform --listen '*:$PORT' --server >/data/local/tmp/lldb-server.log 2>&1 </dev/null &"
#$ADB shell "pkill -f lldb-server || true; /data/local/tmp/lldb-server platform --server --listen localhost:$PORT"
#$ADB shell "pkill -f lldb-server || true; /data/local/tmp/lldb-server gdbserver :$PORT $REMOTE_BIN"

#$ADB shell "echo foo; /data/local/tmp/lldb-server gdbserver :$PORT $REMOTE_BIN"

sleep 1

LLDB_CMDS=$(mktemp)
cat > "$LLDB_CMDS" <<EOF
platform select remote-android
platform connect connect://:$PORT
platform shell cd /data/local/tmp
target create $REMOTE_BIN
settings set target.run-args ${ARGS[*]:-}
EOF

echo
echo "Connected setup complete."
echo "Inside LLDB, use:"
echo "  run"
echo "or set breakpoints first, e.g.:"
echo "  breakpoint set --name main"
echo "  run"
echo

"$LLDB_BIN" -s "$LLDB_CMDS"
