# iOS

## Setup

### Install Xcode

Follow the [Xcode setup instructions](README_SETUP.md#xcode). CMake prefers the packaged toolchain and iOS SDK under `$DYNAMO_HOME/ext/SDKs` when available and falls back to the selected Xcode installation. After configuring an `arm64-ios` build, run `grep '^DEFOLD_TARGET_SYSROOT:PATH=' engine/build/arm64-ios/CMakeCache.txt` to see which device SDK it uses.

For a physical device, add your Apple Account in Xcode Settings, enable Developer Mode on the device, and install a development provisioning profile that includes it. The [device test instructions](#running-cmake-tests-on-a-connected-device) describe how the build selects a profile and signing identity.

## iOS debugging

Install and launch a development-signed bundle on the connected device. In Xcode choose **Debug → Attach to Process** and select the running app. Use the executable and debug symbols from that build. See the [crash debugging guide](README_DEBUGGING.md#ios-and-macos) to resolve addresses from a crash report.

## Running CMake Tests On A Connected Device

After completing [build setup](README_BUILD.md), install and build dependencies
for each platform you will test (also repeat this after `distclean`):

    $ ./scripts/build.py --platform=arm64-ios install_ext
    $ ./scripts/build.py --platform=arm64_sim-ios install_ext

The top-level CMake `build_engine` flow can build, sign, install and run `arm64-ios`
unit tests on a locally connected iOS device:

    $ ./scripts/build.py --platform=arm64-ios build_engine

The `arm64_sim-ios` platform runs tests through an iOS simulator:

    $ ./scripts/build.py --platform=arm64_sim-ios build_engine

Physical-device tests use `xcrun devicectl` and installed/local signing assets.
The runner does not create or update provisioning profiles and it does not
register devices in Apple Developer. The selected provisioning profile must
already include the connected device. Simulator tests use `xcrun simctl` and do
not require provisioning profiles.

If exactly one connected physical iOS device is available, the runner selects it
automatically. Pass `--test-device <udid-or-name>` only when multiple devices are
available or when you want to force a specific device:

    $ ./scripts/build.py --platform=arm64-ios --test-device <udid-or-name> build_engine

Simulator tests require an installed iOS Simulator runtime and at least one
available iPhone simulator device:

    $ xcodebuild -downloadPlatform iOS
    $ xcrun simctl list devices available
    $ python3 build_tools/build_ios.py list-simulators

You can also install the iOS Simulator runtime from Xcode via Settings ->
Components. After installing the runtime, list the available runtimes and device
types:

    $ xcrun simctl list runtimes available
    $ xcrun simctl list devicetypes
    $ xcrun simctl list devices available

If no iPhone simulator device exists, create one using a device type id and
runtime id from the previous commands:

    $ xcrun simctl create "Defold iPhone" <device-type-id> <runtime-id>

Start a simulator before running tests, or let the runner boot it when exactly
one matching simulator is available:

    $ open -a Simulator
    $ xcrun simctl boot <simulator-udid-or-name>
    $ xcrun simctl bootstatus <simulator-udid-or-name> -b

If exactly one iOS simulator is available, the runner selects and boots it
automatically. If multiple simulators are available, either boot the one you want
first or pass `--test-device <simulator-udid-or-name>`.

By default the runner auto-selects a single matching development identity and
provisioning profile. If more than one asset matches, or if the automatic choice
is not the one you want, pass explicit overrides:

    $ ./scripts/build.py --platform=arm64-ios \
        --ios-identity "Apple Development: Name (TEAMID)" \
        --ios-mobileprovision /path/to/profile.mobileprovision \
        --ios-team-id TEAMID \
        build_engine

The generated test app bundle id is `<prefix>.<test-target>`, using
`com.defold.tests` as the default prefix. Pass `--ios-bundle-id-prefix` only
when your provisioning profile requires a different app id prefix.

The `build_tools/build_ios.py` helper is normally invoked by `build.py`, but it
can also be run directly from the repository root when diagnosing device or
signing setup:

    $ python3 build_tools/build_ios.py --help
    $ python3 build_tools/build_ios.py list-devices
    $ python3 build_tools/build_ios.py list-simulators
    $ python3 build_tools/build_ios.py list-identities
    $ python3 build_tools/build_ios.py list-teams
    $ python3 build_tools/build_ios.py can-run-tests
    $ python3 build_tools/build_ios.py can-run-tests --platform simulator

`list-teams` prints teams found in installed provisioning profiles. Its columns
are team id, team name, profile count, iOS profile count, and unexpired iOS
profile count.

Use `can-run-tests` before a full engine build if you want to verify that
`devicectl`, a connected device, a provisioning profile, and a matching signing
identity are available. The same signing overrides accepted by `build.py` can be
passed to the helper with shorter option names:

    $ python3 build_tools/build_ios.py can-run-tests \
        --identity "Apple Development: Name (TEAMID)" \
        --mobileprovision /path/to/profile.mobileprovision \
        --team-id TEAMID

`run-test` packages and runs one already-built test executable. Most local
testing should use `./scripts/build.py --platform=arm64-ios build_engine`
instead, because it builds the engine tests and generates the per-test
`run_<target>` commands. Add `--test-device <udid-or-name>` only when you need
to override device auto-selection.

Tests that require iOS Local Network privacy for Bonjour/mDNS are not included
in unattended iOS `run_tests`. iOS requires the user to grant that permission
interactively; Developer Mode and development signing do not let the runner
pre-approve it.

If `devicectl` reports that CoreDeviceService could not initialize, first verify
that Xcode can see the device:

    $ xcrun devicectl list devices

Unlock and reconnect the device, quit Xcode, and retry. On slow machines or
after Xcode updates, you can give CoreDevice more time:

    $ IOS_DEVICECTL_TIMEOUT=120 IOS_DEVICECTL_RETRIES=3 \
        python3 build_tools/build_ios.py list-devices

### Inspect a provisioning profile

To inspect a packaged app, list the files in the IPA with `unzip -l Game.ipa`. If it contains a provisioning profile, extract it with `unzip -p Game.ipa 'Payload/Game.app/embedded.mobileprovision' > profile.mobileprovision`, substituting the app’s path from the listing.

On macOS, inspect the profile’s entitlements and, for a development profile, its list of devices with:

```sh
security cms -D -i profile.mobileprovision
```

## Update the iOS and macOS SDKs

1. Select the Xcode release to package. Review [Apple's release notes](https://developer.apple.com/documentation/xcode-release-notes) and update the corresponding Xcode, Clang and SDK versions in [build_tools/sdk.py](build_tools/sdk.py).
2. From the repository root, run `./scripts/package/package_xcode_and_sdks.sh`. It writes the toolchain, macOS, iOS and simulator SDK archives to `local_sdks/`. See the [SDK packaging guide](scripts/package/README.md#ios--macos).
3. Test the packages with `./scripts/build.py install_sdk --platform=arm64-ios --package-path=local_sdks`, then run `install_ext` and `build_engine` for that target. Repeat for `arm64_sim-ios` and the macOS targets you are updating.
4. For native extensions, update the SDK packages on the Extender build server and review the allowed Apple frameworks and libraries in [build_input.yml](share/extender/build_input.yml). The SDK build generates `extender/build.yml` from this template.
