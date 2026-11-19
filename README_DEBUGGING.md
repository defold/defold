# Debugging crashes

## Identify the engine build

A Defold crash dump records the engine version and full commit SHA in `crash.SYSFIELD_ENGINE_VERSION` and `crash.SYSFIELD_ENGINE_HASH`. In a running game, `sys.get_engine_info()` returns them as `version` and `version_sha1`. Use these values, the platform and the engine variant to locate matching symbols. For a custom engine, keep the symbols from the native extension build. See the [crash API implementation](engine/crash/src/script_crash.cpp) and [engine info API](engine/script/src/script_sys.cpp).

If you only have the native executable or shared library, inspect its embedded strings for candidate versions and commit hashes. If several hashes appear, match the prefix in the engine startup log.

```sh
strings -a /path/to/engine | grep -E '^([0-9]+\.[0-9]+\.[0-9]+|[[:xdigit:]]{40})$'
```

## Retrieve an Android crash dump

For a debuggable app, substitute its package name in this command:

```sh
adb exec-out run-as com.defold.adtest cat files/_crash > _crash
```

You can also inspect the app's files with Android Studio's [Device Explorer](https://developer.android.com/studio/debug/device-file-explorer).

## Android symbols

Bundle with Bob's `--with-symbols` option. For a stock engine, the default paths to the unstripped library are `build/<platform>/libdmengine_release.so` for release and `build/<platform>/libdmengine.so` for debug. For a custom engine built with native extensions, use the matching `<name>.apk.symbols/lib/<abi>/lib<name>.so` from the bundle output.

On macOS, this command resolves a module-relative `pc` from an ARM64 tombstone for a stock release build.

```sh
"$ANDROID_NDK_ROOT/toolchains/llvm/prebuilt/darwin-x86_64/bin/llvm-addr2line" \
    -C -f -e build/arm64-android/libdmengine_release.so 0x00257224
```

For a standard archived engine, the unstripped file is `https://d.defold.com/archive/<channel>/<sha1>/engine/<platform>/libdmengine.so`. For the release variant use `libdmengine_release.so`. To resolve an entire tombstone or logcat crash trace, see the [NDK's ndk-stack guide](https://developer.android.com/ndk/guides/ndk-stack).

## iOS and macOS

For a standard archived engine, download `https://d.defold.com/archive/<channel>/<sha1>/engine/<platform>/dmengine.dSYM.zip` if available. Use `dmengine_release.dSYM.zip` for the release variant. For native extensions, request symbols with `--with-symbols` and use the dSYM produced by that build.

Check the UUID and architecture against the Binary Images entry in the crash report. Pass the image's load address and the frame's full instruction address to `atos`. For example, for an arm64 app named `Game`:

```sh
dwarfdump --uuid Game.app/Game
dwarfdump --uuid Game.dSYM/Contents/Resources/DWARF/Game
atos -arch arm64 -o Game.dSYM/Contents/Resources/DWARF/Game \
    -l 0x100000000 0x1001492c4
```

On macOS the app executable is under `Game.app/Contents/MacOS/`. See [Apple's crash report symbolication guide](https://developer.apple.com/documentation/xcode/adding-identifiable-symbol-names-to-a-crash-report).

## HTML5

The web engine build emits a `.js.symbols` file that maps WebAssembly function indices to names. For a standard debug-variant engine, the archive path is `https://d.defold.com/archive/<channel>/<sha1>/engine/wasm-web/dmengine.js.symbols`. Use `wasm_pthread-web` for the threaded build and `dmengine_release.js.symbols` for the release variant. For source-level debugging, see the [Emscripten guide](README_EMSCRIPTEN.md#running-and-debugging).
