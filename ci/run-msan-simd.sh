#!/bin/bash
set -euo pipefail

platform=${1:?Expected a Linux target platform}
case "$platform" in
  x86_64-linux|arm64-linux) ;;
  *) echo "Unsupported MSan platform: $platform" >&2; exit 1 ;;
esac

: "${DYNAMO_HOME:?The Defold build environment must be loaded}"
: "${RUNNER_TEMP:?A temporary build directory is required}"
build_root="$RUNNER_TEMP/defold-msan-$platform"

if [[ -z "${MSAN_SYMBOLIZER_PATH:-}" ]]; then
  symbolizer=$(command -v llvm-symbolizer || command -v llvm-symbolizer-20 || true)
  if [[ -n "$symbolizer" ]]; then
    export MSAN_SYMBOLIZER_PATH="$symbolizer"
  fi
fi

# These C-only suites need no instrumented C++ runtime or external archives.
cmake -S . -B "$build_root/build" -G Ninja \
  -DTARGET_PLATFORM="$platform" \
  -DDEFOLD_SDK_ROOT="$DYNAMO_HOME" \
  -DDEFOLD_BUILD_HOME="$build_root/outputs" \
  -DDEFOLD_SELECTED_ENGINE_LIBS=dlib \
  -DDEFOLD_SKIP_BOB_LIGHT=ON \
  -DBUILD_TESTS=ON \
  -DWITH_MSAN=ON
cmake --build "$build_root/build" --target \
  run_test_simd_scalar run_test_simd run_test_simd_wide64
