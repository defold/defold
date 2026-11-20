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

The Android backend sets `g_AndroidApp` when it enters `dmNativePreMain`. Engine code can access the native app through `dmAndroid::GetAndroidApp()`. See the [Android backend](engine/platform/src/native/android/android_init.c) and the [dlib Android API](engine/dlib/src/dmsdk/dlib/android.h).

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

#### CPU/GPU system traces

Debug engines advertise `/profiler/perfetto` in `/openapi.json`. This
coordinates full Perfetto system traces with APA/ADB using Android's built-in
`trigger_perfetto` command. No GFXReconstruct library, Vulkan capture layer, or
additional APK is required. Android permits the host's ADB shell to start the
full system recorder; the app can signal a named trigger in an armed recording.

1. POST `/profiler/perfetto` with `{"duration_ms": 5000}` to prepare a capture
   plan. The response includes `capture_id`, `config_url`, `config_filename`,
   `record_command`, and the planned output filename on your computer.
2. Download the returned `config_url` and save it to `config_filename` on your
   computer. Preparing a new plan makes the previous URL return HTTP 409.
3. Run `record_command` on your computer to arm Perfetto. On Windows, use
   Command Prompt (`cmd.exe`) for the command's input redirection. Leave the
   command running and use another terminal or asynchronous host process for
   the next step.
4. POST `/profiler/perfetto` with
   `{"action": "trigger", "capture_id": "<returned capture_id>"}`.
   If the host recorder is still running but its output file stays empty,
   retry this POST with the same `capture_id` once the helper leaves
   `trigger_pending`. Stop retrying when trace data starts arriving.
   Preparing another plan changes the trigger name and cannot recover the
   existing recorder. Repeated signals do not restart an active recording.
5. Wait for `record_command` to finish successfully and check that its output
   file contains data. Open the `.perfetto-trace` file in Android Studio, APA, or
   [Perfetto](https://ui.perfetto.dev).

GET reports the current plan and signal-delivery state: `idle`, `prepared`,
`trigger_pending`, `trigger_sent`, or `failed`. `trigger_sent` does not confirm
that a matching recording was armed or that the file is complete. The host
owns recording, stopping, and completion. An armed recorder waits up to 120
seconds for its trigger and then records for the requested duration. Preparing
another plan does not stop an existing recording.

The config collects CPU scheduling/frequency, process metadata, Android graphics
events, and GPU frequency/memory/render-stage/frame-timeline data where the
device and driver expose them. On Android 13 and later, Perfetto polls CPU
frequency at capture start and every 250 ms. Configure additional GPU hardware
counters in APA when needed. APA can also use the returned trigger name in a
custom Perfetto config. These are system performance traces, not graphics API replays.

The returned command streams the trace directly to a uniquely named file in
the host's current directory, so no trace file or storage permission is needed
in the app or on the device. It uses `adb shell -T` to preserve binary output
and keep recorder diagnostics on stderr. A shell pipe supports older Android
permissions, and `pipefail` preserves recorder failures. Perfetto drains its
buffers to stdout every second during recording, so long captures are not
limited to the last bufferful of events. Add `-s <serial>` after `adb` when
multiple devices are connected.

See the [Perfetto recording guide](https://perfetto.dev/docs/learning-more/android)
and [trigger configuration](https://perfetto.dev/docs/concepts/config#triggers).

### Android activity and window lifecycle

The [Android platform backend](engine/platform/src/native/android/) handles the activity and native window lifecycle. Android delivers activity events such as `APP_CMD_RESUME` and `APP_CMD_PAUSE` separately from `APP_CMD_INIT_WINDOW` and `APP_CMD_TERM_WINDOW`. A resumed activity needs a renderable window before the engine can draw.

[android_init.c](engine/platform/src/native/android/android_init.c) handles the app commands and sets the iconified state. It checks the EGL surface for OpenGL and the native window for a backend that manages its own surface. The engine polls events as part of its input update.

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
