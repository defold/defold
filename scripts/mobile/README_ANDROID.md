# Repack scripts - Android

## HWASan (Android 14+, ARM64)

Use HWAddressSanitizer (HWASan) for native memory debugging with Defold 1.13.2
or newer on an ARM64 device running Android 14 or newer.
[Android's ASan support is deprecated](https://developer.android.com/ndk/guides/asan).

### Build the engine

Set up the engine dependencies as described in [README_BUILD.md](../../README_BUILD.md).
Use Android NDK r27 or newer, Android SDK Build Tools 36.1.0, and a JDK providing
`keytool`.

From the repository root, enter the build shell and select your installed SDKs:

```sh
./scripts/build.py shell
export ANDROID_NDK_ROOT=/absolute/path/to/android-ndk
export ANDROID_SDK_ROOT=/absolute/path/to/android-sdk

cmake -S . -B tmp/hwasan/cmake -G Ninja \
    -DTARGET_PLATFORM=arm64-android \
    -DANDROID_NDK="$ANDROID_NDK_ROOT" \
    -DWITH_HWASAN=ON \
    -DBUILD_TESTS=OFF \
    -DDEFOLD_BUILD_HOME="$PWD/tmp/hwasan"
cmake --build tmp/hwasan/cmake --target dmengine
```

The resulting engine is
`tmp/hwasan/engine/engine/build/arm64-android/libdmengine.so`.
This instruments engine C/C++ sources. Prebuilt third-party libraries are not
instrumented.

### Build with native extensions and an app manifest

Create `game.appmanifest`, or merge these settings into your existing app manifest:

```yaml
platforms:
    arm64-android:
        context:
            flags: ['-fsanitize=hwaddress', '-fno-omit-frame-pointer']
            linkFlags: ['-fsanitize=hwaddress', '-shared-libsan', '-nostdlib++']
            excludeLibs: ['c++_static']
            dynamicLibs: ['c++_shared']
```

Select it in `game.project`:

```ini
[native_extension]
app_manifest = /game.appmanifest
```

Build and bundle for `arm64-android` only, using the debug variant.

This instruments native extension source code. Prebuilt engine and third-party
libraries are not instrumented by the app manifest.

### Bundle and repack the APK

Use a project and engine from the same Defold version. In `game.project`, set:

```ini
[android]
debuggable = 1
extract_native_libs = 1
```

Bundle an APK containing only `arm64-android` (Bob's
`--architectures=arm64-android`). With a custom Android manifest, also remove any
`android:useAppZygote="true"` attribute.

For a project without native extensions, replace the bundled engine and prepare
the APK:

```sh
python3 scripts/mobile/android-repack-hwasan.py /absolute/path/to/game.apk \
    --engine "$PWD/tmp/hwasan/engine/engine/build/arm64-android/libdmengine.so"
```

For an APK built with the app manifest above, omit `--engine`
to keep the custom engine and its extensions:

```sh
python3 scripts/mobile/android-repack-hwasan.py /absolute/path/to/game.apk
```

This writes `game.hwasan.apk` beside the source, signed with a temporary debug key.
Use the same NDK version for building and repacking. For native extension builds,
match the NDK used by the build server. `--ndk` and `--sdk` can specify the NDK and
Android SDK paths instead of the environment variables above.

To use an existing signing key, pass the keystore and password-file paths after
the APK path. Temporary keys change on each run; reuse the same signing key with
`adb install -r` to retain app data. `--reinstall` uninstalls the existing package,
deleting its app data, then installs the result.

```sh
adb install /absolute/path/to/game.hwasan.apk
adb logcat
```

Launch the app normally. Memory errors appear as HWAddressSanitizer reports in
logcat. Keep the matching unstripped engine for symbolication.
See the [Android HWASan guide](https://developer.android.com/ndk/guides/hwasan)
for more about interpreting reports.

## ASan (legacy)

The script `android-repack-asan.sh` doesn't actually replace the executable in the package.
What it does it detect if the executable is dependent on the `libclang_rt.asan-*-android.so` library.
If so, it will copy the file to the correct library path in the package.

Also, it will then copy the `android-wrap-asan.sh` as the `wrap.sh` in the package.

In order to run this package via the `wrap.sh`, you will need to have set the project to "debuggable".
In game.project, set the property `android.debuggable` to `1`.

Note: The wrap.sh support was added in Android O (API 27).

[Documentation - wrap.sh](https://developer.android.com/ndk/guides/wrap-script#creating_the_wrap_shell_script)
[Documentation - Asan](https://developer.android.com/ndk/guides/asan)

### C++ and ASAN

You need to set some compiler+linker flags. We suggest you add a `game.appmanifest` file, and set that in the `game.project` setting `native_extension.app_manifest`:

```
platforms:
    armv7-android:
        context:
            defines: ['SANITIZE_ADDRESS']
            flags: ['-fsanitize=address', '-fno-omit-frame-pointer', '-fsanitize-address-use-after-scope']
            linkFlags: ['-fsanitize=address', '-fno-omit-frame-pointer', '-fsanitize-address-use-after-scope']

    arm64-android:
        context:
            defines: ['SANITIZE_ADDRESS']
            flags: ['-fsanitize=address', '-fno-omit-frame-pointer', '-fsanitize-address-use-after-scope']
            linkFlags: ['-fsanitize=address', '-fno-omit-frame-pointer', '-fsanitize-address-use-after-scope']
```
