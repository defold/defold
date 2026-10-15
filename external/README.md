# External

`./scripts/build.py install_ext` installs the remaining prepackaged dependencies,
then calls `build_ext` to build source dependencies with the regular Defold CMake
toolchain and install them into `tmp/dynamo_home/ext`. Run it with the platform SDK set up, before the first engine
build, and whenever these sources or the toolchain change. Use `--platform`
for cross-compilation; this builds dependencies for the host tools first,
then for the target platform. Repeated calls reuse each platform's CMake
build directory.
`clean_ext --platform=<platform>` removes `external/build/<platform>`,
`external/*/build/<platform>`, and that platform's installed directories under
`ext/lib`, `ext/bin`, `ext/include`, and `ext/share`. It defaults to the host
platform when `--platform` is omitted. Other platforms, shared dependency files,
toolchains under `ext/SDKs`, engine outputs, source archives, and package archives
outside the build directories are retained.
`distclean` removes these build caches as well as the installed SDK.

Use `./scripts/build.py build_ext --platform=<platform>` to rebuild and install
the source dependencies directly. This builds Basis Universal, both Box2D
versions, Bullet, HarfBuzz, libunibreak, LZ4, Opus, SheenBidi, and Skribidi on all
platforms. Protobuf is built for desktop platforms; mobile and web builds use the
host's Protobuf tools. Protobuf and Abseil use the same CMake toolchain and build
graph as the other source dependencies and install directly into `ext`.
`external/build` itself is generated build output.

CI caches built external dependencies for engine, Bob, and editor jobs using the
[`build-external`](../.github/actions/build-external/action.yml) action.

To force a fresh build, run
`./scripts/build.py clean_ext install_ext --platform=<platform>`.

The command-time summary lists wall-clock times under `Libraries` for each
platform's CMake build command, also saved in `build_times.json`. Each time spans
the library's first task start to its last task finish, including scheduling
waits. Libraries run in parallel, so these times do not add up to the command's
elapsed time. Protobuf includes its Abseil dependencies. Only tasks
executed in the current build are counted.

External libraries can also be distributed as packages. Rebuild the packages
listed below with `build_external`, which writes archives under `defold/packages`.

From the repository root, with the platform SDK set up:

```sh
./scripts/build.py shell
./scripts/build.py build_external --package=opus --platform=arm64-macos
./scripts/build.py install_ext --platform=arm64-macos
```

The package builds use CMake. The migrated packages retain their existing names
and installation layout:

| Package selector | Archive prefix | Contents |
| --- | --- | --- |
| `opus` | `opus-1.5.2` | Decoder library in the platform archive; headers in the common archive. |
| `harfbuzz` | `harfbuzz-13.2.1` | Library in the platform archive; headers and Defold's configuration override in the common archive. |
| `sheenbidi` | `SheenBidi-2.9.0` | Unity-built library in the platform archive; headers in the common archive. |
| `libunibreak` | `libunibreak-6.1` | Library in the platform archive; headers in the common archive. |

Box2D also produces separate common and platform archives, with both SIMD and
non-SIMD libraries in the platform archive. `external/rebuild.sh` uses the same
`build_external` command.

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

External builds do not require an installed `protoc`, so the commands below
also work before the first `install_ext`.

The **Build Dawn** GitHub Actions workflow builds all four
platforms and uploads the package archives as artifacts, retained for seven days.
It uses `build_external --package=dawn`.
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
objects.

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
