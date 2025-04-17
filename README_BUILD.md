# Build Engine

Defold uses the Python based build system [Waf](https://waf.io/). Most of the interaction is done through the `build.py` script but it is also possible to use Waf directly.

## IMPORTANT PREREQUISITE - SETUP!

Make sure you have followed the [setup guide](/README_SETUP.md) before attempting to build the engine. If you do not install all of the required software from the setup guide your attempts to build the engine will likely fail.

## IMPORTANT PREREQUISITE - PLATFORM SDKs!

Our build setup can find the installation of the platform SDK's for the common platforms.

See the setup for the [Platform SDK](./README_SETUP.md#platform-sdk)

<details><summary>Locally installed platform sdks</summary><p>

### Using local installation

Our build setup can find the installation of the platform SDK's for the common platforms.
See [Platform SDK](./README_SETUP.md#platform-sdk)

If you have these tools installed, you can skip the `./scripts/build.py install_sdk` step altogether.

Since support for locally installed SDK's is in progress, some platforms still _do_ require the `--install_sdk` step, and thus requires you to have the prepackaged sdk's available.

### Prepackaged SDKs

This step is currently needed for HTML5.

Due to licensing restrictions **the SDKs are not distributed with Defold**. You need to provide these from a URL accessible by your local machine so that `build.py` and the `install_ext` command can download and unpack them.

__In order to simplify this process we provide scripts to download and package the SDKs__ [Read more about this process here](/scripts/package/README.md).

The path to the SDKs can either be passed to `build.py` using the `--package-path` option or by setting the `DM_PACKAGES_URL` environment variable.

</p></details>

## Standard workflow

The standard workflow when building the engine is the following:

1. Setup environment
2. Install platform specific libraries and SDKs
3. Build the engine

When working on a new feature or fixing a bug you start by first building the engine once as described above. You then proceed to develop your feature or fix the bug and rebuild and test changes until satisfied. When you do a rebuild you can speed things up by only building the parts that have changed.

## Build examples

To give an quick overview of the steps required.
(The following paragraphs will explain in more detail what each step does.)

The `--platform=` is implied in these examples, as it defaults to the host platform (x86_64-win32, x86_64-linux, x86_64-macos or arm64-macos)

Once per session:
```
$ ./scripts/build.py shell          # creates the shell. No need for "--platform"
```

Once per platform to be built
```
$ ./scripts/build.py install_ext    # extracts packages
$ ./scripts/build.py check_sdk      # checks that it finds the platform SDK
```

Build full engine, docs, bob light, tests + running the tests

    $ ./scripts/build.py build_engine

Build full engine, but without: docs, bob light, tests, skipping the tests (for a significant speedup)

    $ ./scripts/build.py build_engine --skip-docs --skip-bob-light --skip-tests -- --skip-build-tests

Rebuild a single library, and then relink the dmengine executable. No tests are run. The fastest option while iterating on a feature.

    $ ./scripts/submodule x86_64-win32 gamesys


You can also specify the platform explicitly:
```
$ ./scripts/build.py install_ext --platform=arm64-android
$ ./scripts/build.py check_sdk --platform=arm64-android
$ ./scripts/build.py build_engine --platform=arm64-android
```

### Platforms

*In the instructions below, the `--platform` argument to `build.py` will default to the host platform if not specified.*

The following platforms are supported:

* `x86_64-linux`
* `x86_64-macos`
* `arm64-macos`
* `win32`
* `x86_64-win32`
* `x86_64-ios`
* `arm64-ios`
* `armv7-android`
* `arm64-android`
* `js-web`
* `wasm-web`
* `wasm_pthread-web`

### STEP 1 - Setup environment

Start by setting up the build environment:

```sh
$ ./scripts/build.py shell
```

This will start a new shell with all of the required environment variables set (`DYNAMO_HOME` etc).

### STEP 2 - Install packages

Next thing you need to do is to install external packages:

Install for the current host platform (e.g x86_64-win32)
```sh
$ ./scripts/build.py install_ext
```

Or for another target platform
```sh
$ ./scripts/build.py install_ext --platform=arm64-android
```

It is important that you provide the `--platform` if you target a platform other than the host platform.
With host platform, we mean any of the `x86_64-win32`, `x86_64-macos`, `arm64-macos` or `x86_64-linux`.

When the `install_ext` command has finished you will find the external packages and any downloaded SDKs in `${DYNAMO_HOME}/ext`.

**IMPORTANT!**
You need to rerun the `install_ext` command for each target platform, as different packages and SDKs are installed.

#### Installing packages
The `install_ext` command starts by installing external packages, mostly pre-built libraries for each supported platform, found in the `./packages` folder. External packages are things such as Bullet and Protocol Buffers (a.k.a. protobuf).

This step also installs some Python dependencies:

* `boto` - For interacting with AWS. Installed from wheel package in `packages/`.
* `markdown` - Used when generating script API docs. Installed from wheel package in `packages/`.
* `protobuf` - Installed from wheel package in `packages/`
* `Pygments` - For use by the `CodeHilite` extension used by `markdown` in `script_doc.py`. Installed from wheel package in `packages/`.
* `requests` - Installed using pip
* `pyaml` - Installed using pip

### Step 3 - Installing SDKs

NOTE: As mentioned above, you may skip this step if your host OS and target OS is in the supported list of platforms that can use the local (host) installations of sdks.
(Most likely, it is)
See [Platform SDK](./README_SETUP.md#platform-sdk)

<details><summary>Install sdk</summary><p>

See [./README_SETUP.md]()

The `install_sdk`command will install SDKs (build tools etc) such as the Android SDK when building for Android or the Emscripten SDK for HTML5.

If you wish to build for any other platform, you will need to install an sdk package where the build system can find it.

Next thing you need to do is to install external packages:

```sh
$ ./scripts/build.py install_sdk --platform=... --package-path=...
```

You could also set the package path in an environment variable `DM_PACKAGES_URL`:

```sh
$ DM_PACKAGES_URL=https://my.url ./scripts/build.py install_sdk --platform=...
```

</p></details>


### STEP 4 - Build the engine

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

And you have the commands `clean`,  `build`, `install`.
ALso some common options `--opt-level=<opt_level>`, `--skip-tests` or `--target=<artifact>`

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
