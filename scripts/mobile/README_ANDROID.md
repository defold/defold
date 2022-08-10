# Repack scripts - Android

## ASan

The script `android-repack-asan.sh` doesn't actually replace the executable in the package.
What it does it detect if the executable is dependent on the `libclang_rt.asan-*-android.so` library.
If so, it will copy the file to the correct library path in the package.

Also, it will then copy the `android-wrap.asan.sh` as the `wrap.sh` in the package.

In order to run this package via the `wrap.sh`, you will need to have set the project to "debuggable".
In game.project, set the property `android.debuggable` to `1`.

Note: The wrap.sh support was added in Android O (API 27).

[Documentation - wrap.sh](https://developer.android.com/ndk/guides/wrap-script#creating_the_wrap_shell_script)
[Documentation - Asan](https://developer.android.com/ndk/guides/asan)

UBSan is currently not supported by Android.

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
