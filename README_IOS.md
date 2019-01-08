# iOS

## Setup

### Install XCode
* Install from AppStore (if not already installed)
* Start XCode
* When asked, enable “Developer mode” on your mac
* In Preferences -> Accounts
    ** Add a new account, use your apple ID
    ** You should now see account and that it’s a member of the Midasplayer team
* You also need to set up a Provisional Profile. This is done in Xcode->Preferences->Accounts. "Team Midasplayer AB" should be visible in the list. Double-click and  a window should appear with IDs in the top half and Provisioning Profiles in the bottom half. Select the profile you want (e.g. "iOS Team Provision Profile: *". Use the downloaded profile when deploying to an iOS device.

After installation of XCode (and each update!) you need to create a symbolic link to iOS sdk:

    $ sudo ln -s /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS10.3.sdk



## iOS Debugging

### Setup XCode project

* Make sure that you build with **--disable-ccache**. Otherwise lldb can't set breakpoints (all pending). The
  reason is currently unknown. The --disable-ccache option is available in waf and in build.py.
* Create a new empty iOS project (Other/Empty)
* Create a new scheme with Project -> New Scheme...
* Select executable (dmengine.app)
* Make sure that debugger is lldb. Otherwise debuginfo is not found for static libraries when compiled with clang for unknown reason

See also: [Attaching to Process](http://stackoverflow.com/questions/9721830/attach-debugger-to-ios-app-after-launch)


### ios-deploy

Good tool for iOS deployment / debugging (lldb): [ios-deploy](https://github.com/phonegap/ios-deploy)

    $ ios-deploy --bundle blossom_blast_saga.ipa

or

	$ ios-deploy --debug --bundle blossom_blast_saga.app


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

### Package iPhone SDK

The easiest way is to pack the folder directly.
However, the extracted folder needs to have the version number, so make sure you
extract it properly!

    $ cd /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs
    $ tar -czvf ~/work/iPhoneOS11.2.sdk.tar.gz iPhoneOS.sdk

#### How to test locally on the engine build:

    $ tar -xvf iPhoneOS11.2.sdk.tar.gz -C $DYNAMO_HOME/ext/SDKs

### Package macOS SDK

    $ cd /Applications/Xcode.app/Contents/Developer/Platforms/MacOSX.platform/Developer/SDKs
    # the -h option didn't work on this package, it got caught in a resolve loop
    $ tar -cvzf ~/work/MacOSX10.13.sdk.tar.gz MacOSX.sdk


### Package toolchain

    $ cd /Applications/Xcode.app/Contents/Developer/Toolchains
    $ tar -cvzf ~/work/XcodeToolchain9.2.tar.gz XcodeDefault.xctoolchain

#### How to test locally on the engine build:

    $ tar -xvf iPhoneOS11.2.sdk.tar.gz -C $DYNAMO_HOME/ext/SDKs
    $ tar -xvf XcodeToolchain9.2.sdk.tar.gz -C $DYNAMO_HOME/ext/SDKs
    $ tar -xvf MacOSX10.12.sdk.tar.gz -C $DYNAMO_HOME/ext/SDKs

### Upload SDKs and toolchain

Upload package to S3:

* Login in to sso.king.com
* Click 'Amazon Web Services'
* Click S3
* Click 'defold-packages'
* Upload package (default settings)

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
      wget -q -O - ${S3_URL}/iPhoneOS11.2.sdk.tar.gz | tar xz -C /opt

### Defold SDK (build.yml)

Also, you should update the list of `allowedLibs` in the `defold/share/extender/build.yml` for both iOS and OSX. The easiest way to do that is to use the `defold/share/extender/find_libs_apple.sh` (after running `./scripts/build.py install_ext` to download the packages to `$DYNAMO_HOME`)
