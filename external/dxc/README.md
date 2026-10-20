# DXC host tools

`build_ext` builds DXC for the desktop host platform. `install_ext` invokes
`build_ext` after installing prepackaged dependencies, so no extra command is
required. Cross-builds first build dependencies for the host as usual and do
not build a target copy of DXC. Build on each native host when preparing tools
for distribution across platforms.
This does not select or replace platform-specific shader compiler backends.

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
Bob/editor distribution packaging and production shaderc integration are
separate follow-up work; this change installs dependencies for development.

## Focused build and validation

For a focused build, use the same compiler environment as the engine. On Windows
use the Visual Studio developer command prompt (including the DIA SDK).

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

Within a configured parent `external` build, the focused target is
`dxc_build_external`, and `cmake --install <build> --component dxc` installs only
these files. Normal `build_ext` builds and installs all dependencies as before.

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

## Validation (2026-10-20)

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
