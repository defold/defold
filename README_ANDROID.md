# Android

## Installation

### Android SDK/NDK

#### SDK locations

The engine build checks for the packaged SDK and NDK under `$DYNAMO_HOME/ext/SDKs` first. For an Android Studio installation, set `ANDROID_HOME` to the absolute SDK path. The usual locations are:

* macOS — `~/Library/Android/sdk`
* Windows — `%LOCALAPPDATA%/Android/Sdk`
* Linux — `$HOME/Android/Sdk`. For example, run `export ANDROID_HOME="$HOME/Android/Sdk"` before building.

#### Android Studio

For the easiest build setup, you can install [Android Studio](https://developer.android.com/studio)
Once installed, you need to also install the NDK.
Open "Settings" -> "Languages & Frameworks" -> "Android SDK". From the "SDK Tools" tab choose "NDK (Side by side)".

For a local installation, the build prefers the configured Build Tools version and the newest installed NDK matching the configured major version. If either is unavailable, it looks for another installed version. The target and minimum API levels are defined in [sdk.py](build_tools/sdk.py).

#### Version specific builds

For packaged SDKs used by CI, the required versions and API levels are defined in [build_tools/sdk.py](./build_tools/sdk.py). Install the SDK and NDK with `build.py install_sdk` before running `install_ext`, as described in the [setup guide](./README_SETUP.md#required-software---platform-sdks).

For local SDK installations, use Android Studio as described above or set `ANDROID_HOME` to the SDK directory.

## General

The engine's `android_main` registers the NDK's `android_app` with `dmAndroid::SetAndroidApp()` and passes it to the platform backend. Engine code can retrieve it with `dmAndroid::GetAndroidApp()`. See the [Android entry point](engine/engine/src/common/android_main.cpp) and the [dlib Android API](engine/dlib/src/dmsdk/dlib/android.h).

### Android Resources and R.java

AAPT2 assigns resource IDs when linking the app’s resources and generates `R.java` for the app and the library packages requested by Extender. Include each library’s resources in the build and leave its generated `R` classes out of precompiled JARs so the identifiers match the final package. See [Extender’s Android build configuration](share/extender/build_input.yml) and the [Android AAPT2 guide](https://developer.android.com/tools/aapt2).

Defold's modified NDK glue is in [android_native_app_glue.c](engine/platform/src/native/android/android_native_app_glue.c).

### Android testing

Copy executable (or directory) with

    # adb push <DIR_OR_DIR> /data/local/tmp

When copying directories append directory name to destination path. It's otherwise skipped

Run exec with:

    # adb shell /data/local/tmp/....

For interactive shell run "adb shell"


### Caveats

If the app is started programmatically, the life cycle behaves differently. Deactivating the app and then activating it by clicking on it results in a new
create message being sent (onCreate/android_main). The normal case is for the app to continue through e.g. onStart.

### Android debugging

Build a debug APK with `android.debuggable` enabled in `game.project`. In Android Studio, choose **Profile or Debug APK**, open the APK, and add the matching unstripped native library for the device ABI. Launch the app and attach the native LLDB debugger to its process. See [Debug a pre-built APK](https://developer.android.com/studio/debug/apk-debugger) and the [crash symbolication instructions](README_DEBUGGING.md#android-symbols).

### Android activity and window lifecycle

At startup, `engine_main` registers the activity callbacks and waits for `APP_CMD_INIT_WINDOW` before starting the engine thread. The Android looper continues to receive lifecycle and input events. The engine thread processes queued events when it polls the window. See the [engine startup code](engine/engine/src/engine_main.cpp).

Android delivers activity events (`APP_CMD_RESUME` and `APP_CMD_PAUSE`) separately from window events (`APP_CMD_INIT_WINDOW` and `APP_CMD_TERM_WINDOW`). The backend marks the window as iconified until the activity is resumed and a drawable window is available. For OpenGL ES it checks the EGL surface. For backends that manage their own surfaces it checks the native window. After a pause, Android can kill the process without delivering `APP_CMD_DESTROY`. See the [Android command handler](engine/platform/src/native/android/android_init.c).

With OpenGL ES, losing the native window releases its `EGLSurface`. The backend keeps the `EGLContext` and creates a surface for the replacement window. Closing the engine window releases both the surface and the context.

### Pulling APKs from a device

Use `adb shell pm list packages` to find the package name, then list its installed APKs. For example:

```sh
adb shell pm path com.defold.adtest
```

For each path reported after `package:`, run `adb pull` with that path to copy the APK. A split installation has a base APK and one or more split APKs.

## Upgrading SDK (i.e. API LEVEL)

In essence, updating the "sdk" means to update the supported api level.
This is done by updating `defold/packages/android-<android version>-common.tar.gz`.
The package contains `android.jar`, which is shared by all target architectures
and is also required when building Bob on desktop hosts.

For the distinction between minimum and target API levels, see Android’s [uses-sdk reference](https://developer.android.com/guide/topics/manifest/uses-sdk-element.html) and [platform versions](https://developer.android.com/about/versions).

### AAPT2 binaries

Bob uses `aapt2` to compile and package Android resources. When updating Build Tools, update the corresponding `aapt2` archives in [packages](packages/) and their entries in [scripts/build.py](scripts/build.py). Bob's [tools.json](com.dynamo.cr/com.dynamo.cr.bob/tools.json) maps the installed host binaries into the JAR.

### Creating the android tar ball

Creating a new android package is straight forward:

    APILEVEL=36
    mkdir -p sdkpack_android
    cd sdkpack_android
    mkdir -p share/java
    cp ../tmp/dynamo_home/ext/SDKs/android-sdk/platforms/android-$APILEVEL/android.jar share/java
    ./../scripts/mobile/android_jar_reduce_size.sh share/java/android.jar
    COPYFILE_DISABLE=1 tar -cvzf android-$APILEVEL-common.tar.gz share
    cp android-$APILEVEL-common.tar.gz ../packages


### Update build script

Update `ANDROID_TARGET_API_LEVEL` in `build_tools/sdk.py`. The common package
list in `scripts/build.py` uses `sdk.ANDROID_PACKAGE` to select the matching
archive automatically.

Check references to `ANDROID_BUILD_TOOLS_VERSION`, `ANDROID_TARGET_API_LEVEL` and `ANDROID_PLATFORM` in the build scripts and CI workflows when changing SDK versions.

### Update Extender

Update the matching Android image in Extender so native extensions build with the same SDK, Build Tools and NDK as the engine. See the [Android image definition](https://github.com/defold/extender/blob/dev/server/docker/Dockerfile.android.ndk25_sdk36-env).

### Reinstall and rebuild

    $ ./scripts/build.py distclean
    $ ./scripts/build.py install_sdk --platform=arm64-android
    $ ./scripts/build.py install_ext --platform=arm64-android
    $ ./scripts/build.py build_engine --platform=arm64-android --skip-docs --skip-tests

## Energy Consumption

      adb shell dumpsys cpuinfo
