# Emscripten

## Installation and build

Defold builds two WebAssembly targets, `wasm-web` and `wasm_pthread-web`. For a local toolchain, install and activate the Emscripten version specified by `EMSCRIPTEN_VERSION_STR` in [build_tools/sdk.py](build_tools/sdk.py). Follow the [Emscripten SDK installation instructions](https://emscripten.org/docs/getting_started/downloads.html).

From the repository root, with `EMSDK` pointing to that installation:

```sh
./scripts/build.py shell
./scripts/build.py install_ext --platform=wasm-web
./scripts/build.py build_engine --platform=wasm-web
```

For a packaged SDK, set `DM_PACKAGES_URL` or pass `--package-path` and run `./scripts/build.py install_sdk --platform=wasm-web` before `install_ext`. Use `wasm_pthread-web` in the build commands for the threaded engine. Each target installs its JavaScript and WebAssembly files under `$DYNAMO_HOME/bin/<platform>/`.

## Updating Emscripten

1. Review the [Emscripten changelog](https://github.com/emscripten-core/emscripten/blob/main/ChangeLog.md). Set `EMSCRIPTEN_VERSION_STR` in [sdk.py](build_tools/sdk.py) and `VERSION` in [package_emscripten.sh](scripts/package/package_emscripten.sh) to the new version. Update the `setup-emsdk` version in the [main CI](.github/workflows/main-ci.yml), [engine nightly](.github/workflows/engine-nightly.yml) and [external package](.github/workflows/build-external-template.yml) workflows to match.
2. From the repository root on each build host, run `./scripts/package/package_emscripten.sh`. Make the resulting `local_sdks/emsdk-<version>-<arch>-<os>.tar.gz` available at the package location used by the build.
3. Install the new package with `./scripts/build.py install_sdk --platform=wasm-web --package-path=local_sdks`. Both web targets use it. Move older Emscripten SDKs out of `$DYNAMO_HOME/ext/SDKs` so CMake selects the new version.
4. For each web target (`wasm-web` and `wasm_pthread-web`), start with an empty `engine/build/<platform>` directory. Run `./scripts/build.py clean_ext install_ext build_engine --platform=<platform>` to rebuild its dependencies and engine. Check that `CMAKE_TOOLCHAIN_FILE` in both `external/build/<platform>/CMakeCache.txt` and `engine/build/<platform>/CMakeCache.txt` points into the new SDK.

## Running and debugging

Web unit tests run with Bun or Node.js. After the engine build, run an individual test from the repository root, for example:

```sh
cmake --build engine/build/wasm-web --target run_test_dlib
```

For tests that require a browser window, see the [Chromium test instructions](engine/platform/src/test/browser/README.md). To run a bundled game locally, change into the bundle directory containing `index.html` and run `python3 /path/to/defold/scripts/simple-cors-http-server.py`. Open `http://localhost:8000/`. This server supplies the cross-origin isolation headers needed by the threaded build.

The [web CMake configuration](scripts/cmake/platform_html5.cmake) enables symbol maps. To emit WebAssembly source maps and separate DWARF information, select the CMake `Debug` configuration with `./scripts/build.py build_engine --platform=wasm-web -- --opt-level=0`. For memory diagnostics, build with `./scripts/build.py build_engine --platform=wasm-web -- --with-asan`. See the [Emscripten debugging guide](https://emscripten.org/docs/porting/Debugging.html) for using these files in a debugger.

For size analysis, [wasm_symbol_sizes.py](scripts/web/wasm_symbol_sizes.py) reads a `.wasm` binary and its matching `.js.symbols` file and reports function sizes using Bloaty.
