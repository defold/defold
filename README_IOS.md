# iOS

## Setup

### Install XCode
* Install from AppStore (if not already installed)
* Start XCode
* When asked, enable “Developer mode” on your mac
* In Preferences -> Accounts
    ** Add a new account, use your apple ID
    ** You should now see account and that it’s a member of the Midasplayer team

After installation of XCode (and each update!) you need to create a symbolic link to iOS sdk:

    $ sudo ln -s /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS.sdk /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS8.1.sdk



## iOS Debugging

### Setup XCode project

* Make sure that you build with **--disable-ccache**. Otherwise lldb can't set breakpoints (all pending). The
  reason is currently unknown. The --disable-ccache option is available in waf and in build.py.
* Create a new empty iOS project (Other/Empty)
* Create a new scheme with Project -> New Scheme...
* Select executable (dmengine.app)
* Make sure that debugger is lldb. Otherwise debuginfo is not found for static libraries when compiled with clang for unknown reason

See also: [Attaching to Process](http://stackoverflow.com/questions/9721830/attach-debugger-to-ios-app-after-launch)


## iOS Crashdumps

From: [http://stackoverflow.com/a/13576028](http://stackoverflow.com/a/13576028)

    symbol address = slide + stack address - load address

* The slide value is the value of vmaddr in LC_SEGMENT cmd (Mostly this is 0x1000). Run the following to get it:

    `$ otool -arch ARCHITECTURE -l "APP_BUNDLE/APP_EXECUTABLE" | grep -B 3 -A 8 -m 2 "__TEXT"`

    Replace ARCHITECTURE with the actual architecture the crash report shows, e.g. armv7. Replace APP_BUNDLE/APP_EXECUTABLE with the path to the actual executable.

* The stack address is the hex value from the crash report.

* The load address can be is the first address showing in the Binary Images section at the very front of the line which contains your executable. (Usually the first entry).

## Misc

Good tool for iOS deployment: [ios-deploy](https://github.com/phonegap/ios-deploy)

    $ ios-deploy --bundle blossom_blast_saga.app