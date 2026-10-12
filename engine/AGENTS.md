# Engine guidance

Follow [engine code style](docs/CODE_STYLE.md) and the repository's `.clang-format`.

## Build and test

Use CMake for engine builds and tests. The [engine build guide](../README_BUILD.md) covers platform setup and full builds; the [CMake guide](../scripts/cmake/README.md) covers focused rebuilds and tests. Run commands from the repository root.

For a separate `-B` directory, also set `-DDEFOLD_BUILD_HOME=<absolute-build-root>` to isolate generated library outputs.
