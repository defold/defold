#!/bin/zsh
set -euo pipefail

cd "$(dirname "$0")"

mkdir -p build

SDKROOT="$(xcrun --sdk macosx --show-sdk-path)"

xcrun clang++ \
    -std=c++17 \
    -fobjc-arc \
    -Wall \
    -Wextra \
    -isysroot "$SDKROOT" \
    feather_harness.mm \
    -framework AppKit \
    -framework Foundation \
    -framework Metal \
    -framework MetalKit \
    -framework QuartzCore \
    -framework CoreGraphics \
    -framework ImageIO \
    -framework CoreFoundation \
    -o build/feather_harness

echo "Built feather/build/feather_harness"
