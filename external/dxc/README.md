# DXC host tools

All five desktop host platforms use `packages/dxc-1.9.2609-<platform>.tar.gz`:
`x86_64-win32`, `arm64-macos`, `x86_64-macos`, `arm64-linux`, and `x86_64-linux`.
`install_ext` extracts these packages; `build_ext` does not compile DXC.
Cross-builds first prepare dependencies for the host as usual. Desktop targets
also need their own DXC library because shaderc links it, including when building
x86_64-macos on an arm64-macos host. Mobile and web targets use only the host copy.

Explicit source rebuilds use `build_external --package=dxc` or the standalone
CMake project. CMake downloads and SHA-256 checks the pinned DXC source and its
matching DirectX-Headers archive. Sources and intermediate files stay in the
build directory. LLVM is configured in a separate Release build with its own C++17,
exception, and RTTI settings. These settings do not propagate into the engine.
Only `dxc`, `dxcompiler`, and the Windows validator are built; SPIR-V generation
and upstream tests are disabled. `patch.cmake` makes upstream honor its SPIR-V
option on Unix and fixes deallocation of variable-sized `CGFunctionInfo`,
`AttributeSetImpl`, and `AttributeSetNode` objects. The latter pairs the raw
allocation with explicit destruction and unsized deletion, avoiding ASAN's
`new-delete-type-mismatch` error when DXC finishes compiling a shader. Version
stamping uses the source archive's fixed version.
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

## Rebuild the packages

### Windows package

With the Windows SDK set up, run from the repository root in the Defold shell:

```sh
./scripts/build.py build_external --package=dxc --platform=x86_64-win32
./scripts/build.py install_ext_packages --platform=x86_64-win32
```

The first command builds the pinned sources with Defold's toolchain and writes
`packages/dxc-1.9.2609-x86_64-win32.tar.gz`. The archive includes the executable,
compiler and validator DLLs, import library, matching headers, and licenses.

### macOS packages

On a macOS host with Xcode set up, build each architecture in the Defold shell:

```sh
./scripts/build.py build_external --package=dxc --platform=arm64-macos
./scripts/build.py build_external --package=dxc --platform=x86_64-macos
./scripts/build.py install_ext_packages
```

These commands write `packages/dxc-1.9.2609-arm64-macos.tar.gz` and
`packages/dxc-1.9.2609-x86_64-macos.tar.gz`. Each archive includes the executable,
`libdxcompiler.dylib`, matching headers, and licenses. The selected platform
controls the architecture even when cross-building on an Apple Silicon host.
Both packages target the repository's macOS minimum (currently 12.0).

### Linux packages

Use the repository's Ubuntu 22.04 Dockerfile; see
[`scripts/docker/README.md`](../../scripts/docker/README.md) for background.
From the repository root, build and load each architecture under a separate tag.
Separate tags also support Docker installations whose image store cannot load
multi-platform images:

```sh
docker buildx build --platform linux/amd64 --load -t builder/ubuntu:dxc-amd64 scripts/docker
docker buildx build --platform linux/arm64 --load -t builder/ubuntu:dxc-arm64 scripts/docker
```

Start the x86_64 container from the repository root:

```sh
docker run --rm -it --platform linux/amd64 --user "$(id -u):$(id -g)" \
  -v "$PWD:/home/builder" builder/ubuntu:dxc-amd64
```

Inside that container, enter the Defold shell and build/install the package:

```sh
./scripts/build.py shell
./scripts/build.py build_external --package=dxc --platform=x86_64-linux
./scripts/build.py install_ext_packages --platform=x86_64-linux
```

Exit the Defold shell and container, then start the arm64 container from the
repository root:

```sh
docker run --rm -it --platform linux/arm64 --user "$(id -u):$(id -g)" \
  -v "$PWD:/home/builder" builder/ubuntu:dxc-arm64
```

Inside that container:

```sh
./scripts/build.py shell
./scripts/build.py build_external --package=dxc --platform=arm64-linux
./scripts/build.py install_ext_packages --platform=arm64-linux
```

These builds write `packages/dxc-1.9.2609-x86_64-linux.tar.gz` and
`packages/dxc-1.9.2609-arm64-linux.tar.gz` into the mounted checkout. Each archive
includes the executable, `libdxcompiler.so`, matching headers, and licenses.
The container architecture must match the selected Defold platform. Running
either container through Docker's emulation on macOS is supported; a physical
Linux machine of each architecture is not required. Run the shaderc tests below
inside the matching container before publishing either archive.

