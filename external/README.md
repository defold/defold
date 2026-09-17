# External

`./scripts/build.py build_ext` builds source dependencies (Bullet, and Dawn on macOS)
with the regular Defold CMake toolchain and installs them into
`tmp/dynamo_home/ext`. Run it after `install_ext`, before the first engine
build, and whenever these sources or the toolchain change. Use `--platform`
for cross-compilation. Repeated calls reuse the CMake build directory.
`distclean` removes these build caches as well as the installed SDK.

Dawn is pinned in `external/dawn/CMakeLists.txt`. The first macOS build downloads
its sources and dependencies and builds a static library with the Metal backend.
Tests, samples, and command-line tools are disabled. Sources and objects are
cached under `external/build/<platform>`. The library
is installed as `ext/lib/<platform>/libwebgpu_dawn.a`, with headers under
`ext/include`. Both `arm64-macos` and `x86_64-macos` are supported.

```sh
./scripts/build.py shell
./scripts/build.py build_ext --platform=arm64-macos
./scripts/build.py build_engine --platform=arm64-macos -- --with-webgpu
```

`build_ext` reports configure (including downloads), build, and install times.
Repeated builds reuse the downloaded sources and compiled objects.

The other external libraries are distributed as packages. Rebuild those with
`build_external`, which writes archives under `defold/packages`.

# Modifications

Always keep the original code separate from the modified code, so that it's easy to reason about and update.

Keep any engine changes in a `.patch` file.
