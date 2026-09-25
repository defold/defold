# Build Engine

Defold uses CMake and Ninja. The `scripts/build.py` script handles dependency installation and full engine builds; you can use CMake directly for incremental builds and individual tests.

## IMPORTANT PREREQUISITE - SETUP

Make sure you have followed the [setup guide](/README_SETUP.md) to install the tools and sdk's you want before attempting to build the engine. If you do not install all of the required software from the setup guide your attempts to build the engine will likely fail.

## Standard workflow

The standard workflow when building the engine is the following:

1. [Setup](/README_SETUP.md) environment
2. [Install](/README_SETUP.md#required-software---platform-sdks) platform SDKs
3. Install packages and build source dependencies with `install_ext`
4. Build the engine

When working on a new feature or fixing a bug you start by first building the engine once as described above. You then proceed to develop your feature or fix the bug and rebuild and test changes until satisfied. When you do a rebuild you can speed things up by only building the parts that have changed.

*NOTE: Make sure you also have read our [Contribution Guide](https://github.com/defold/defold/blob/build-doc-update/CONTRIBUTING.md). Especially before undertaking larger tasks.*

**NOTE for Linux users**: To avoid any problems with version of shared dependencies engine will link with - use Docker container to build the engine. More information [here](/scripts/docker/README.md).

## Build examples

To give an quick overview of the steps required.
(The following paragraphs will explain in more detail what each step does.)

The `--platform=` is implied in these examples, as it defaults to the host platform (x86_64-win32, x86_64-linux, arm64-linux, x86_64-macos or arm64-macos)

Once per session:
```
$ ./scripts/build.py shell          # creates the shell. No need for "--platform"
```

Once per platform to be built
```
$ ./scripts/build.py install_ext    # extracts packages, checks the SDK, then builds source dependencies
```

Set up the platform SDK before running `install_ext`. Repeat the installation
after `distclean`. Re-run `install_ext` when the external
sources or toolchain change; ordinary engine rebuilds use the installed libraries.

Build full engine, docs, bob light, tests + running the tests

    $ ./scripts/build.py build_engine

Build Android tests against a specific connected device serial

    $ ./scripts/build.py build_engine --platform=arm64-android --test-device <serial>

Build full engine, but without: docs, bob light, tests, skipping the tests (for a significant speedup)

    $ ./scripts/build.py build_engine --skip-docs --skip-bob-light --skip-tests -- --skip-build-tests

Rebuild changed dependencies and relink the dmengine executable. No tests are run.

    $ cmake --build engine/build/x86_64-win32 --target dmengine


You can also specify the platform explicitly:
```
$ ./scripts/build.py install_ext --platform=arm64-android
$ ./scripts/build.py build_engine --platform=arm64-android
```

### Platforms

*In the instructions below, the `--platform` argument to `build.py` will default to the host platform if not specified.*

The following platforms are supported:

* `x86_64-linux`
* `x86_64-macos`
* `arm64-macos`
* `x86_64-win32`
* `arm64-ios`
* `arm64_sim-ios`
* `armv7-android`
* `arm64-android`
* `wasm-web`
* `wasm_pthread-web`

### STEP 1 - Setup environment

Start by setting up the build environment:

```sh
$ ./scripts/build.py shell
```

This will start a new shell with all of the required environment variables set (`DYNAMO_HOME` etc).

### STEP 2 - Install SDKs (when needed)

NOTE: As mentioned above, you may skip this step if your host OS and target OS is in the supported list of platforms that can use the local (host) installations of sdks.
(Most likely, it is)
See [Supported Hosts + Targets](./README_SETUP.md#supported-hosts-targets)

<details><summary>Install sdk</summary><p>

See [./README_SETUP.md]()

The `install_sdk`command will install SDKs (build tools etc) such as the Android SDK when building for Android or the Emscripten SDK for HTML5.

If you wish to build for any other platform, you will need to install an sdk package where the build system can find it.

Install the SDK before running `install_ext`, which needs it to build source dependencies:

```sh
$ ./scripts/build.py install_sdk --platform=... --package-path=...
```

You could also set the package path in an environment variable `DM_PACKAGES_URL`:

```sh
$ DM_PACKAGES_URL=https://my.url ./scripts/build.py install_sdk --platform=...
```

</p></details>


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

The `install_ext` command first installs the prepackaged dependencies from `./packages`,
including Box2D and Protocol Buffers (a.k.a. protobuf). After installing the packages
and support files, it checks the SDK and builds and installs source
dependencies with CMake. These include Bullet, Basis Universal, LZ4, and GLFW on iOS.
Cross-builds build source dependencies for both the host and target platform.

When `install_ext` finishes, the dependencies are installed in `${DYNAMO_HOME}/ext`.
Run it for each target platform before building the engine or packaging a local
platform SDK, and repeat it after `distclean`.

Run `install_ext` again when the external sources or toolchain change.
Subsequent calls reuse the CMake build cache; ordinary engine rebuilds use the
installed libraries.

This step also installs some Python dependencies:

* `boto` - For interacting with AWS. Installed from wheel package in `packages/`.
* `markdown` - Used when generating script API docs. Installed from wheel package in `packages/`.
* `protobuf` - Installed from wheel package in `packages/`
* `Pygments` - For use by the `CodeHilite` extension used by `markdown` in `script_doc.py`. Installed from wheel package in `packages/`.
* `requests` - Installed using pip
* `pyaml` - Installed using pip

### STEP 4 - Build the engine

With the setup and installation done you're ready to build the engine:

```sh
$ ./scripts/build.py build_engine --platform=...
```

This will build the engine and run all unit tests. In order to speed up the process you can skip running the tests:

```sh
$ ./scripts/build.py build_engine --platform=... --skip-tests -- --skip-build-tests
```

When running Android tests, you can target a specific connected device either with `--test-device <serial>` or by setting `ANDROID_SERIAL` in the environment.

Options after `--`, such as `--opt-level=0` and `--with-asan`, are translated into CMake settings. The built engine ends up in `./tmp/dynamo_home/bin/%platform%`.

---

## Rebuilding the engine

After the first `build_engine`, run CMake from the repository root to rebuild individual targets. These examples use `arm64-macos`; replace it with your configured platform.

```sh
# Rebuild only dlib
$ cmake --build engine/build/arm64-macos --target dlib

# Rebuild changed dependencies and relink the engine
$ cmake --build engine/build/arm64-macos --target dmengine

# Install the updated artifacts into DYNAMO_HOME
$ cmake --install engine/build/arm64-macos
```

To change build options, run `build.py` again. For example, to enable AddressSanitizer:

```sh
$ ./scripts/build.py build_engine --platform=arm64-macos -- --with-asan
```

---

## Unit tests

`build_engine` runs unit tests unless `--skip-tests` is specified. After configuring with tests enabled, you can build and run a single test through CMake:

```sh
$ cmake --build engine/build/arm64-macos --target run_test_dlib
```

To build the test without running it, use the `test_dlib` target. You can then run the executable from its module directory and use `--test-filter` to select tests (see the [jctest documentation](https://jcash.github.io/jctest/api/03-runtime/#command-line-options)):

```sh
$ cmake --build engine/build/arm64-macos --target test_dlib
$ cd engine/dlib
$ ./build/arm64-macos/src/test/test_dlib --test-filter SomeTestPattern
```
