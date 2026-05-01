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
