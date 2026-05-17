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

`scripts/build.py build_engine` configures from the top-level `CMakeLists.txt`.
It first selects the host tool libs needed by `bob-light`, then selects
the full target lib set for the engine build.

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

The top-level CMake cache lives under `engine/build/<platform>`. Each engine
library gets its own binary directory under `engine/<lib>/build/<platform>`.
The CMake path is incremental by default, so repeated builds should no-op at
the Ninja target level when inputs have not changed.

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

## Project generation

Currently, the CMake configuration will generate build commands for the unit tests as well.
Instead, the option of building and/or running the tests is moved to the
`cmake --build` invocation.

* We really only want to generate the (i.e. moving to incremental builds as a default)
* We want a full solution generation to have access to all the tests as well for easy iteration.

## Solution generation

You can create a top level CMakeLists.txt and a solution with the build command:

```bash
./scripts/build.py make_solution
```

This uses the host platform by default, with a `RelWithDebInfo` configuration
and test targets enabled. Pass `--platform=<platform>` when generating for
another target.

Note that for e.g. Android, the CMakeLists.txt _is_ the solution.


## Folder structures

CMake outputs into different folder structures than before, so make not of any previous assumptions that may be stored in helper scripts etc.

For tests that need to load build content, we have a define `DM_USE_CMAKE` that can be used to fixup the build folder. We may replace that with an actual `DM_TEST_CONTENT_PATH` later.
