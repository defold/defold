# CMake

## CMake includes

The order of the includes are shown (.cmake is implied)

# defold - a bit like our top level waf_dynamo.py
# sdk - the toolset for the target platforms
# platform - the compiler/tool setup for the target platform
# functions - optional helpers that set defines/flags/linkerflags on build artefacts

Some rules:
* Each of the sdk/platform includes should only include _one_ of the target platform includes

## Configuration

By default, it uses `Release` build type.
We use `Debug` builds if you pass in `--opt-level=N`, where `N<2`.

## Invocation

Just as before, we invoke a separate, single configure + build command for each built library.
This is done via `build.py : _build_engine_lib_cmake`

## Solution generation

You can create a top level CMakeLists.txt and a solution with the build command:

```bash
./scripts/build.py make_solution --platform=arm64-foo
```

Note that for e.g. Android, the CMakeLists.txt _is_ the solution.


## Folder structures

CMake outputs into different folder structures than before, so make not of any previous assumptions that may be stored in helper scripts etc.

For tests that need to load build content, we have a define `DM_USE_CMAKE` that can be used to fixup the build folder. We may replace that with an actual `DM_TEST_CONTENT_PATH` later.
