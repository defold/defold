# External

`./scripts/build.py build_ext` builds source dependencies
with the regular Defold CMake toolchain and installs them into
`tmp/dynamo_home/ext`. Run it after `install_ext`, before the first engine
build, and whenever these sources or the toolchain change. Use `--platform`
for cross-compilation; this builds dependencies for the host tools first,
then for the target platform. Repeated calls reuse each platform's CMake
build directory.
`distclean` removes these build caches as well as the installed SDK.

The other external libraries are distributed as packages. Rebuild those with
`build_external`, which writes archives under `defold/packages`.

Dawn is a desktop package built by `build_external`, with its revision pinned in
`external/dawn/CMakeLists.txt`. The first build downloads its sources and
dependencies and builds a static library with the native backend. Tests, samples,
and command-line tools are disabled. Shader input is limited to WGSL, with only
the MSL, SPIR-V, or HLSL output writer needed by the platform enabled. Sources
and objects are cached under `external/dawn/build/<platform>`. Supported
platforms and backends are:

| Platforms | Dawn backend |
| --- | --- |
| `arm64-macos` | Metal |
| `arm64-linux`, `x86_64-linux` | Vulkan, with X11 and Wayland surfaces |
| `x86_64-win32` | Direct3D 11 and 12 |

Unfiltered `build_external` runs skip Dawn on other platforms. Linux builds
require the X11 and Wayland development headers (`libx11-dev`, `libwayland-dev`
on Ubuntu). Windows builds target the Windows 10 API (`WINVER` and
`_WIN32_WINNT` set to `0x0A00`) and use the static MSVC runtime and HWND surfaces;
optional UWP/WinUI surface support is disabled. The API target is set for the
standalone Dawn package through `DEFOLD_WIN32_WINNT`.
Windows packaging also requires LLVM's `llvm-strip`, available on `PATH` or in
`%ProgramFiles%/LLVM/bin`; `DAWN_STRIP_EXECUTABLE` can override its location.

`build_external` installs the packaged host `protoc` tool if it is missing, so
the commands below also work before the first `install_ext`.

The **Build Dawn** GitHub Actions workflow builds all four
platforms and uploads the package archives as artifacts, retained for seven days.
It uses `build_external --package=dawn`; `build_ext` does not build Dawn.
Pushes to `webgpu-dawn-support` build and commit the packages back to the branch
after all four platforms succeed. Manual dispatch is also supported once the
workflow exists on the repository's default branch. Manual runs commit packages
by default; disable `push_changes` to upload artifacts without committing them.
The run title identifies which mode was selected. Artifact-only runs do not
cancel runs that will commit packages. The commit job rejects packages above
GitHub's 100 MiB file limit, skips unchanged packages, and uses a normal push
from the built revision so it cannot overwrite a branch that has advanced.
Its GitHub token push does not trigger another build.
CI caches the pinned Dawn sources and downloaded dependencies separately from
compiler results, which use sccache and GitHub's cache storage. CMake configures
a fresh build tree on each runner. The first run still downloads dependencies
and compiles the library; later runs can reuse matching compiler results.

```sh
./scripts/build.py shell
./scripts/build.py build_external --package=dawn --platform=arm64-macos
./scripts/build.py install_ext --platform=arm64-macos
```

These commands produce `packages/dawn-6bab1bd-arm64-macos.tar.gz` and install
the library to `ext/lib/arm64-macos/libwebgpu_dawn.a` and the headers to
`ext/include/arm64-macos`. Dawn headers are scoped to each native platform so they
cannot shadow Emscripten's WebGPU headers. When updating an existing installation,
remove the old shared `ext/include/webgpu` and `ext/include/dawn` directories
before running `install_ext`.

All Dawn packages strip debug information while preserving the
symbols needed for linking; the libraries in the build directories retain their
debug information. Repeated package builds reuse downloaded sources and compiled
objects. `build_ext` does not configure or build Dawn.

To build the engine with Dawn on macOS, run this in the same build shell:

```sh
./scripts/build.py build_engine --platform=arm64-macos -- --with-webgpu
```

The native graphics regression test draws with depth writes both enabled and
disabled, fails on WebGPU validation errors, and checks runtime swap-interval
changes against the Metal layer. After building the `test_app_graphics` target,
run it from the repository root:

```sh
DEFOLD_TEST_AUTO_EXIT=1 ./engine/graphics/build/arm64-macos/src/test/test_app_graphics webgpu
```

# Modifications

Always keep the original code separate from the modified code, so that it's easy to reason about and update.

Keep any engine changes in a `.patch` file.
