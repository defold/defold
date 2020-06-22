# Build Engine

Defold uses the Python based build system [Waf](https://waf.io/). Most of the interaction is done through the `build.py` script but it is also possible to use Waf directly.

## Standard workflow

The standard workflow when working on the engine is the following:

1. Setup platform specific environment
2. Install platform specific dependencies
3. (Re)Build the engine
4. Develop a feature
5. Test
6. Repeat steps 3-5 until done. Start from 1 when switching platform.

### Platforms

*In the instructions below, the `--platform` argument to `build.py` will default to the host platform if not specified.*

The following platforms are supported:

* `x86_64-linux`
* `x86_64-darwin`
* `win32`
* `x86_64-win32`
* `x86_64-ios`
* `armv7-darwin`
* `arm64-darwin`
* `armv7-android`
* `arm64-android`
* `js-web`
* `wasm-web`

### Setup environment

Start by setting up the build environment:

    $ ./scripts/build.py shell --platform=...

This will start a new shell with all of the required environment variables set (`DYNAMO_HOME` etc). This step is required whenever you switch target platform.

### Install packages and SDKs

Next thing you need to do is to install external packages and download required platform SDKs:

    $ ./scripts/build.py install_ext --platform=... --package-path=...

It is important that you provide the `--platform` option to let the `install_ext` command know which platform you intend to build for (the target platform). If you do not provide a platform the command will default to the host platform. When the `install_ext` command has finished you will find the external packages and downloaded SDKs in `./tmp/dynamo_home/ext`.

IMPORTANT! You need to rerun this command whenever you switch target platform, as different packages and SDKs are installed.

#### Installing packages
The command starts by installing external packages, mostly pre-built libraries for each supported platform, found in the `./packages` folder. External packages are things such as Bullet, Protocol Buffers (a.k.a. protobuf), WebP and c-ares. This step also installs some Python dependencies such as `requests`, `boto` and `Pygments`.

#### Installing SDKs
The second step of the `install_ext` command will install SDKs (build tools etc) such as the Android SDK when building for Android or the Emscripten SDK for HTML5.

Due to licensing restrictions the SDKs are not distributed with Defold. You need to provide these from a URL accessible by your local machine so that `build.py` and the `install_ext` command can download and unpack them. In order to simplify this process we provide scripts to download and package the SDKs. Read more about this process [here](/scripts/package/README.md). The path to the SDKs can either be passed to `build.py` using the `--package-path` option or by setting the `DM_PACKAGES_URL` environment variable.


### Build the engine

With these two steps done you're ready to build the engine:

    $ ./scripts/build.py build_engine --platform=...

This will build the engine and run all unit tests. In order to speed up the process you can skip running the tests:

    $ ./scripts/build.py build_engine --platform=... --skip-tests -- --skip-build-tests

Anything after `--` is passed directly as arguments to Waf. The built engine ends up in `./tmp/dynamo_home/bin/%platform%`.

### Rebuilding the engine

When you are working on a specific part of the engine there is no need to rebuild the whole thing to test your changes. You can use Waf directly to build and test your changes (see Unit tests below for more information about running tests):

    $ cd engine/dlib
    $ waf

You can also use rebuild a specific part of the engine and create a new executable:

    # Rebuild dlib and sound modules and create a new executable
    $ ./scripts/submodule.sh x86_64-darwin dlib sound

## Unit tests

Unit tests are run automatically when invoking waf if not `--skip-tests` is specified. A typically workflow when working on a single test is to run:

    $ waf --skip-tests && ./build/default/.../test_xxx

With the flag `--test-filter=` it's possible to run a single test in the suite, see [jctest documentation](https://jcash.github.io/jctest/api/03-runtime/#command-line-options)
