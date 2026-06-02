# CMake

## CMake includes

The order of the includes are shown (.cmake is implied)

- defold - a bit like our top level waf_dynamo.py
- functions - optional helpers that set defines/flags/linkerflags on build artefacts
- tools - verify our list of tools (e.g. java, ninja etc)
- verify our list of tools (e.g. java, ninja etc)
- platform - the compiler/tool setup for the target platform. sets HOST_PLATFORM/TARGET_PLATFORM.
  - sdk - the toolset for the target platforms
- defold_post_process - run just at the start of a `project()` call, in order to do fixup on teh compiler flags

Some rules:
* Only include a file _once_ (for efficiency)
* Each of the sdk/platform includes should only include _one_ of the target platform includes

## Configuration

Use the Defold shell first. It sets `CMAKE_GENERATOR=Ninja` plus the usual
engine environment:

```bash
./scripts/build.py shell
```

From the repository root:

```bash
cmake -S . -B engine/build/arm64-macos
```

Defaults:

* `CMAKE_GENERATOR=Ninja` from `./scripts/build.py shell`
* `TARGET_PLATFORM` inferred from the host platform
* `CMAKE_BUILD_TYPE=RelWithDebInfo`
* `BUILD_TESTS=ON`

Common overrides:

```bash
cmake -S . -B engine/build/arm64-macos -DTARGET_PLATFORM=arm64-macos
cmake -S . -B engine/build/arm64-macos -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B engine/build/arm64-macos -DBUILD_TESTS=OFF
```

If you are not in `./scripts/build.py shell`, pass `-G Ninja` or set
`CMAKE_GENERATOR=Ninja` before configuring.

### Feature toggles

CMake builds honour the same feature flags as the legacy Waf flow.

For named engine features, pass comma-separated values with
`DEFOLD_ENABLE_FEATURES`:

```bash
cmake -S . -B engine/build/arm64-macos -DDEFOLD_ENABLE_FEATURES=font_layout
cmake -S . -B engine/build/arm64-macos -DDEFOLD_ENABLE_FEATURES=box2dv3
cmake -S . -B engine/build/arm64-macos -DDEFOLD_ENABLE_FEATURES=box2dv3,font_layout
```

The semicolon form also works if quoted:

```bash
cmake -S . -B engine/build/arm64-macos -DDEFOLD_ENABLE_FEATURES="box2dv3;font_layout"
```

To clear features in an existing build directory, configure with an empty value:

```bash
cmake -S . -B engine/build/arm64-macos -DDEFOLD_ENABLE_FEATURES=
```

When invoking `scripts/build.py`, pass `--with-asan`, `--with-ubsan`, or
`--with-tsan` after the `--` separator and the configure step applies the
matching `WITH_*` cache options, such as `WITH_ASAN=ON`. The graphics toggles
such as `--with-vulkan` continue to map to `WITH_VULKAN`.

## Invocation

`scripts/build.py build_engine` configures from the top-level `CMakeLists.txt`,
with one CMake cache under `engine/build/<platform>`. Each engine library still
gets its own binary directory under `engine/<lib>/build/<platform>`, so objects,
generated files, and archives stay with the library.

During the transition, `scripts/build.py --with-waf build_engine` uses the
restored Waf lib loop instead.

For local shorthand, the host platform, release-with-debug-symbols build type,
and tests are all defaulted:

```bash
cmake -S . -B engine/build/arm64-macos
cmake --build engine/build/arm64-macos --target all build_tests install
cmake --build engine/build/arm64-macos --target run_tests
```

To inspect task timing from Ninja, generate a standalone HTML timeline:

```bash
python3 scripts/cmake/plot_ninja_log.py engine/build/arm64-macos/.ninja_log -o build-tasks.html
```

The page supports mouse wheel zoom, sideways drag panning, horizontal trackpad
panning, hover details, and search highlighting. Without a path, the script
uses the newest `engine/**/.ninja_log`. It renders every log entry by default;
pass `--latest` to render only the latest Ninja invocation.

The normal `scripts/build.py build_engine` CMake path configures one top-level
CMake cache under `engine/build/<platform>`.
The CMake path is incremental by default, so repeated builds should no-op at
the Ninja target level when inputs have not changed.

For example, to build and run the engine tests after `build_engine` has
configured the `engine` library:

```bash
cmake --build engine/build/arm64-macos --target run_tests
```

Without tests:

```bash
cmake -S . -B engine/build/arm64-macos -DBUILD_TESTS=OFF
cmake --build engine/build/arm64-macos --target all install
```

To configure a subset, use `DEFOLD_SELECTED_ENGINE_LIBS`:

```bash
cmake -S . -B engine/build/arm64-macos -DDEFOLD_SELECTED_ENGINE_LIBS=dlib
cmake -S . -B engine/build/arm64-macos -DDEFOLD_SELECTED_ENGINE_LIBS=testmain,dlib
```

### Common library tasks

From the repository root, after configuring `engine/build/<platform>`:

```bash
# Rebuild one library target
cmake --build engine/build/arm64-macos --target dlib

# Rebuild and run tests for one library
cmake -S . -B engine/build/arm64-macos -DDEFOLD_SELECTED_ENGINE_LIBS=dlib
cmake --build engine/build/arm64-macos --target build_tests run_tests

# Run the tests again for the configured library set
cmake --build engine/build/arm64-macos --target run_tests
```

Replace `arm64-macos` with the target platform and `dlib` with the library
target. When using a full build tree, `run_tests` runs all generated tests. To
scope `run_tests` to one library, configure that build tree with
`DEFOLD_SELECTED_ENGINE_LIBS=<lib>`. Individual runnable tests are also exposed
as `run_<test-target>`, for example `run_test_dlib`.

## Project generation

When `BUILD_TESTS=ON`, CMake generates the unit test targets during configure:
`build_tests`, `run_tests`, each `test_*` binary target, and each
`run_<test-target>` command target. Test binaries are excluded from the default
`all` target and are only built when requested through `build_tests`,
`run_tests`, or a direct `test_*`/`run_*` target.

## Solution generation

You can generate a solution for a platform with:

```bash
./scripts/build.py make_solution
./scripts/build.py --platform=arm64-macos make_solution
./scripts/build.py --platform=arm64-macos make_solution -- --enable-feature=box2dv3 --with-opengl
```

This uses the host platform by default, with a `RelWithDebInfo` configuration
and test targets enabled. Pass `--platform=<platform>` when generating for
another target. Pass `-- --skip-build-tests` to generate with
`BUILD_TESTS=OFF`, which omits test targets from the solution. Feature and
backend toggles, such as `--enable-feature=box2dv3` and `--with-opengl`, are
passed after the `--` separator.

Note that for e.g. Android, the CMakeLists.txt _is_ the solution.


## Folder structures

CMake outputs into different folder structures than before, so make not of any previous assumptions that may be stored in helper scripts etc.

For tests that need to load build content, we have a define `DM_USE_CMAKE` that can be used to fixup the build folder. We may replace that with an actual `DM_TEST_CONTENT_PATH` later.
