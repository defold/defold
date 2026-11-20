# Android

## Installation

### Android SDK/NDK

#### For contributors

The Android Studio installation will be automatically picked up from the default installation

* macOS - `~/Library/android/sdk`
* Windows - `%LOCALAPPDATA%/Android/Sdk`

or by specifying the `ANDROID_HOME` environment variable.

#### For the maintainers

_This is mainly for the Defold team and their CI_

When building the engine, the build system tries to find the sdk in this order:

* `$DYNAMO_HOME/ext/SDKs/android-sdk`

#### Android Studio

For the easiest build setup, you can install [Android Studio](https://developer.android.com/studio)
Once installed, you need to also install the NDK.
Open "Settings" -> "Languages & Frameworks" -> "Android SDK". From the "SDK Tools" tab choose "NDK (Side by side)".

The build system will find the most recent build tools and NDK versions.
However, the minimum API target numbers are fixed, and are specified our [sdk.py](./build_tools/sdk.py)

#### Version specific builds

For packaged SDKs used by CI, the required versions and API levels are defined in [build_tools/sdk.py](./build_tools/sdk.py). Install the SDK and NDK with `build.py install_sdk` before running `install_ext`, as described in the [setup guide](./README_SETUP.md#required-software---platform-sdks).

For local SDK installations, use Android Studio as described above or set `ANDROID_HOME` to the SDK directory.

## General

The engine's `android_main` stores the NDK's `android_app` with `dmAndroid::SetAndroidApp` and passes it to the platform backend. Engine code can retrieve it with `dmAndroid::GetAndroidApp`.

### Android Resources and R.java

Long story short. Static resources on Android are referred by an integer identifier. These identifiers are generated to a file R.java.
The id:s generated are conceptually a serial number and with no guarantees about uniqueness. Due to this limitations **all** identifiers
must be generated when the final application is built. As a consequence all resources must be available and it's not possible to package
library resources in a jar. Moreover, one identical *R.java* must be generated for every package/library linked with the final application.

This is a known limitation on Android.

**NOTE:** Never ever package compiled **R*.class-files** with third party libraries as it doesn't work in general.

The Android backend builds Defold's [copy of the NDK native app glue](engine/platform/src/native/android/android_native_app_glue.c).

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

There are two alternatives to debug native code on Android;

1. through the `ndk-gdb` script available in the Android NDK
2. starting a `gdbserver` and connecting manually

The first option is the preferred and easiest solution, but due to bugs with certain Android versions `ndk-gdb` does not always work. ([Android 4.2.2 and 4.3 are known to have issues.](http://visualgdb.com/KB/?ProblemID=nopkg))

#### Using ndk-gdb
* Go to the build directory for the engine subsystem, `engine/engine/build/default/src/dmengine.android/`
* This directory includes the debug app APK, `dmengine.apk` and has a structure that `ndk-gdb` understands.
* Install (`adb install dmengine.apk`) and launch the application.
* Run `ndk-gdb` from Android NDK; `$DYNAMO_HOME/ext/SDKs/android-ndk-<version>/ndk-gdb --start`

If you are having problems with `ndk-gdb` try running it with `--verbose` for troubleshooting. If you encounter the error below it might be easier to go with the `gdbserver` solution.

    ERROR: Could not extract package's data directory. Are you sure that
        your installed application is debuggable?

#### Using gdbserver
You need to have a executable binary of `gdbserver` available on the device. There are ways to transfer such binary (`adb put gdbserver ...`) to device, and then setting it executable (`adb shell "chmod +x ..."`), but the easiest way is to just rely on the `gdbserver` bundled together with `dmengine.apk`.

To get debug symbols you will need to download the system libraries from the device, like this:

    $ mkdir system_libs
    $ cd system_libs
    $ adb pull /system/lib
    $ cd ..
    $ mkdir vendor_libs
    $ cd vendor_libs
    $ adb pull /vendor/lib
    $ cd ..
    $ adb pull /system/bin/linker

Once you have an executable gdbserver on the device, and have downloaded the system libraries, do the following:

    $ adb pull /system/bin/app_process
This fetches the `app_process` binary from the Android device to your machine which you will need when starting `gdb` later on.

    $ adb forward tcp:5039 tcp:5039
This forwards the port 5039 from the Android device to your local machine.

Start your application on the device and find its PID (for example via `adb logcat | grep "ENGINE"`).

    $ adb shell "run-as <your-app-package-name> /data/data/com.defold.dmengine/lib/gdbserver :5039 --attach <PID>"
This will start the gdbserver (which in this case is located inside dmengine.apk) on port 5039 and attach to the running process id.

Next you will need to start a local gdb instance and connect to the gdbserver. The correct gdb binaries are located in the Android NDK:

    $ $DYNAMO_HOME/ext/SDKs/android-ndk-<version>/toolchains/arm-linux-androideabi-4.6/prebuilt/darwin-x86_64/bin/arm-linux-androideabi-gdb app_process
    ...
    (gdb) target remote :5039
    ...
    (gdb) set solib-search-path ./system_libs/:<path-to-dir-with-libdmengine.so>
    ...
    (gdb) continue

This will connect to your locally forwarded port, thus connecting to the gdbserver on device. Then you specify where the libraries can be found, which will give you symbols for easier debugging. Finally you continue the execution (it will be paused when you first connect).

### Android window lifecycle

At startup, `engine_main` registers the activity callbacks and waits for `APP_CMD_INIT_WINDOW` before starting the engine thread. The Android looper continues to receive lifecycle and input events. The engine thread processes the queued events when it polls the window.

The activity and the native window have separate lifetimes. Android can send `APP_CMD_TERM_WINDOW` and later `APP_CMD_INIT_WINDOW` while the same engine is running. Rendering resumes once the activity is resumed and a drawable window is available. After `APP_CMD_PAUSE`, Android can kill the process without delivering `APP_CMD_DESTROY`.

With OpenGL ES, losing the native window releases its `EGLSurface`. The backend keeps the `EGLContext` and creates a surface for the replacement window. Closing the engine window releases both the surface and the context.

### Pulling APKs from device

E.g. when an APK produces a crash, backing it up is always a good idea before you attempt to fix it.

## Determine package name:
  adb shell pm list packages
## Get the path on device:
  adb shell pm path <package-name>
## Pull the APK to local disk
  adb pull <package-path>


## Upgrading SDK (i.e. API LEVEL)

In essence, updating the "sdk" means to update the supported api level.
This is done by updating `defold/packages/android-<android version>-common.tar.gz`.
The package contains `android.jar`, which is shared by all target architectures
and is also required when building Bob on desktop hosts.

Some relevant links:

- https://developer.android.com/guide/topics/manifest/uses-sdk-element.html
- https://developer.android.com/about/versions
- https://developer.android.com/tools/support-library/index.html

### AAPT Binaries

We ship Android "aapt" (Android Asset Packaging Tool) binaries for all platforms that the editor runs on. When upgrading the Android SDK, these needs to be copied from the Android SDK directory into `com.dynamo.cr/com.dynamo.cr.bob/libexec/<PLATFORM>/`. Currently the Linux `aapt` binary is dynamically linked against a version of `libc++.so` that comes with the Android SDK. This forces us to also copy this library into `com.dynamo.cr/com.dynamo.cr.bob/libexec/x86_64-linux/lib/`.

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

Find and update all `ANDROID_BUILD_TOOLS_VERSION`, `ANDROID_TARGET_API_LEVEL` and `ANDROID_PLATFORM` in the `defold` project folder.

`ANDROID_BUILD_TOOLS_VERSION` maybe found here:
![Screenshot 2022-04-05 at 10 28 15](https://user-images.githubusercontent.com/2209596/161712727-f52f3616-1965-454b-87ef-f1f6bca1c037.jpg)

### Update Dockerfile

Update Android environment variables in [the extender's Dockerfile](https://github.com/defold/extender/blob/2960a454940bc4d2629af721e9dc48c60c4ed432/server/docker-base/Dockerfile#L207).


### Reinstall and rebuild

    $ ./scripts/build.py distclean
    $ ./scripts/build.py install_sdk --platform=arm64-android
    $ ./scripts/build.py install_ext --platform=arm64-android
    $ ./scripts/build.py build_engine --platform=arm64-android --skip-docs --skip-tests

## Energy Consumption

      adb shell dumpsys cpuinfo