### Updating packages

Build intermediates remain in `external/dxc/build/<platform>` for incremental
package rebuilds. Ordinary CI only extracts the checked-in archives.

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
```

Adjust the platform and executable suffix for other hosts. The wrapper limits
DXC to four compilation jobs by default; set `DEFOLD_DXC_BUILD_JOBS` to adjust
memory/CPU usage. Incremental builds reuse the downloaded source and compiled
objects.

In this standalone build, the focused target is `dxc_build_external`, and
`cmake --install <build> --component dxc` installs only these files. Normal parent
`external` builds contain no DXC targets; use the standalone project or explicit
package commands above for source rebuilds.

After installing the package into the engine SDK, build and run `test_shaderc`
using the [focused CMake test workflow](../../scripts/cmake/README.md). The suite
exercises DXC compilation and resource mapping on desktop hosts. The detailed
root-signature tests use Windows SDK validation and run on Windows.

## Linux compatibility

Use the Ubuntu 22.04 baseline from the documented Docker image for both x86_64
and arm64. Build within a matching Linux architecture, either directly or in an
emulated container. The source wrapper checks the build environment's OS and
architecture to prevent installing a binary under the wrong platform name.
DXC uses the selected compiler and sysroot; the resulting binaries inherit that
environment's system-library requirements. The packages do not bundle a newer
glibc or libstdc++.

Building on a newer workstation is not evidence of compatibility with the
release baseline. Run the shaderc tests on the baseline environment and inspect
the installed binaries' ELF dependencies and symbol versions before publishing
packages. A compiler may require a newer C++ language version without requiring
a newer runtime; the source build and runtime checks establish that separately.

## Windows ASAN package update (2026-11-04)

- Rebuilt `dxc-1.9.2609-x86_64-win32.tar.gz` with MSVC 19.51.36246,
  Release configuration, and the static CRT, applying the same three
  variable-sized allocation fixes as the Linux update below.
- The archive is 9,210,989 bytes and contains the same 38 files. Clean extraction
  matched the staged installation byte-for-byte. Only `dxc.exe`, `dxcompiler.dll`,
  and `dxil.dll` changed; headers, licenses, and the import library are unchanged.
  All three binaries are x64 and import only Windows system libraries.
- Standalone SM6 vertex, pixel (including sparse output slots and mixed numeric
  types), and compute compilation passed using the extracted package.
- All 33 shaderc tests passed with the rebuilt compiler DLL, including pixel
  output metadata and Windows root-signature validation, along with all four
  standalone CLI tests. These were ordinary Windows tests, not an ASAN run.

## Linux ASAN package update (2026-11-04)

- Rebuilt both Linux archives with Clang 20.1.8 in the documented Ubuntu 22.04
  images, applying the three variable-sized allocation fixes in `patch.cmake`.
  The macOS archives were not rebuilt; the Windows rebuild is recorded above.
- Before the fix, an ASAN-enabled DXC API harness using the shader and arguments
  from `Shaderc.HLSLPixelOutputMetadata` reproduced the 64/32-byte mismatch on
  both Linux architectures. Fixing `CGFunctionInfo` exposed a second mismatch
  in LLVM's attribute sets, which the patch also addresses.
- After extracting the rebuilt archives, the harness passed with
  `ASAN_OPTIONS=new_delete_type_mismatch=1` on both architectures. Additional
  ASAN checks verified sparse pixel output slots, component masks and numeric
  types through reflection, plus vertex and compute shader compilation.
  ARM ran in the native Linux VM; x86_64 ran through Docker emulation on macOS.
  These were focused DXC checks, not a full engine test run.
- Standalone vertex and compute compilation passed without `LD_LIBRARY_PATH`.
  Archive paths, headers and licenses match the previous packages; only `dxc`
  and `libdxcompiler.so` changed. ELF architecture, relative runtime paths and
  SONAME were verified. Both executables and libraries retain the previous
  maximum symbol versions: `GLIBC_2.34`, `GLIBCXX_3.4.29`, and `CXXABI_1.3.9`.

## Linux package validation (2026-10-29)

- Built both archives from the pinned sources with Clang 20.1.8 and Release
  configuration in the documented Ubuntu 22.04 Docker image (Ubuntu 22.04.5).
  Each archive contains 36 files: arm64 is 9,834,429 bytes; x86_64 is 10,063,109
  bytes.
- Extracted into clean directories and verified all 36 files against each staged
  installation. All 34 shared headers and license files match the Windows
  package byte-for-byte.
- The executables and libraries have the expected ELF architectures and use
  `$ORIGIN/../../lib/<platform>` for library lookup. `libdxcompiler.so` has a
  relative SONAME and only system runtime dependencies. All four ELF files'
  highest referenced symbol versions are `GLIBC_2.34`,
  `GLIBCXX_3.4.29`, and `CXXABI_1.3.9`.
- With all `LD_*` environment variables removed, SM6 vertex, pixel, and compute
  compilation, native reflection using the packaged headers, and root-signature
  checks passed for versions 1.0 and 1.1 on both architectures. ARM tests ran in
  the native Linux VM; x86_64 tests ran through Docker's Rosetta emulation on
  macOS. Reflection layouts and serialized root signatures match the macOS
  arm64 results for both versions.
- `install_ext_packages --platform=arm64-macos` successfully installed all five
  desktop DXC archives exactly once, including both Linux packages.

## macOS allocation-fix package update (2026-11-05)

- Rebuilt both macOS archives from the pinned source with the same three
  variable-sized allocation fixes as the Linux packages. Used Apple Clang
  21.0.0, Release configuration, and a macOS 12.0 deployment target.
- Extracted each candidate archive and ran ASAN-enabled DXC API checks with
  `ASAN_OPTIONS=new_delete_type_mismatch=1`: the
  `Shaderc.HLSLPixelOutputMetadata` shader and arguments, sparse output
  reflection (slots, masks and numeric types), and vertex and compute shaders
  all passed. Tests ran on macOS 26.5.2, natively for arm64 and through Rosetta
  for x86_64. These were focused DXC checks, not a full engine test run.
- Standalone vertex and compute compilation passed with `DYLD_LIBRARY_PATH`
  and `DYLD_FALLBACK_LIBRARY_PATH` unset. Verified both Mach-O architectures,
  relative install names and runtime paths, system-only dependencies, and the
  macOS 12.0 minimum in all four binaries. Runtime testing on macOS 12.0 remains
  separate.
- Both archives retain the same 36 files. Headers and licenses are unchanged;
  only `dxc` and `libdxcompiler.dylib` changed. The Windows archive has not yet
  been rebuilt with these fixes.

## macOS package validation (2026-10-28)

- Built both archives from the pinned sources with Apple Clang 21.0.0, Release
  configuration, and a macOS 12.0 deployment target. Each archive includes 36
  files: arm64 is 8,521,908 bytes; x86_64 is 9,037,511 bytes.
- Extracted into clean directories and verified every file against the staged
  installations. The executables and dylibs have the expected architectures,
  relative library paths, and only system-library dependencies.
- On macOS 26.5.2, SM6 vertex, pixel, and compute compilation, native reflection
  using the packaged headers, and root-signature checks passed for versions 1.0
  and 1.1 with all `DYLD_*` environment variables removed. The x86_64 checks ran
  through Rosetta. Reflection layouts and serialized root signatures match
  between architectures. Runtime testing on macOS 12.0 remains separate.
- `install_ext_packages` installed both macOS archives successfully. Package and
  CMake tests cover installation across host/target combinations, archive
  contents, explicit package architecture selection, and macOS build graphs
  that exclude DXC compilation.
- The arm64 shaderc integration passed 23 unit tests (four Windows-only tests
  skipped) and all four standalone CLI tests using the installed package.

## Windows package validation (2026-10-27)

- Built `dxc-1.9.2609-x86_64-win32.tar.gz` from the pinned source archives with
  MSVC 19.51.36246, Release configuration, and the static CRT. The archive is
  9,210,665 bytes and includes 38 files.
- Extracted into a clean directory and verified every file against the staged
  installation. SM6 vertex, pixel, and compute compilation, reflection, and
  root-signature checks passed for versions 1.0 and 1.1. The executable and DLLs
  import only Windows system libraries.
- All 27 shaderc tests passed
  using the packaged compiler DLL. The packaging tests cover runtime archive
  contents and package installation across six host/target combinations.
- The normal Windows external build graph contains no DXC targets.

GPU execution and private platform compilers still require their own validation.
No changes to the production shader compilation path or platform-specific
backends are part of this dependency integration.
