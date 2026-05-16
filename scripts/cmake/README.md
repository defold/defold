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

Defaults are intended to be convenient for local development:

* `CMAKE_GENERATOR` defaults to `Ninja` in `./scripts/build.py shell`.
* `TARGET_PLATFORM` is inferred from the host platform when not specified.
* `CMAKE_BUILD_TYPE` defaults to `RelWithDebInfo` for single-config generators.
* `BUILD_TESTS` defaults to `ON`.

When invoking CMake through `scripts/build.py`, pass `-- --opt-level=0` to use
`Debug`, `-- --skip-build-tests` to configure without test targets, or
`--skip-tests` to build tests without running them.

### Feature toggles

CMake builds honour the same feature flags as the legacy Waf flow. Pass
`--with-asan`, `--with-ubsan`, or `--with-tsan` after the `--` separator when
invoking `scripts/build.py` and the configure step applies the matching
`WITH_*` cache options (e.g. `WITH_ASAN=ON`). The graphics toggles such as
`--with-vulkan` continue to map to `WITH_VULKAN`.

## Invocation

Just as before, we invoke a separate, single configure + build command for each built library.
This is done via `build.py : _build_engine_lib_cmake`

For local shorthand, the host platform, release-with-debug-symbols build type,
and tests are all defaulted:

```bash
cmake -S engine/dlib -B engine/dlib/build
cmake --build engine/dlib/build --target all build_tests
cmake --build engine/dlib/build --target run_tests
```

## Project generation

Currently, the CMake configuration will generate build commands for the unit tests as well.
Instead, the option of building and/or running the tests is moved to when executing Ninja build command.

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
