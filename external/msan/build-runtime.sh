#!/usr/bin/env bash
set -euo pipefail

build_dir=${1:?Expected a runtime build directory}
install_dir=${2:?Expected a runtime install directory}
revision=87f0227cb60147a26a1eeb4fb06e3b505e9c7261 # LLVM 20.1.8
cc=${CC:-clang}
cxx=${CXX:-clang++}
if [[ $("$cc" -dumpversion) != 20.* ]]; then
  echo "The MSan runtime requires Clang 20, matching the Linux toolchain." >&2
  exit 1
fi

mkdir -p "$build_dir"
if [[ ! -d "$build_dir/source/.git" ]]; then
  git init "$build_dir/source"
  git -C "$build_dir/source" remote add origin https://github.com/llvm/llvm-project.git
  git -C "$build_dir/source" config remote.origin.promisor true
  git -C "$build_dir/source" config remote.origin.partialclonefilter blob:none
  git -C "$build_dir/source" fetch --depth 1 --filter=blob:none origin "$revision"
  # libc supplies shared floating-point headers used by libc++; no compiler or
  # C library is built. The sparse source and build occupy about 225 MiB.
  git -C "$build_dir/source" sparse-checkout set cmake llvm/cmake runtimes libcxx libcxxabi libc
  git -C "$build_dir/source" checkout --detach FETCH_HEAD
fi
if [[ $(git -C "$build_dir/source" rev-parse HEAD) != "$revision" ]]; then
  echo "Unexpected LLVM revision in $build_dir/source" >&2
  exit 1
fi

cmake -S "$build_dir/source/runtimes" -B "$build_dir/build" -G Ninja \
  -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_C_COMPILER="$cc" -DCMAKE_CXX_COMPILER="$cxx" \
  -DCMAKE_INSTALL_PREFIX="$install_dir" \
  '-DLLVM_ENABLE_RUNTIMES=libcxx;libcxxabi' \
  -DLLVM_USE_SANITIZER=MemoryWithOrigins \
  -DLLVM_ENABLE_PER_TARGET_RUNTIME_DIR=OFF \
  -DLIBCXX_ENABLE_SHARED=OFF -DLIBCXXABI_ENABLE_SHARED=OFF \
  -DLIBCXXABI_USE_LLVM_UNWINDER=OFF \
  -DLLVM_INCLUDE_TESTS=OFF -DLLVM_INCLUDE_DOCS=OFF \
  -DLIBCXX_INCLUDE_BENCHMARKS=OFF
cmake --build "$build_dir/build"
cmake --install "$build_dir/build"
