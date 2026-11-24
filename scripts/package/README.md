# Packaging the SDK's

Most of the SDKs used by Defold have licenses that prevent third party redistribution. To efficiently work with the build servers and our build pipeline it is recommended to package the SDKs and serve them from a private URL, accessible by `build.py`. The URL is defined as `CDN_PACKAGES_URL` in `build.py` (or `DM_PACKAGES_URL` environment variable).

We provide a number of scripts to package the SDKs into a format expected by `build.py`. The basic principle when packaging an SDK is to "zip it up" and put it in a cloud storage that the build scripts can access later on. Some of the package scripts can be run on a single host, and yet package the SDK for another host. However, some scripts need to be run on a certain host (for instance various installers).

Usually, the folders can be packaged as-is, but it's generally preferable to remove redundant folders in order to minimize download and installation sizes.
(E.g. for the extender Docker container)

## Installation

This step is automated given the proper archives (sdk's) are available either locally or on a file server.
You can configure the path via a command line option:

	 ./scripts/build.py --package-path=<local_folder_or_url> install_sdk --platform=<platform>

or by specifying the `DM_PACKAGES_URL` environment variable.

As a result, all packages should be unpacked under the path `$DYNAMO_HOME/ext/SDKs`:

	./tmp/dynamo_home/ext/SDKs
	├── android-ndk-r25b
	└── android-sdk

## Android

### Prerequisites

To package the SDK, you need to run it on Linux/macOS/Windows.
The NDK can be packaged on any host for any host.

The [Package Android SDKs workflow](../../.github/workflows/pack-android-sdks.yml) shows the host setup. On Apple silicon it runs the SDK tools with x64 Java 11 under Rosetta.

### Packaging

#### Android NDK

Run the script (on any host):

	$ ./scripts/package/package_android_ndk.sh darwin
	$ ./scripts/package/package_android_ndk.sh linux
	$ ./scripts/package/package_android_ndk.sh windows

and it will output the package in:

```
	local_sdks/android-ndk-r25b-darwin.tar.gz
	local_sdks/android-ndk-r25b-linux.tar.gz
	local_sdks/android-ndk-r25b-windows.tar.gz
```

#### Android SDK

*Since this step is running an actual installer, it is required to be run on the respective host platform.*

Run the script from the repository root on each host:

```sh
./scripts/package/package_android_sdk.sh
```

The script downloads the command-line tools, installs the selected platform and build tools, and writes `local_sdks/android-sdk-<host>-android-<api>-<build-tools>.tar.gz`. The archive uses `darwin`, `linux`, or `win` for the host. For example, the Windows archive for API 36 and Build Tools 36.1.0 is `android-sdk-win-android-36-36.1.0.tar.gz`.

#### Android strip tool

Bob strips the Android engine and native extension libraries when bundling with
`--strip-executable`. The tool is the NDK's `llvm-strip`, which reads every Android ABI, so one
package covers armv7/arm64/x86_64.

Run the script once per host (`linux` and `windows` package from any host, `darwin` needs macOS
because it uses `lipo` to thin the NDK's universal binary, and `linux` needs `patchelf`):

	$ ./scripts/package/package_android_strip.sh linux
	$ ./scripts/package/package_android_strip.sh windows
	$ ./scripts/package/package_android_strip.sh darwin

and it will output packages named after the NDK's LLVM version in `local_sdks`:

```
	local_sdks/strip_android-14.0.6-x86_64-linux.tar.gz
	local_sdks/strip_android-14.0.6-x86_64-win32.tar.gz
	local_sdks/strip_android-14.0.6-x86_64-macos.tar.gz
	local_sdks/strip_android-14.0.6-arm64-macos.tar.gz
```

Only the macOS tool is self contained. The linux package also carries the NDK's `libc++.so.1`
(and the script rewrites the tool's runpath to `$ORIGIN` so it is found next to the executable),
and the windows package carries `libwinpthread-1.dll`. Bob unpacks everything in a `libexec`
platform folder side by side, so both are found at runtime.

Unlike the SDKs above these are small enough to live in `packages/`, so copy them there and update
the version in the `PACKAGES_*` lists in `scripts/build.py`.


## iOS + macOS

### Prerequisites

This script packages the installed Xcode selected by `DEVELOPER_DIR` or `xcode-select`. Download Xcode from [Apple Developer Downloads](https://developer.apple.com/download/) and select the version listed in [sdk.py](https://github.com/defold/defold/blob/dev/build_tools/sdk.py) before running the script. Xcode 27 requires an Apple silicon Mac for packaging; its macOS SDK can still build Intel targets.

### Packaging

Run the script

	./scripts/package/package_xcode_and_sdks.sh

and it will output files in `local_sdks` (version depends on which is the current recommended version):

	./local_sdks/MacOSX27.0.sdk.tar.gz
	./local_sdks/XcodeDefault27.1.xctoolchain.darwin.tar.gz
	./local_sdks/iPhoneOS27.1.sdk.tar.gz
	./local_sdks/iPhoneSimulator27.1.sdk.tar.gz

## Windows

Run the script

	./scripts/package/package_win32_sdk.sh

The script uses the system-installed `vswhere.exe` from the Visual Studio Installer to locate the active MSVC toolchain and Windows 10 SDK.

## HTML5

Run `./scripts/package/package_emscripten.sh` on each host to create its SDK archive in `local_sdks/`. Install from that directory with `./scripts/build.py install_sdk --platform=wasm-web --package-path=local_sdks`. See the [Emscripten guide](../../README_EMSCRIPTEN.md) for local SDK setup and tests.

## Linux

Linux builds use Clang from `PATH`. For the required version and libraries, see the [engine setup guide](../../README_SETUP.md#required-software---platform-sdks) or use the [repository's Ubuntu build image](../docker/README.md).
