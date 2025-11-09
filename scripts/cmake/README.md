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

By default, it uses `RelWithDebugInfo` build type.
We use `Debug` builds if you pass in `--opt-level=N`, where `N<2`.

### Feature toggles

CMake builds honour the same feature flags as the legacy Waf flow. Pass
`--with-asan`, `--with-ubsan`, or `--with-tsan` after the `--` separator when
invoking `scripts/build.py` and the configure step applies the matching
`WITH_*` cache options (e.g. `WITH_ASAN=ON`). The graphics toggles such as
`--with-vulkan` continue to map to `WITH_VULKAN`.

## Invocation

Just as before, we invoke a separate, single configure + build command for each built library.
This is done via `build.py : _build_engine_lib_cmake`

## Project generation

Currently, the CMake configuration will generate build commands for the unit tests as well.
Instead, the option of building and/or running the tests is moved to when executing Ninja build command.

* We really only want to generate the (i.e. moving to incremental builds as a default)
* We want a full solution generation to have access to all the tests as well for easy iteration.

## Solution generation

You can create a top level CMakeLists.txt and a solution with the build command:

```bash
./scripts/build.py make_solution --platform=arm64-macos
```

Note that for e.g. Android, the CMakeLists.txt _is_ the solution.


## Folder structures

CMake outputs into different folder structures than before, so make not of any previous assumptions that may be stored in helper scripts etc.

For tests that need to load build content, we have a define `DM_USE_CMAKE` that can be used to fixup the build folder. We may replace that with an actual `DM_TEST_CONTENT_PATH` later.
