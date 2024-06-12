# Android

## Installation

### Android SDK/NDK

When building the engine, the build system tries to find the sdk in this order:

* `$DYNAMO_HOME/ext/SDKs/android-sdk` -  This is mainly for the Defold team and their CI
* `$ANDROID_HOME` - Set this path to point to your android studio sdk installation (e.g. `ANDROID_HOME=~/Library/Android/sdk` is the default installation on macOS)

#### Android Studio

For the easiest build setup, you can install [Android Studio](https://developer.android.com/studio)
Once installed, you need to also install the NDK.
Open "Settings" -> "Languages & Frameworks" -> "Android SDK". From the "SDK Tools" tab choose "NDK (Side by side)".

The build system will find the most recent build tools and NDK versions.
However, the minimum API target numbers are fixed, and are specified our [sdk.py](./build_tools/sdk.py)

#### Version specific builds

This step isn't required if you have Android Studio installed.
It is mainly for the Defold team and the build server setup.

**Note that the SDK version numbers aren't the same as the Api Level numbers!**

* Download SDK Tools 24.3.4 (or later) from here: [http://developer.android.com/sdk/index.html](http://developer.android.com/sdk/index.html)

NOTE: Newer versions of the SDK Tools come shipped with the new `sdkmanager` command line tool and not the old Android SDK Manager. The contents of the tools folder is also slightly different with an `sdklib-x.y.z.jar` instead of an `sdklib.jar` (as referenced by `waf_dynamo.py`). Older versions of the SDK Tools can be downloaded at the following URLs:

* http://dl-ssl.google.com/android/repository/tools_r[rev]-windows.zip
* http://dl-ssl.google.com/android/repository/tools_r[rev]-linux.zip
* http://dl-ssl.google.com/android/repository/tools_r[rev]-macosx.zip

Where [rev] corresponds to an SDK Tools revision, e.g. 23.0.2. The process of installation would be to create an `android-sdk` folder, unzip the downloaded SDK Tools into a `tools` folder inside `android-sdk` and then run the Android SDK Manager:

    cd android-sdk/tools
    ./android

Install the platform tools and build tools for a version matching what is defined in `waf_dynamo.py` (currently 23.0.2).

* Put the SDK in **~/android/android-sdk**
* The Android NDK is managed as a package and automatically installed via install_ext from the `build.py` script

Installer+GUI installation of those tools are a bit tricky, so it's recommended doing it via command line
Here are some commands to help out with the process:

    mkdir ~/android
    cd ~/android

    # Supported: macosx,linux,windows
    PLATFORM=macosx
    # The sdkmanager tool is provided in the Android SDK Tools package (25.2.3 and higher) and is located in android_sdk/tools/bin/ https://developer.android.com/studio/command-line/sdkmanager .
    TOOL_VERSION=25.2.3
    wget https://dl.google.com/android/repository/tools_r$TOOL_VERSION-$PLATFORM.zip
    tar xvf tools_r$TOOL_VERSION-$PLATFORM.zip
    mkdir android-sdk
    mv tools android-sdk/tools

    # You can list all the packages contained (not just the latest versions)
    # and see their aliases (e.g. "tools")
    ./android-sdk/tools/bin/sdkmanager --verbose --list --include_obsolete

    # you can use some aliases to install them
    # Note the API level version: E.g. "android-23"
    ./android-sdk/tools/bin/sdkmanager --verbose "tools" "platform-tools" "extras;android;m2repository" "platforms;android-23" "build-tools;23.0.2"

After installing the SDK check that the PATH env variable contains the path to the android sdk. If not, add it manually.

**Note** Newer version have the suffixes ".bin" or ".exe" as they are now installers.
Simply use that as the suffix, and and extract with **7z**

* How to launch the [Android Tool](http://developer.android.com/sdk/installing/adding-packages.html) manually


## General

By convention we currently have a weak reference to struct android\_app \* called g\_AndroidApp.
g\_AndroidApp is set by glfw and used by dlib. This is more or less a circular dependency. See sys.cpp and android_init.c.
Life-cycle support should probably be moved to dlib at some point.

### Android Resources and R.java

Long story short. Static resources on Android are referred by an integer identifier. These identifiers are generated to a file R.java.
The id:s generated are conceptually a serial number and with no guarantees about uniqueness. Due to this limitations **all** identifiers
must be generated when the final application is built. As a consequence all resources must be available and it's not possible to package
library resources in a jar. Moreover, one identical *R.java* must be generated for every package/library linked with the final application.

This is a known limitation on Android.

**NOTE:** Never ever package compiled **R*.class-files** with third party libraries as it doesn't work in general.

**NOTE2:** android_native_app_glue.c from the NDK has been modified to fix a back+virtual keyboard bug in OS 4.1 and 4.2, the modified version is in the glfw source.

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

### Life-cycle and GLFW

NDK uses a separate thread which runs the game, separate from the Android UI thread.

The main life cycle (LC) of an android app is controlled by the following events, received on the game thread:

* `_glfwPreMain(struct* android_app)`, corresponds to create
* APP_CMD_START, (visible)
* APP_CMD_RESUME
* APP_CMD_GAINED_FOCUS
* APP_CMD_LOST_FOCUS
* APP_CMD_PAUSE
* APP_CMD_STOP, (invisible)
* APP_CMD_SAVE_STATE
* APP_CMD_DESTROY

After APP_CMD_PAUSE, the process might be killed by the OS without APP_CMD_DESTROY being received.

Window life cycle (LC), controls the window (app_activity->window) and might happen at any point while the app is visible:

* APP_CMD_INIT_WINDOW
* APP_CMD_TERM_WINDOW

Specifics of exactly when they are received depend on manufacturer, OS version etc.

The graphics resources used are divided into Context and Surface:

* Context
  * EGLDisplay display
  * EGLContext context
  * EGLConfig config
* Surface
  * EGLSurface surface

GLFW functions called by the engine are:

* `_glfwPlatformInit` (Context creation)
* `_glfwPlatformOpenWindow` (Surface creation)
* `_glfwPlatformCloseWindow` (Surface destruction)
* `_glfwPlatformTerminate` (implicit Context destruction)

Some implementation details to note:

* `_glfwPreMain` pumps the LC commands until the window has been created (APP_CMD_INIT_WINDOW) before proceeding to boot the app (engine-main).
  This should be possible to streamline so that content loading can start faster.
* The engine continues to pump the LC commands as a part of polling for input (glfw)
* OpenWindow is the first time when the window dimensions are known, which controls screen orientation.
* The glfw window is considered open (`_glfwWin.opened`) from APP_CMD_INIT_WINDOW until APP_CMD_DESTROY, which is app termination
* The glfw window is considered iconified (`_glfwWin.iconified`) when not visible to user, which stops buffer swapping and controls poll timeouts
* Between CloseWindow and OpenWindow the GL context is temp-stored in memory (ordinary struct is memset to 0 by glfw in CloseWindow)
* When rebooting the engine (when using the dev app), essentially means CloseWindow followed by OpenWindow.
* APP_CMD_TERM_WINDOW might do Context destruction before `_glfwPlatformTerminate`, depending on which happens first
* `_glfwPlatformTerminate` pumps the LC commands until the Context has been destroyed

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
This is done by updating the `defold/packages/android-<android version>-<arch>.tar.gz` etc

Some relevant links:

- https://developer.android.com/guide/topics/manifest/uses-sdk-element.html
- https://developer.android.com/about/versions
- https://developer.android.com/tools/support-library/index.html

### AAPT Binaries

We ship Android "aapt" (Android Asset Packaging Tool) binaries for all platforms that the editor runs on. When upgrading the Android SDK, these needs to be copied from the Android SDK directory into `com.dynamo.cr/com.dynamo.cr.bob/libexec/<PLATFORM>/`. Currently the Linux `aapt` binary is dynamically linked against a version of `libc++.so` that comes with the Android SDK. This forces us to also copy this library into `com.dynamo.cr/com.dynamo.cr.bob/libexec/x86_64-linux/lib/`.

### Creating the android tar ball

Creating a new android package is straight forward:

    APILEVEL=34
    mkdir -p sdkpack_android
    cd sdkpack_android
    mkdir -p share/java
    cp ../tmp/dynamo_home/ext/SDKs/android-sdk/platforms/android-$APILEVEL/android.jar share/java
    ./../scripts/mobile/android_jar_reduce_size.sh share/java/android.jar
    tar -cvzf android-$APILEVEL-armv7-android.tar.gz share
    tar -cvzf android-$APILEVEL-arm64-android.tar.gz share
    cp android-$APILEVEL-armv7-android.tar.gz ../packages
    cp android-$APILEVEL-arm64-android.tar.gz ../packages


### Update build script

Update the reference to the tar ball in `<defold>/scripts/build.py`

    PACKAGES_ANDROID="... android-34 ...".split()

Find and update all `ANDROID_BUILD_TOOLS_VERSION`, `ANDROID_TARGET_API_LEVEL` and `ANDROID_PLATFORM` in the `defold` project folder.

`ANDROID_BUILD_TOOLS_VERSION` maybe found here:
![Screenshot 2022-04-05 at 10 28 15](https://user-images.githubusercontent.com/2209596/161712727-f52f3616-1965-454b-87ef-f1f6bca1c037.jpg)

### Update Dockerfile

Update Android environment variables in [the extender's Dockerfile](https://github.com/defold/extender/blob/2960a454940bc4d2629af721e9dc48c60c4ed432/server/docker-base/Dockerfile#L207).


### Reinstall and rebuild

    $ ./scripts/build.py distclean
    $ ./scripts/build.py install_sdk --platform=arm64-android
    $ ./scripts/build.py install_ext --platform=arm64-android
    $ ./scripts/build.py build_engine --platform=arm64-android --skip-docs --skip-codesign --skip-tests

## Energy Consumption

      adb shell dumpsys cpuinfo
