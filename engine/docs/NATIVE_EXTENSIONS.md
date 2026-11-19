# Native Extensions

## Brief

Native extensions allows you to create plugins to the engine, using the Lua interface, native code and also
functionality Defold provides via the Defold SDK.


## Defold SDK

The defold sdk contains public headers and libraries from the engine.
The internal structure of the sdk's are matching the structure in the **DYNAMO_HOME** folder.

Extender downloads the combined SDK for a build from:

    https://d.defold.com/archive/<channel>/<commit>/engine/defoldsdk.zip

### Build SDK

Public CI packages each platform SDK during `archive_engine` and passes it to the
`build-sdk` job as a GitHub Actions artifact. That job verifies the platform SDK
checksums, merges the archives, and uploads the combined SDK, checksum, and platform
SDK mappings to S3. The temporary platform artifacts are deleted after successful
publication. Failed builds retain them for seven days to allow retries, as configured in the
[main CI workflow](../../.github/workflows/main-ci.yml). After cleanup
or expiry, rerun the engine jobs as well to regenerate the inputs.

`--sdk-artifacts-dir=<directory>` selects this flow: `archive_engine` stages
`<directory>/<platform>/defoldsdk.zip` and `defoldsdk.sha256`, and `build_sdk`
reads those files instead of downloading platform archives. Without this option,
standalone and private-platform builds continue to archive individual SDKs to S3
and download them when building the combined SDK.

You can build each platform individually:

    defold$ ./scripts/build.py install_ext build_engine build_platform_sdk --platform=x86_64-macos

### build.yml

The SDK's `extender/build.yml` controls the compiler settings used by the build server. Edit [build_input.yml](../../share/extender/build_input.yml), then regenerate and install the merged configuration:

    $ cmake --build engine/build/arm64-macos --target extender
    $ cmake --install share/extender/build/arm64-macos

Run these commands from the repository root, replacing `arm64-macos` with your configured platform.

### Local build server

Build the engine for the target platform in the Defold build shell. The shell sets `DYNAMO_HOME` to the local SDK, normally `tmp/dynamo_home` under the repository root. Use the [Extender repository](https://github.com/defold/extender) for server setup and the [Extender debugging guide](https://github.com/defold/extender/blob/dev/README_DEBUGGING.md) for troubleshooting.

## Building an extension in the editor

Place an `ext.manifest` at the root of the extension directory. Put native source files in `src/`, public headers in `include/` and prebuilt libraries in `lib/<platform>/`. The [native extension manual](https://defold.com/manuals/extensions/) describes the layout and supported files.

Build the project with **Project → Build** (Cmd+B or Ctrl+B). The editor detects native extensions automatically and shows compiler errors in the console. To target a local Extender instance, set **Preferences → Extensions → Build Server** to its URL.

When launching a custom desktop engine, the editor extracts it under the project's `build/<extender-platform>/` directory. This is the executable to use when attaching a native debugger. See [engine.clj](../../editor/src/clj/editor/engine.clj) for how the editor installs it.
