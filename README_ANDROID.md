# Android

## Installation

### Android SDK/NDK

* Download SDK Tools 23.0 (or later) from here: [http://developer.android.com/sdk/index.html](http://developer.android.com/sdk/index.html)
* Download NDK 10b: [http://developer.android.com/tools/sdk/ndk/index.html](http://developer.android.com/tools/sdk/ndk/index.html)
* Put NDK/SDK in **~/android/android-ndk-r10b** and **~/android/android-sdk** respectively

Installer+GUI installation of those tools are a bit tricky, so it's recommended doing it via command line
Here are some commands to help out with the process:

    mkdir ~/android
    cd ~/android

    # Supported: darwin,linux,windows
    PLATFORM=darwin
    wget http://dl.google.com/android/ndk/android-ndk32-r10b-$PLATFORM-x86.tar.bz2
    tar xvf android-ndk32-r10b-$PLATFORM-x86.tar.bz2

    wget http://dl.google.com/android/android-sdk_r23.0.2-$PLATFORM.tgz
    tar xvf android-sdk_r23.0.2-$PLATFORM.tgz
    mv android-sdk-$PLATFORM android-sdk

    # You can list all the packages contained (not just the latest versions)
    ./android-sdk/tools/android list sdk --all

    # you can use some aliases to install them
    ./android-sdk/tools/android update sdk -a -u -t tools,platform-tools,build-tools-20.0.0

**Note** Newer version have the suffixes ".bin" or ".exe" as they are now installers.
Simply use that as the suffix, and and extract with **7z**

* How to launch the [Android Tool](http://developer.android.com/sdk/installing/adding-packages.html) manually


## General

By convention we currently have a weak reference to struct android\_app \* called g\_AndroidApp.
g\_AndroidApp is set by glfw and used by dlib. This is more or less a circular dependency. See sys.cpp and android_init.c.
Life-cycle support should probably be moved to dlib at some point.

### Android Resources and R.java

Long story short. Static resources on Android are referred by an integer identifier. These identifiers are generated to a file R.java.
The id:s generated are conceptually a serial number and with no guarantees about uniqueness. Due to this limitations **all** identifiers
must be generated when the final application is built. As a consequence all resources must be available and it's not possible to package
library resources in a jar. Moreover, one identical *R.java* must be generated for every package/library linked with the final application.

This is a known limitation on Android.

**NOTE:** Never ever package compiled **R*.class-files** with third party libraries as it doesn't work in general.

**NOTE2:** android_native_app_glue.c from the NDK has been modified to fix a back+virtual keyboard bug in OS 4.1 and 4.2, the modified version is in the glfw source.

### Android Bundling with Local Builds

With the above in mind, since it may be desirable to create Android bundles using locally build versions of the editor, we will describe how
to manually set up content under com.dynamo.cr. Note that this information has been derived from build.py and related scripts, since running
those has the undesirable side effect of uploading content.

Create apkc, by invoking the following from the root defold directory:

    $ go install defold/...

This will result in the production of apkc under “go/bin”.

    $ PLATFORM=x86-darwin
    $ cp go/bin/apkc com.dynamo.cr/com.dynamo.cr.target/libexec/$PLATFORM

Copy classes.dex

    $ cp $DYNAMO_HOME/share/java/classes.dex com.dynamo.cr/com.dynamo.cr.bob/lib/


Copy all content from $DYNAMO_HOME/ext/share/java/res to com.dynamo.cr/com.dynamo.cr.target/res. You should expect to be copying material for
Facebook and Google Play into this location:

    $ mkdir com.dynamo.cr/com.dynamo.cr.target/res
    $ cp -r $DYNAMO_HOME/ext/share/java/res/* com.dynamo.cr/com.dynamo.cr.target/res

And finally

    $ cp $DYNAMO_HOME/ext/share/java/android.jar com.dynamo.cr/com.dynamo.cr.target/lib/


### Android testing

Copy executable (or directory) with

    # adb push <DIR_OR_DIR> /data/local/tmp

When copying directories append directory name to destination path. It's otherwise skipped

Run exec with:

    # adb shell /data/local/tmp/....

For interactive shell run "adb shell"


### Caveats

If the app is started programatically, the life cycle behaves differently. Deactivating the app and then activating it by clicking on it results in a new
create message being sent (onCreate/android_main). The normal case is for the app to continue through e.g. onStart.

### Android debugging

* Go to application bundle-dir in build/default/...,  e.g. build/default/examples/simple_gles2.android
* Install and launch application
* Run ndk-gdb from android ndk
* Debug

### Life-cycle and GLFW

NDK uses a separate thread which runs the game, separate from the Android UI thread.

The main life cycle (LC) of an android app is controlled by the following events, received on the game thread:

* _glfwPreMain(struct* android_app), corresponds to create
* APP_CMD_START, (visible)
* APP_CMD_RESUME
* APP_CMD_GAINED_FOCUS
* APP_CMD_LOST_FOCUS
* APP_CMD_PAUSE
* APP_CMD_STOP, (invisible)
* APP_CMD_SAVE_STATE
* APP_CMD_DESTROY

After APP_CMD_PAUSE, the process might be killed by the OS without APP_CMD_DESTROY being received.

Window life cycle (LC), controls the window (app_activity->window) and might happen at any point while the app is visible:

* APP_CMD_INIT_WINDOW
* APP_CMD_TERM_WINDOW

Specifics of exactly when they are received depend on manufacturer, OS version etc.

The graphics resources used are divided into Context and Surface:

* Context
  * EGLDisplay display
  * EGLContext context
  * EGLConfig config
* Surface
  * EGLSurface surface

GLFW functions called by the engine are:

* _glfwPlatformInit (Context creation)
* _glfwPlatformOpenWindow (Surface creation)
* _glfwPlatformCloseWindow (Surface destruction)
* _glfwPlatformTerminate (implicit Context destruction)

Some implementation details to note:

* _glfwPreMain pumps the LC commands until the window has been created (APP_CMD_INIT_WINDOW) before proceeding to boot the app (engine-main).
  This should be possible to streamline so that content loading can start faster.
* The engine continues to pump the LC commands as a part of polling for input (glfw)
* OpenWindow is the first time when the window dimensions are known, which controls screen orientation.
* The glfw window is considered open (_glfwWin.opened) from APP_CMD_INIT_WINDOW until APP_CMD_DESTROY, which is app termination
* The glfw window is considered iconified (_glfwWin.iconified) when not visible to user, which stops buffer swapping and controls poll timeouts
* Between CloseWindow and OpenWindow the GL context is temp-stored in memory (ordinary struct is memset to 0 by glfw in CloseWindow)
* When rebooting the engine (when using the dev app), essentially means CloseWindow followed by OpenWindow.
* APP_CMD_TERM_WINDOW might do Context destruction before _glfwPlatformTerminate, depending on which happens first
* _glfwPlatformTerminate pumps the LC commands until the Context has been destroyed

### Pulling APKs from device

E.g. when an APK produces a crash, backing it up is always a good idea before you attempt to fix it.

## Determine package name:
  adb shell pm list packages
## Get the path on device:
  adb shell pm path <package-name>
## Pull the APK to local disk
  adb pull <package-path>

