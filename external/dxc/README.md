# DXC host tools

Windows x64 uses `packages/dxc-1.9.2609-x86_64-win32.tar.gz`. `install_ext`
extracts this package; `build_ext` does not compile DXC on Windows.
Linux and macOS continue building DXC from source through `build_ext`.
Cross-builds first prepare dependencies for the host as usual. Desktop targets
also need their own DXC library because shaderc links it, including when building
x86_64-macos on an arm64-macos host. Mobile and web targets use only the host copy.
Linux builds require a native runner for each architecture.

The pinned DXC source and its matching DirectX-Headers archive are downloaded
and SHA-256 checked by CMake. Sources and intermediate files stay in the build
directory. LLVM is configured in a separate Release build with its own C++17,
exception, and RTTI settings. These settings do not propagate into the engine.
Only `dxc`, `dxcompiler`, and the Windows validator are built; SPIR-V generation
and upstream tests are disabled. `patch.cmake` makes upstream honor its SPIR-V
option on Unix. Version stamping uses the source archive's fixed version.
Windows uses the static CRT, matching the other Defold tools.

Installed files under the SDK's `ext` directory:

- `bin/<platform>/dxc[.exe]`
- `lib/<platform>/libdxcompiler.so` or `libdxcompiler.dylib` on Unix
- `bin/<platform>/dxcompiler.dll`, `dxil.dll`, and
  `lib/<platform>/dxcompiler.lib` on Windows
- `include/dxc/` with the public API and matching portable reflection headers
- `share/licenses/dxc/`

Unix executables use a relative library search path for the installed layout.
They do not require `LD_LIBRARY_PATH` or an absolute path to a developer's build.
The package retains this layout so shaderc and Bob/editor packaging use the
same installed files as source builds.

## Rebuild the Windows package

With the Windows SDK set up, run from the repository root in the Defold shell:

```sh
./scripts/build.py build_external --package=dxc --platform=x86_64-win32
./scripts/build.py install_ext_packages --platform=x86_64-win32
```

The first command builds the pinned sources with Defold's toolchain and writes
`packages/dxc-1.9.2609-x86_64-win32.tar.gz`. The archive includes the executable,
compiler and validator DLLs, import library, matching headers, and licenses.
Build intermediates remain in `external/dxc/build/x86_64-win32` for incremental
package rebuilds. Ordinary CI only extracts the checked-in archive.

When updating DXC or its patches, rebuild the archive and update its package
version in `scripts/build.py` as appropriate. Keep `DXC_VERSION` and
`DXC_HEADERS_REVISION` in `CMakeLists.txt` aligned with the packaged sources.

## Focused build and validation

For a focused build, use the Defold shell with the platform SDK set up. The
standalone CMake project selects the same toolchain as the engine.

```sh
cmake -S external/dxc -B tmp/dxc-build -G Ninja \
  -DTARGET_PLATFORM=x86_64-linux \
  -DDEFOLD_EXTERNAL_INSTALL_PREFIX=/absolute/path/to/dxc-install
cmake --build tmp/dxc-build
cmake --install tmp/dxc-build
python3 engine/shaderc/src/test/dxc_prototype/run.py \
  --dxc /absolute/path/to/dxc-install/bin/x86_64-linux/dxc \
  --out tmp/dxc-results
```

Adjust the platform and executable suffix for other hosts. The wrapper limits
DXC to four compilation jobs by default; set `DEFOLD_DXC_BUILD_JOBS` to adjust
memory/CPU usage. Incremental builds reuse the downloaded source and compiled
objects.

Within a configured Linux/macOS parent `external` build, the focused target is
`dxc_build_external`, and `cmake --install <build> --component dxc` installs only
these files. Windows source rebuilds use the standalone project or the explicit
package command above.

Build the native reflection probe using the installed headers:

```sh
g++ -std=c++11 \
  -I/absolute/path/to/dxc-install/include \
  -I/absolute/path/to/dxc-install/include/dxc/DirectX-Headers/directx \
  -I/absolute/path/to/dxc-install/include/dxc/DirectX-Headers/wsl/stubs \
  engine/shaderc/src/test/dxc_prototype/reflect.cpp -ldl -o /tmp/dxc-reflect
```

Pass `--reflect /tmp/dxc-reflect --dxcompiler /absolute/path/to/dxc-install/lib/x86_64-linux/libdxcompiler.so`
to the prototype runner to check native reflection too.

## Linux compatibility

Use the existing Linux build baseline. The documented Docker image and current CI use Ubuntu 22.04
and native runners for x86-64 and ARM64. This integration does not download or
bundle a newer glibc, libstdc++, or compiler. It uses the selected compiler and
sysroot; the resulting binaries inherit that environment's system-library
requirements. Native Linux runners are required to avoid accidentally
installing a host executable under a different architecture.

Building on a newer workstation is not evidence of compatibility with the
release baseline. Run the prototype on the baseline environment and inspect
the installed binaries' ELF dependencies and symbol versions before publishing
packages. A compiler may require a newer C++ language version without requiring
a newer runtime; the source build and runtime checks establish that separately.

## Windows package validation (2026-10-27)

- Built `dxc-1.9.2609-x86_64-win32.tar.gz` from the pinned source archives with
  MSVC 19.51.36246, Release configuration, and the static CRT. The archive is
  9,210,665 bytes and includes 38 files.
- Extracted into a clean directory and verified every file against the staged
  installation. SM6 vertex, pixel, and compute compilation, reflection, and
  root-signature checks passed for versions 1.0 and 1.1. The executable and DLLs
  import only Windows system libraries.
- All 27 shaderc tests and the dedicated DXC and root-signature tests passed
  using the packaged compiler DLL. The packaging tests cover runtime archive
  contents and package installation across six host/target combinations.
- The normal Windows external build graph contains no DXC targets. Linux and
  macOS retain their source builds.

## Source-build validation (2026-10-20)

- Windows x64: built and installed from the parent `external` project used by
  `build_ext`. The prototype passed for SM6.0 vertex, pixel, and compute shaders
  with root-signature versions 1.0 and 1.1. Import inspection found only Windows
  system DLL dependencies, with no dynamic MSVC CRT dependency. A repeated build
  reported no work.
- Linux x64: built with GCC 9.4 on Ubuntu 20.04/WSL, using the original glibc
  2.31 and no replacement runtime libraries. Both prototype variants passed
  against the installed executable, including native reflection compiled against
  the installed headers. Reflected layouts and serialized root signatures match
  the Windows build.
- The Linux compiler library's highest referenced GLIBC and GLIBCXX symbol
  versions were `GLIBC_2.29` and `GLIBCXX_3.4.21`. These are observations for this
  build, not a claim of testing every distribution with those versions.

The exact Ubuntu 22.04/Clang CI configuration, macOS, ARM hosts, GPU execution,
and private platform compilers still require their own validation. No changes
to the production shader compilation path or platform-specific backends are
part of this dependency integration.
