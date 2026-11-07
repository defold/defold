# Build Engine

Defold uses CMake and Ninja. The `scripts/build.py` script handles dependency installation and full engine builds; you can use CMake directly for incremental builds and individual tests. See the [CMake guide](scripts/cmake/README.md) for more build options, sanitizers, and IDE project generation.

## IMPORTANT PREREQUISITE - SETUP

Follow the [setup guide](/README_SETUP.md) before building. Packaged SDK and toolchain versions are defined in [build_tools/sdk.py](build_tools/sdk.py); the minimum CMake version is specified in [CMakeLists.txt](CMakeLists.txt).

## Standard workflow

The standard workflow when building the engine is the following:

1. [Setup](/README_SETUP.md) environment
2. [Install](/README_SETUP.md#required-software---platform-sdks) platform SDKs
3. Install packages and build source dependencies with `install_ext`
4. Build the engine

When working on a new feature or fixing a bug you start by first building the engine once as described above. You then proceed to develop your feature or fix the bug and rebuild and test changes until satisfied. When you do a rebuild you can speed things up by only building the parts that have changed.

*NOTE: Read our [Contribution Guide](CONTRIBUTING.md), especially before undertaking larger tasks.*

**NOTE for Linux users**: Use the [Docker build environment](/scripts/docker/README.md) for consistent versions of shared dependencies.

## Build examples

The following examples give a quick overview; each step is explained below.

The `--platform` argument defaults to the host platform (`x86_64-win32`, `x86_64-linux`, `arm64-linux`, `x86_64-macos` or `arm64-macos`). CMake examples use `arm64-macos`; replace it with your configured platform.

Once per session:
```
$ ./scripts/build.py shell          # creates the shell. No need for "--platform"
```

Once per platform to be built
```
$ ./scripts/build.py install_ext    # extracts packages, checks the SDK, then builds source dependencies
```

Set up the platform SDK before running `install_ext`. Repeat the installation
after `distclean`, which also removes SDKs installed under `DYNAMO_HOME`.
Re-run `install_ext` when the external
sources or toolchain change; ordinary engine rebuilds use the installed libraries.

Build and install the full engine, docs, Bob Light, and builtins; build the tests and run them when the host supports the target:

    $ ./scripts/build.py build_engine

Build the Android engine and tests, and run the tests on a specific connected device:

    $ ./scripts/build.py build_engine --platform=arm64-android --test-device <serial>

After a successful full build, skip rebuilding docs and Bob Light, and skip building and running tests:

    $ ./scripts/build.py build_engine --skip-docs --skip-bob-light --skip-tests -- --skip-build-tests

`--skip-bob-light` uses the existing `${DYNAMO_HOME}/share/java/bob-light.jar` to build engine content. Rebuild Bob Light after changing Bob or its native compiler libraries.

Rebuild changed dependencies and relink the dmengine executable. No tests are run.

    $ cmake --build engine/build/arm64-macos --target dmengine


You can also specify the platform explicitly:
```
$ ./scripts/build.py install_ext --platform=arm64-android
$ ./scripts/build.py build_engine --platform=arm64-android
```

### Platforms

*In the instructions below, the `--platform` argument to `build.py` will default to the host platform if not specified.*

The public build targets in `scripts/build.py` are:

* `x86_64-linux`
* `arm64-linux`
* `x86_64-macos`
* `arm64-macos`
* `x86_64-win32`
* `arm64-ios`
* `arm64_sim-ios`
* `armv7-android`
* `arm64-android`
* `x86_64-android`
* `wasm-web`
* `wasm_pthread-web`

See [supported hosts and targets](README_SETUP.md#supported-hosts--targets) for host/target combinations. Private console platforms require their separate platform repositories and SDKs.

### STEP 1 - Setup environment

Start by setting up the build environment:

```sh
$ ./scripts/build.py shell
```

This starts a new shell with the build environment set, including `DEFOLD_HOME`, `DYNAMO_HOME`, `JAVA_HOME`, `PYTHONPATH`, `PATH`, and `CMAKE_GENERATOR=Ninja`. By default, `DYNAMO_HOME` is `<repository>/tmp/dynamo_home`.

### STEP 2 - Install SDKs (when needed)

You can skip `install_sdk` when the build system can find suitable local SDKs for your host and target platforms. Follow the [platform SDK setup](README_SETUP.md#required-software---platform-sdks) before running `install_ext`.

Building `armv7-android` from macOS also requires Emscripten and Node.js for LuaJIT's 32-bit generators, even though the target is Android. Set up a local Emscripten SDK using `EMSDK`, or use `install_sdk --platform=armv7-android` to install the packaged SDKs.

<details><summary>Install packaged SDKs</summary>

The `install_sdk` command installs packaged SDKs such as the Android SDK/NDK and Emscripten SDK into the build environment. For SDKs that cannot be distributed with Defold, provide your own package location as described in the [setup guide](README_SETUP.md).

Install the SDK before running `install_ext`, which needs it to build source dependencies. Use `--package-path` to override the default package location:

```sh
$ ./scripts/build.py install_sdk --platform=... --package-path=...
```

You could also set the package path in an environment variable `DM_PACKAGES_URL`:

```sh
$ DM_PACKAGES_URL=https://my.url ./scripts/build.py install_sdk --platform=...
```

</details>


### STEP 3 - Install dependencies

Install for the current host platform (e.g x86_64-win32):
```sh
$ ./scripts/build.py install_ext
```

Or for another target platform:
```sh
$ ./scripts/build.py install_ext --platform=arm64-android
```

It is important that you provide the `--platform` if you target a platform other than the host platform.
With host platform, we mean any of the `x86_64-win32`, `x86_64-macos`, `arm64-macos`, `x86_64-linux` or `arm64-linux`.

The `install_ext` command first installs the remaining prepackaged dependencies
from `./packages` and support files, then calls `build_ext` to check the SDK and
build and install source dependencies with CMake. These include Bullet, Basis
Universal, LZ4, both Box2D versions, HarfBuzz, libunibreak, Opus, SheenBidi,
Skribidi, and LuaJIT on non-web targets. Desktop builds also include
meshoptimizer, Draco, Ogg, and Protobuf; Linux builds include OpenH264.
See [external/CMakeLists.txt](external/CMakeLists.txt) for the current platform conditions.
The DXC shader tools for all five desktop host platforms are installed from
versioned archives in `packages`; normal builds do not compile LLVM/DXC.
Explicit DXC package rebuilds use pinned, checksum-verified source archives and
an isolated LLVM build. See [DXC build notes](external/dxc/README.md) for package
rebuilds, the Ubuntu 22.04 Docker workflow for both Linux architectures, focused
builds, and shader prototype validation.
Mobile and web platform backends are built with the engine.
Cross-builds build source dependencies for both the host and target platform.
These source dependencies are no longer installed from package archives.

When `install_ext` finishes, the dependencies are installed in `${DYNAMO_HOME}/ext`.
Run it for each target platform before building the engine or packaging a local
platform SDK, and repeat it after `distclean`.

Run `build_ext` when only the external sources or toolchain change, or `install_ext`
to also reinstall the packages.
Subsequent calls reuse the CMake build cache; ordinary engine rebuilds use the
installed libraries.

For a fresh external installation and build, run
`./scripts/build.py clean_ext install_ext --platform=<platform>`.
`clean_ext` removes external build directories and installed dependency files for
the selected platform (the host platform when `--platform` is omitted). It retains
other platforms, shared dependency files, platform SDKs, engine outputs, and
package archives outside the build directories.

This step also installs pinned Python dependencies into `${DYNAMO_HOME}/ext/lib/python`:

* `Markdown` and `Pygments` for script API documentation.
* `protobuf` for DDF generation.
* `Pillow`, `boto3`, `PyYAML`, `pystache`, `rangehttpserver`, and `requests` for build tools, tests, and release scripts.

All of these are installed with pip from bundled wheels in `packages/python`, using `--no-index`; this step does not download Python packages from PyPI. The pinned versions are listed in `install_ext_packages` in [scripts/build.py](scripts/build.py).

DDF generators use these bundled Python dependencies without loading host
site-packages. `install_ext` checks that the bundled Python Protobuf runtime loads,
and `check_sdk` checks both the runtime and Protoc. CMake first searches
`${DYNAMO_HOME}/ext/bin/<host-platform>/` for Protoc and falls back to `PATH`
when the host compiler is absent. Reconfiguring an existing build replaces any
previously cached local compiler with the SDK compiler.

`./scripts/build.py check_sdk` shows tool versions and status. Add `--verbose`
to include tool paths and per-check timings.

### STEP 4 - Build the engine

With the setup and installation done you're ready to build the engine:

```sh
$ ./scripts/build.py build_engine --platform=...
```

This builds and installs the engine, builds the test binaries, and runs the enabled unit tests when the host can run the target. To skip running tests while still building them:

```sh
$ ./scripts/build.py build_engine --platform=... --skip-tests
```

To skip both building and running tests:

```sh
$ ./scripts/build.py build_engine --platform=... --skip-tests -- --skip-build-tests
```

Android tests require a connected device or emulator with a compatible ABI. Select it with `--test-device <serial>` or `ANDROID_SERIAL`. iOS tests run from macOS and also accept `--test-device` for a device or simulator; physical devices require code signing and provisioning. Web tests require Bun or Node.js on a supported host. Use `--skip-tests` when only building for these platforms.

Options after `--`, such as `--opt-level=0`, `--with-asan` and `--with-ubsan`, are translated into CMake settings. The default build type is `RelWithDebInfo`; `--opt-level=0` selects `Debug`. Installed engine binaries are written to `${DYNAMO_HOME}/bin/<platform>`.

---

## Rebuilding the engine

After the first `build_engine`, run CMake from the repository root to rebuild individual targets. These examples use `arm64-macos`; replace it with your configured platform.

```sh
# Rebuild only dlib
$ cmake --build engine/build/arm64-macos --target dlib

# Rebuild changed dependencies and relink the engine
$ cmake --build engine/build/arm64-macos --target dmengine

# Install the built artifacts into DYNAMO_HOME (does not rebuild them)
$ cmake --install engine/build/arm64-macos
```

The `dmengine` target builds the engine in `engine/engine/build/<platform>`; it does not update the installed copy in `DYNAMO_HOME`. Before a full install, build any other targets whose installed artifacts also need to be refreshed, or rerun `build_engine` to build and install everything.

To change build options, run `build.py` again. For example, to enable AddressSanitizer:

```sh
$ ./scripts/build.py build_engine --platform=arm64-macos -- --with-asan
```

---

## Unit tests

`build_engine` runs the enabled unit tests when the target is runnable, unless `--skip-tests` is specified. If the build was configured with `--skip-build-tests`, rerun `build_engine` without that flag to restore the test targets.

After configuring with tests enabled, build and run all enabled tests through CMake:

```sh
$ cmake --build engine/build/arm64-macos --target run_tests
```

To build and run just the `test_dlib` executable:

```sh
$ cmake --build engine/build/arm64-macos --target run_test_dlib
```

To build the test without running it, use the `test_dlib` target. On a native desktop build, run the executable from its module directory and use `--test-filter` to select tests by a substring of the test name (see the [jctest documentation](https://jcash.github.io/jctest/api/03-runtime/#command-line-options)):

```sh
$ cmake --build engine/build/arm64-macos --target test_dlib
$ cd engine/dlib
$ ./build/arm64-macos/src/test/test_dlib --test-filter HashToString32
```
