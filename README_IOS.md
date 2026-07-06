# iOS

## Setup

### Install XCode
* [Install XCode](./README_BUILD.md#xcode)
* Start XCode
* When asked, enable “Developer mode” on your mac
* In Preferences -> Accounts
    ** Add a new account, use your Apple ID
    ** You should now see the account and that its team membership
* You also need to set up a Provisioning Profile. This is done in Xcode->Preferences->Accounts. Double-click and a window should appear with IDs in the top half and Provisioning Profiles in the bottom half. Select the profile you want (e.g. "`iOS Team Provision Profile: *`"). Use the downloaded profile when deploying to an iOS device.

After installation of XCode (and each update!) you need to create a symbolic link to iOS sdk:

    $ sudo ln -s /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS10.3.sdk



## iOS Debugging

### Setup XCode project

* Make sure that you build with **--disable-ccache**. Otherwise lldb can't set breakpoints (all pending). The reason is currently unknown. The --disable-ccache option is available in waf and in build.py.
* Create a new empty iOS project (Other/Empty)
* Create a new scheme with Product -> Scheme -> New Scheme...
* Select executable (dmengine.app)
* Make sure that debugger is lldb. Otherwise debuginfo is not found for static libraries when compiled with clang for unknown reason

See also: [Attaching to Process](http://stackoverflow.com/questions/9721830/attach-debugger-to-ios-app-after-launch)


### ios-deploy

*NOTE: ios-deploy seems to be outdated, and it is recommended to use xcode command line tools*

Good tool for iOS deployment / debugging (lldb): [ios-deploy](https://github.com/phonegap/ios-deploy)

    $ ios-deploy --bundle test.ipa

or

	$ ios-deploy --debug --bundle test.app

## Running CMake Tests On A Connected Device

The top-level CMake `build_engine` flow can build, sign, install and run `arm64-ios`
unit tests on a locally connected iOS device:

    $ ./scripts/build.py --platform=arm64-ios build_engine

The `x86_64-ios` platform runs tests through an iOS simulator:

    $ ./scripts/build.py --platform=x86_64-ios build_engine

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
available iPhone simulator device. On Apple Silicon hosts, the current
`x86_64-ios` runner expects a universal iOS simulator runtime:

    $ xcodebuild -downloadPlatform iOS -architectureVariant universal
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

### QuickLook plugin for .ipa and .mobileprovision

It's often required to peek inside the permissions of a package or mobile provisioning file.
For this you can install [ProvisionQL](https://github.com/ealeksandrov/ProvisionQL)

        $ brew cask install provisionql

## iOS Crashdumps

From: [http://stackoverflow.com/a/13576028](http://stackoverflow.com/a/13576028)

    symbol address = slide + stack address - load address

* The slide value is the value of vmaddr in LC_SEGMENT cmd (Mostly this is 0x1000). Run the following to get it:

    `$ otool -arch ARCHITECTURE -l "APP_BUNDLE/APP_EXECUTABLE" | grep -B 3 -A 8 -m 2 "__TEXT"`

    Replace ARCHITECTURE with the actual architecture the crash report shows, e.g. armv7. Replace APP_BUNDLE/APP_EXECUTABLE with the path to the actual executable.

* The stack address is the hex value from the crash report.

* The load address can be is the first address showing in the Binary Images section at the very front of the line which contains your executable. (Usually the first entry).


## Update SDK

Both iPhoneOS + macOS SDK's use the same steps to update.

### Check what's been updated

To make sure you know what's been changed, you can check this page: https://developer.apple.com/library/content/releasenotes/General/WhatsNewIniOS/Introduction/Introduction.html#//apple_ref/doc

From XCode 9.+ and onwards, you'll see the added/modified/deprecated items here: https://developer.apple.com/documentation?changes=latest_major

### Download latest stable XCode

    https://developer.apple.com/download/more/

### Package SDK (iPhone, iPhone Simulator, MacOS, Xcode toolchain)

See the script [./scripts/package/package_xcode_and_sdks.sh](./scripts/package/package_xcode_and_sdks.sh)

#### How to test locally on the engine build:

    $ tar -xvf iPhoneOS11.2.sdk.tar.gz -C $DYNAMO_HOME/ext/SDKs
    $ tar -xvf XcodeToolchain9.2.sdk.tar.gz -C $DYNAMO_HOME/ext/SDKs
    $ tar -xvf MacOSX10.12.sdk.tar.gz -C $DYNAMO_HOME/ext/SDKs

### Build.py

Update the sdk version(s).
In ```install_ext```, update the commands if needed.

### waf_dynamo.py

Update the sdk version(s) at the top of the file

### Native Extension

#### Dockerfile

Open ```extender/server/docker-base/Dockerfile```

Make sure you unpack the package with the correct version number!
Here, the package is downloaded and extracted to 'iPhoneOSXxx.sdk',
making sure that the contained library has a version number!

    NOTE: If it doesn't have a version number, it will bug out in subtle ways (E.g. the device orientation events won't fire properly)

    RUN \
      wget -q -O - ${DM_PACKAGES_URL}/iPhoneOS11.2.sdk.tar.gz | tar xz -C /opt

### Defold SDK (build.yml)

Also, you should update the list of `allowedLibs` in the `defold/share/extender/build.yml` for both iOS and OSX. The easiest way to do that is to use the `defold/share/extender/find_libs_apple.sh` (after running `./scripts/build.py install_ext` to download the packages to `$DYNAMO_HOME`)
