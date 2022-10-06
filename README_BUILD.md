# Build Engine

Defold uses the Python based build system [Waf](https://waf.io/). Most of the interaction is done through the `build.py` script but it is also possible to use Waf directly.

## IMPORTANT PREREQUISITE - SETUP!

Make sure you have followed the [setup guide](/README_SETUP.md) before attempting to build the engine. If you do not install all of the required software from the setup guide your attempts to build the engine will likely fail.

## IMPORTANT PREREQUISITE - PACKAGE SDKs!

### Local SDKs
If you're a contributor, chances are that you have the necessary sdk's and tools already installed (E.g. Windows: Visual Studio+Windows SDK, macOS: Xcode or Linux: clang++)
If this is the case, then you won't have to run the `install_sdk` build command, and thus would have no requirement for any prepackaged sdks.

Since support for locally installed SDK's is in progress, some platforms still _do_ require the `--install_sdk` step, and thus requires you to have the prepackaged sdk's available.

### Prepackaged SDKs

Due to licensing restrictions **the SDKs are not distributed with Defold**. You need to provide these from a URL accessible by your local machine so that `build.py` and the `install_ext` command can download and unpack them. In order to simplify this process we provide scripts to download and package the SDKs. Read more about this process [here](/scripts/package/README.md). The path to the SDKs can either be passed to `build.py` using the `--package-path` option or by setting the `DM_PACKAGES_URL` environment variable.

## Standard workflow

The standard workflow when building the engine is the following:

1. Setup environment
2. Install platform specific libraries and SDKs
3. Build the engine

When working on a new feature or fixing a bug you start by first building the engine once as described above. You then proceed to develop your feature or fix the bug and rebuild and test changes until satisfied. When you do a rebuild you can speed things up by only building the parts that have changed.


### Platforms

*In the instructions below, the `--platform` argument to `build.py` will default to the host platform if not specified.*

The following platforms are supported:

* `x86_64-linux`
* `x86_64-macos`
* `win32`
* `x86_64-win32`
* `x86_64-ios`
* `arm64-ios`
* `armv7-android`
* `arm64-android`
* `js-web`
* `wasm-web`

### STEP 1 - Setup environment

Start by setting up the build environment:

```sh
$ ./scripts/build.py shell
```

This will start a new shell with all of the required environment variables set (`DYNAMO_HOME` etc).

### STEP 2 - Install packages and SDKs

Next thing you need to do is to install external packages and download required platform SDKs:

```sh
$ ./scripts/build.py install_ext --platform=... --package-path=...
```

It is important that you provide the `--platform` option to let the `install_ext` command know which platform you intend to build for (the target platform). When the `install_ext` command has finished you will find the external packages and downloaded SDKs in `./tmp/dynamo_home/ext`.

**IMPORTANT!**
You need to rerun the `install_ext` command whenever you switch target platform, as different packages and SDKs are installed.

#### Installing packages
The `install_ext` command starts by installing external packages, mostly pre-built libraries for each supported platform, found in the `./packages` folder. External packages are things such as Bullet and Protocol Buffers (a.k.a. protobuf). This step also installs some Python dependencies such as `requests`, `boto` and `Pygments`.

#### Installing SDKs
The second step of the `install_ext` command will install SDKs (build tools etc) such as the Android SDK when building for Android or the Emscripten SDK for HTML5.

### STEP 3 - Build the engine

With the setup and installation done you're ready to build the engine:

```sh
$ ./scripts/build.py build_engine --platform=...
```

This will build the engine and run all unit tests. In order to speed up the process you can skip running the tests:

```sh
$ ./scripts/build.py build_engine --platform=... --skip-tests -- --skip-build-tests
```

Anything after `--` is passed directly as arguments to Waf. The built engine ends up in `./tmp/dynamo_home/bin/%platform%`.

---

## Rebuilding the engine

When you are working on a specific part of the engine there is no need to rebuild the whole thing to test your changes. You can use Waf directly to build and test your changes (see Unit tests below for more information about running tests):

```sh
$ cd engine/dlib
$ waf
```

You can also use rebuild a specific part of the engine and create a new executable:

```sh
# Rebuild dlib and sound modules and create a new executable
$ ./scripts/submodule.sh x86_64-macos dlib sound
```

---

## Unit tests

Unit tests are run automatically when invoking waf if not `--skip-tests` is specified. A typically workflow when working on a single test is to run:

```sh
$ waf --skip-tests && ./build/default/.../test_xxx
```

With the flag `--test-filter=` it's possible to run a single test in the suite, see [jctest documentation](https://jcash.github.io/jctest/api/03-runtime/#command-line-options)
