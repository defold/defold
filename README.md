Defold
======

Repository for engine, editor and server.

Code Style
----------

Follow current code style and use 4 spaces for tabs. Never commit code
with trailing white-spaces.

For Eclipse:
* Install [AnyEditTools](http://andrei.gmxhome.de/eclipse.html) for easy Tabs to Spaces support
* Import the code formating xml: Eclipse -> Preferences: C/C++ -> CodeStyle -> Formatter .. Import 'defold/share/codestyle.xml' and set ”Dynamo” as active profile

Setup
-----

**Required Software**

* [Java 8 JDK](http://www.oracle.com/technetwork/java/javase/downloads/index.html)
* [Eclipse SDK 3.8.2](http://archive.eclipse.org/eclipse/downloads/drops/R-3.8.2-201301310800/) (the editor isn't compatible with Eclipse 4.X)
* Python (on OSX you must run the python version shipped with OSX, eg no homebrew installed python versions)

* Linux:

        $ sudo apt-get install libxi-dev freeglut3-dev libglu1-mesa-dev libgl1-mesa-dev libxext-dev x11proto-xext-dev mesa-common-dev libxt-dev libx11-dev libcurl4-openssl-dev uuid-dev python-setuptools build-essential

* Windows:
  - [Visual C++ 2010 Express](http://www.visualstudio.com/downloads/download-visual-studio-vs#DownloadFamilies_4)
  - [MSYS/MinGW](http://www.mingw.org/), will get you a shell that behaves like Linux, and much easier to build defold through.
  - [easy_install]( https://pypi.python.org/pypi/setuptools#id3 )

**Eclipse Plugins**

<table>
<tr>
<th>Plugin</th>
<th>Link</th>
<th>Install</th>
</tr>

<tr>
<td>CDT</td>
<td>http://download.eclipse.org/tools/cdt/releases/juno</td>
<td>`C/C++ Development Tools`</td>
</tr>

<tr>
<td>Google Plugin</td>
<td>https://dl.google.com/eclipse/plugin/4.2</td>
<td>
`Google Plugin for Eclipse`<br/>
`Google App Engine Java`<br/>
`Google Web Toolkit SDK`
</td>
</tr>

<tr>
<td>PyDev</td>
<td>http://pydev.org/updates</td>
<td>`PyDev for Eclipse`</td>
</tr>

<tr>
<td>AnyEditTools (optional)</td>
<td>http://andrei.gmxhome.de/eclipse/ <br> (manual install) <br> http://andrei.gmxhome.de/anyedit/index.html </td>
<td>`AnyEditTools`</td>
</tr>

</table>

Always launch Eclipse from the **command line** with a development environment
setup. See `build.py` and the `shell` command below.


**Optional Software**

* [ccache](http://ccache.samba.org) - install with `brew install ccache` on OS X and `sudo apt-get install ccache`
  on Debian based Linux distributions. Configure cache (3.2.3) by by running ([source](https://ccache.samba.org/manual.html))

    `/usr/local/bin/ccache --max-size=5G`



**Import Java Projects**

* Import Java projects with `File > Import`
* Select `General > Existing Projects into Workspace`,
* Set root directory to `defold/com.dynamo.cr`
* Select everything apart from `com.dynamo.cr.web` and `com.dynamo.cr.webcrawler`.
* Ensure that `Copy projects into workspace` is **not** selected


**Import Engine Project**

* Create a new C/C++ project
* Makefile project
    - `Empty Project`
    - `-- Other Toolchain --`
    - Do **not** select `MacOSX GCC` on OS X. Results in out of memory in Eclipse 3.8
* Use custom project location
     - `defold/engine`
* Add `${DYNAMO_HOME}/include`, `${DYNAMO_HOME}/ext/include` and `/usr/include` to project include.
    - `$DYNAMO_HOME` defaults to `defold/tmp/dynamo_home`
    - `Project Properties > C/C++ General > Paths and Symbols`
* Disable `Invalid arguments` in `Preferences > C/C++ > Code Analysis`

** Troubleshooting **

If eclipse doesn’t get the JDK setup automatically:
* Preferences -> Java -> Installed JRE’s:
* Click Add
* JRE Home: /Library/Java/JavaVirtualMachines/jdk1.8.0_60.jdk/Contents/Home
* JRE Name: 1.8 (Or some other name)
* Finish



Build Engine
------------

Setup build environment with `$PATH` and other environment variables.

    $ ./scripts/build.py shell

Install external packages. This step is required once only.

    $ ./scripts/build.py install_ext

Build engine for host target. For other targets use ``--platform=``

    $ ./scripts/build.py build_engine --skip-tests

Build at least once with 64 bit support (to support the particle editor, i.e. allowing opening collections)

    $ ./scripts/build.py build_engine --skip-tests —-platform=x86_64-darwin

When the initial build is complete the workflow is to use waf directly. For
example

    $ cd engine/dlib
    $ waf

**Unit tests**

Unit tests are run automatically when invoking waf if not --skip-tests is
specified. A typically workflow when working on a single test is to run

    $ waf --skip-tests && ./build/default/.../test_xxx

With the flag `--gtest_filter=` it's possible to a single test in the suite,
see [Running a Subset of the Tests](https://code.google.com/p/googletest/wiki/AdvancedGuide#Running_a_Subset_of_the_Tests)

Build and Run Editor
--------------------

* In the workspace, invoke `Project > Build All`
     - Generates Google Protocol Buffers etc
* Refresh entire workspace
* In `om.dynamo.cr.editor-product`, double click on template/cr.product
* Press `Launch an Eclipse application`
* Speed up launch
    - Go to `Preferences > Run/Debug`
    - Deselect `Build` in `General Options`
    - This disables building of custom build steps and explicit invocation of `Project > Build All` is now required.

Note: When running the editor and building a Defold project you must first go to Preferences->Defold->Custom Application and point it to a dmengine built for your OS.

**Notes for building the editor under Linux:**
* Install JDK8 (from Oracle) and make sure Eclipse is using it (`Preferences > Java > Installed JREs`).
* Install [libssl0.9.8](https://packages.debian.org/squeeze/i386/libssl0.9.8/download), the Git version bundled with the editor is currently linked against libcrypto.so.0.9.8.
* Make sure that the [protobuf-compiler](http://www.rpmseek.com/rpm-dl/protobuf-compiler_2.3.0-2_i386.html) version used is 2.3, latest (2.4) does not work.
* `.deb` files can be installed by running:

        $ sudo dpkg -i <filename>.deb

        # If dpkg complains about dependencies, run this directly afterwards:
        $ sudo apt-get install -f

### Troubleshooting
#### If you run the editor and get the following error while launching:
```
1) Error injecting constructor, java.net.SocketException: Can't assign requested address
  at com.dynamo.upnp.SSDP.<init>(SSDP.java:62)
  while locating com.dynamo.upnp.SSDP
  at com.dynamo.cr.target.core.TargetPlugin$Module.configure(TargetPlugin.java:42)
  while locating com.dynamo.upnp.ISSDP
    for parameter 0 at com.dynamo.cr.target.core.TargetService.<init>(TargetService.java:95)
  while locating com.dynamo.cr.target.core.TargetService
  at com.dynamo.cr.target.core.TargetPlugin$Module.configure(TargetPlugin.java:40)
  while locating com.dynamo.cr.target.core.ITargetService

1 error
    at com.google.inject.internal.InjectorImpl$4.get(InjectorImpl.java:987)
    at com.google.inject.internal.InjectorImpl.getInstance(InjectorImpl.java:1013)
    at com.dynamo.cr.target.core.TargetPlugin.start(TargetPlugin.java:54)
    at org.eclipse.osgi.framework.internal.core.BundleContextImpl$1.run(BundleContextImpl.java:711)
    at java.security.AccessController.doPrivileged(Native Method)
    at org.eclipse.osgi.framework.internal.core.BundleContextImpl.startActivator(BundleContextImpl.java:702)
    ... 65 more
Caused by: java.net.SocketException: Can't assign requested address
    at java.net.PlainDatagramSocketImpl.join(Native Method)
    at java.net.AbstractPlainDatagramSocketImpl.join(AbstractPlainDatagramSocketImpl.java:179)
    at java.net.MulticastSocket.joinGroup(MulticastSocket.java:323)
```

And the editor starts with:
```
Plug-in com.dynamo.cr.target was unable to load class com.dynamo.cr.target.TargetContributionFactory.
An error occurred while automatically activating bundle com.dynamo.cr.target (23).
```

Then add the following to the VM args in your Run Configuration:
```
-Djava.net.preferIPv4Stack=true
```

#### When running `test_cr` on OS X and you get errors like:
```
...
com.dynamo.cr/com.dynamo.cr.bob/src/com/dynamo/bob/util/MathUtil.java:[27]
[ERROR] return b.setX((float)p.getX()).setY((float)p.getY()).setZ((float)p.getZ()).build();
[ERROR] ^^^^
[ERROR] The method getX() is undefined for the type Point3d
...
```

This means that the wrong `vecmath.jar` library is used and you probably have a copy located in `/System/Library/Java/Extensions` or `/System/Library/Java/Extensions`. Move `vecmath.jar` somewhere else while running `test_cr`.

#### When opening a .collection in the editor you get this ####
```
org.osgi.framework.BundleException: Exception in com.dynamo.cr.parted.ParticleEditorPlugin.start() of bundle com.dynamo.cr.parted.
at org.eclipse.osgi.framework.internal.core.BundleContextImpl.startActivator(BundleContextImpl.java:734)
at org.eclipse.osgi.framework.internal.core.BundleContextImpl.start(BundleContextImpl.java:683)
at org.eclipse.osgi.framework.internal.core.BundleHost.startWorker(BundleHost.java:381)
at org.eclipse.osgi.framework.internal.core.AbstractBundle.start(AbstractBundle.java:300)
at org.eclipse.osgi.framework.util.SecureAction.start(SecureAction.java:440)
```

If you get this error message, it’s most likely from not having the 64 bit binaries, did you build the engine with 64 bit support? E.g. “--platform=x86_64-darwin”
To fix, rebuild engine in 64 bit, and in Eclipse, do a clean projects, refresh and rebuild them again

Licenses
--------

* **Sony Vectormath Library**: [http://bullet.svn.sourceforge.net/viewvc/bullet/trunk/Extras/vectormathlibrary](http://bullet.svn.sourceforge.net/viewvc/bullet/trunk/Extras/vectormathlibrary) - **BSD**
* **json**: Based on [https://bitbucket.org/zserge/jsmn/src](https://bitbucket.org/zserge/jsmn/src) - **MIT**
* **zlib**: [http://www.zlib.net](http://www.zlib.net) - **zlib**
* **axTLS**: [http://axtls.sourceforge.net](http://axtls.sourceforge.net) - **BSD**
* **stb_image** [http://nothings.org/](http://nothings.org) **Public domain**
* **stb_vorbis** [http://nothings.org/](http://nothings.org) **Public domain**
* **tremolo** [http://wss.co.uk/pinknoise/tremolo/](http://wss.co.uk/pinknoise/tremolo/) **BSD**
* **facebook** [https://github.com/facebook/facebook-ios-sdk](https://github.com/facebook/facebook-ios-sdk) **Apache**
* **glfw** [http://www.glfw.org](http://www.glfw.org) **zlib/libpng**
* **lua** [http://www.lua.org](http://www.lua.org) **MIT**
* **luasocket** [http://w3.impa.br/~diego/software/luasocket/](http://w3.impa.br/~diego/software/luasocket/) **MIT**
* **box2d** [http://box2d.org](http://box2d.org) **zlib**
* **bullet** [http://bulletphysics.org](http://bulletphysics.org) **zlib**
* **vp8** [http://www.webmproject.org](http://www.webmproject.org) **BSD**
* **openal** [http://kcat.strangesoft.net/openal.html](http://kcat.strangesoft.net/openal.html) **LGPL**
* **alut** [https://github.com/vancegroup/freealut](https://github.com/vancegroup/freealut) was **BSD** but changed to **LGPL**
* **md5** Based on md5 in axTLS
* **xxtea-c** [https://github.com/xxtea](https://github.com/xxtea) **MIT**


Tagging
-------

New tag

    # git tag -a MAJOR.MINOR [SHA1]
    SHA1 is optional

Push tags

    # git push origin --tags


Folder Structure
----------------

**ci** - Continious integration related files

**com.dynamo.cr** - _Content repository_. Editor and server

**engine** - Engine

**packages** - External packages

**scripts** - Build and utility scripts

**share** - Misc shared stuff used by other tools. Waf build-scripts, valgrind suppression files, etc.

Content pipeline
----------------

The primary build tool is bob. Bob is used for the editor but also for engine-tests.
In the first build-step a standalone version of bob is built. A legacy pipeline, waf/python and some classes from bob.jar,
is still used for gamesys and for built-in content. This might be changed in the future but integrating bob with waf 1.5.x
is pretty hard as waf 1.5.x is very restrictive where source and built content is located. Built-in content is compiled
, via .arc-files, to header-files, installed to $DYNAMO_HOME, etc In other words tightly integrated with waf.

Byte order/endian
-----------------

By convention all graphics resources are expliticly in little-ending and specifically ByteOrder.LITTLE_ENDIAN in Java. Currently we support
only little endian architectures. If this is about to change we would have to byte-swap at run-time or similar.
As run-time editor code and pipeline code often is shared little-endian applies to both. For specific editor-code ByteOrder.nativeOrder() is
the correct order to use.


iOS Debugging
-------------

* Make sure that you build with **--disable-ccache**. Otherwise lldb can't set breakpoints (all pending). The
  reason is currently unknown. The --disable-ccache option is available in waf and in build.py.
* Create a new empty iOS project (Other/Empty)
* Create a new scheme with Project>New Scheme...
* Select executable (dmengine.app)
* Make sure that debugger is lldb. Otherwise debuginfo is not found for static libraries when compiled with clang for unknown reason

iOS Crashdumps
--------------

From: [http://stackoverflow.com/a/13576028](http://stackoverflow.com/a/13576028)

    symbol address = slide + stack address - load address

* The slide value is the value of vmaddr in LC_SEGMENT cmd (Mostly this is 0x1000). Run the following to get it:

      otool -arch ARCHITECTURE -l "APP_BUNDLE/APP_EXECUTABLE" | grep -B 3 -A 8 -m 2 "__TEXT"

  Replace ARCHITECTURE with the actual architecture the crash report shows, e.g. armv7. Replace APP_BUNDLE/APP_EXECUTABLE with the path to the actual executable.

* The stack address is the hex value from the crash report.

* The load address can be is the first address showing in the Binary Images section at the very front of the line which contains your executable. (Usually the first entry).



Android
-------

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

    # go install defold/...

This will result in the production of apkc under “go/bin”. This should be copied to “com.dynamo.cr/com.dynamo.cr.target/lib/<host platform>”.

Copy classes.dex from $DYNAMO_HOME/share/java to com.dynamo.cr/com.dynamo.cr.target/lib.

Copy all content from $DYNAMO_HOME/ext/share/java/res to com.dynamo.cr/com.dynamo.cr.target/res. You should expect to be copying material for
Facebook and Google Play into this location.

### Android SDK/NDK


* Download SDK Tools 23.0 from here: [http://developer.android.com/sdk/index.html](http://developer.android.com/sdk/index.html).
  Drill down to *VIEW ALL DOWNLOADS AND SIZES* and *SDK Tools Only*. Change URL to ...23.0.. if necessary.
* Launch android tool and install Android SDK Platform-tools 20 and Build-tools 20.0
* Download NDK 10b: [http://developer.android.com/tools/sdk/ndk/index.html](http://developer.android.com/tools/sdk/ndk/index.html).
  The revision you look for might be outdated and the path syntax to the link tends to change.
  You can find older versions, for example Max OS X 64, r10b, by the following convention:
    [http://dl.google.com/android/ndk/android-ndk32-r10b-darwin-x86_64.tar.bz2](http://dl.google.com/android/ndk/android-ndk32-r10b-darwin-x86_64.tar.bz2).
* Put NDK/SDK in ~/android/android-ndk-r10b and ~/android/android-sdk respectively

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

OpenGL and jogl
---------------

Prior to GLCanvas#setCurrent the GLDrawableFactory must be created on OSX. This might be a bug but the following code works:

        GLDrawableFactory factory = GLDrawableFactory.getFactory(GLProfile.getGL2ES1());
        this.canvas.setCurrent();
        this.context = factory.createExternalGLContext();

Typically the getFactory and createExternalGLContext are in the same statement. The exception thrown is "Error: current Context (CGL) null, no Context (NS)" and might be related to loading of shared libraries that seems to triggered when the factory is
created. Key is probably that GLCanvas.setCurrnet fails to set current context before the factory is created. The details
are unknown though.

Emscripten
----------

**TODO**

* Run all tests
* In particular (LuaTableTest, Table01) and setjmp
* Profiler (disable http-server)
* Non-release (disable engine-service)
* Verify that exceptions are disabled
* Alignments. Alignment to natural boundary is required for emscripten. uint16_t to 2, uint32_t to 4, etc
  However, unaligned loads/stores of floats seems to be valid though.
* Create a node.js package with uvrun for all platforms (osx, linux and windows)

### Create SDK Packages

* Download [emsdk_portable](http://kripken.github.io/emscripten-site/docs/getting_started/downloads.html)
* Compile on 32-bit Linux
* Run `emsdk update` and `emsdk install`
* On Linux first remove the following directories
  - `emsdk_portable/clang/fastcomp/src`
  - Everything **apart** from `emsdk_portable/clang/fastcomp/build_master_32/bin`
  - Strip debug information from files in `emsdk_portable/clang/fastcomp/build_master_32/bin`
* Create a tarball of the package
* Upload packages to s3-bucket `defold-packages`

In order to run on 64-bit Ubuntu install the following packages `lib32z1 lib32ncurses5 lib32bz2-1.0 lib32stdc++6`

### Installation

To install the emscripten tools, invoke 'build.py install_ems'.

Emscripten creates a configuration file in your home directory (~/.emscripten).Should you wish to change branches to one
in which a different version of these tools is used then call 'build.py activate_ems' after doing so. This will cause the .emscripten file to be updated.

Emscripten also relies upon python2 being on your path. You may find that this is not the case (which python2), it should be sufficient to create a symbolic link to
the python binary in order to solve this problem.

waf_dynamo contains changes relating to emscripten. The simplest way to collect these changes is to run 'build_ext'

    > scripts/build.py install_ext

Building for js-web requires installation of the emscripten tools. This is a slow process, so not included int install_ext, instead run install_ems:

    > scripts/build.py install_ems

As of 1.22.0, the emscripten tools emit separate *.js.mem memory initialisation files by default, rather than embedding this data directly into files.
This is more efficient than storing this data as text within the javascript files, however it does add to a new set of files to include in the build process.
Should you wish to manually update the contents of the editor's engine files (com.dynamo.cr.engine/engine/js-web) then remember to include these items in those
that you copy. Build scripts have been updated to move these items to the expected location under *DYNAMO HOME* (bin/js-web), as has the copy_dmengine.sh script.

### Running Headless Builds

In order to run headless builds of the engine, take the following steps:

* Ensure that you have installed the [xhr2 node module](https://www.npmjs.org/package/xhr2)
* Select or create a folder in which to run your test
* Copy dmengine_headless.js, dmengine_headless.js.mem, game.darc and game.projectc into your folder
* Run dmengine_headless.js with node.js

Since game.darc and game.projectc are not platform specific, you may copy these from any project bundle built with the same engine version that you wish to
test against.

When running headless builds, you may also find it useful to install [node-inspector](https://github.com/node-inspector/node-inspector). Note that it operates on
port 8080 by default, so either close your Defold tools or change this port when running such builds.

To get working keyboard support (until our own glfw is used or glfw is gone):
- In ~/local/emscripten/src/library\_glfw.js, on row after glfwLoadTextureImage2D: ..., add:
glfwShowKeyboard: function(show) {},

To use network functionality during development (or until cross origin support is added to QA servers):
- google-chrome --disable-web-security
- firefox requires a http proxy which adds the CORS headers to the web server response, and also a modification in the engine is required.

Setting up Corsproxy with defold:
To install and run the corsproxy on your network interface of choice for example 172.16.11.23:
```sh
sudo npm install -g corsproxy
corsproxy 172.16.11.23
```

Then, the engine needs a patch to change all XHR:s:
- remove the line engine/script/src/script_http_js.cpp:
```
xhr.open(Pointer_stringify(method), Pointer_stringify(url), true);
```
- and add
```
var str_url = Pointer_stringify(url);
str_url = str_url.replace("http://", "http://172.16.11.23:9292/");
str_url = str_url.replace("https://", "http://172.16.11.23:9292/");
xhr.open(Pointer_stringify(method), str_url, true);
```

For faster builds, change in scripts/build.py -O3 to -O1 in CCFLAGS, CXXFLAGS and LINKFLAGS
To profile in the browser, add -g2 to CCFLAGS, CXXFLAGS and LINKFLAGS. This will cause function names and whitespaces to remain in the js file but also increases the size of the file.

Some flags that is useful for emscripten projects would be to have:
-s ERROR_ON_UNDEFINED_SYMBOLS=1
'-fno-rtti'. Can't be used at the moment as gtest requires it, but it would be nice to have enabled

Firefox OS
----------
To bundle up a firefox OS app and deploy to a connected firefox OS phone, we need to have a manifest.webapp in the web root directory:
```
{
  "launch_path": "/Keyword.html",
  "orientation": ["portrait-primary"],
  "fullscreen": "true",
  "icons": {
    "128": "/keyword_120.png"
  },
  "name": "Keyword"
}
```
Then use ffdb.py to package, deploy to Firefox OS phone, start on the phone and then tail the log on the phone:
```
$EMSCRIPTEN/tools/ffdb.py install . --run --log
```

Tip is to also use the Firefox App Manager. (in Firefox, press and release Alt) -> Tools -> Web Developer -> App Manager. Click device, then "Connect to localhost:6000" at the bottom of the screen. if it fails, manually run:
```
adb forward tcp:6000 localfilesystem:/data/local/debugger-socket
```
In that web app manager, you can see the console output, take screenshots or show other web developer options for the phone.

Flash
-----

**TODO**

* Investigate mutex and dmProfile overhead. Removing DM_PROFILE, DM_COUNTER_HASH, dmMutex::Lock and dmMutex::Unlock from dmMessage::Post resulted in 4x improvement
* Verify that exceptions are disabled


Asset loading
-------------

Assets can be loaded from file-system, from an archive or over http.

See *dmResource::LoadResource* for low-level loading of assets, *dmResource* for general resource loading and *engine.cpp*
for initialization. A current limitation is that we don't have a specific protocol for *resource:* For file-system, archive
and http url schemes *file:*, *arc:* and *http:* are used respectively. See dmConfigFile for the limitation about the absence
of a resource-scheme.

### Http Cache

Assets loaded with dmResource are cached locally. A non-standard batch-oriented cache validation mechanism
used if available in order to speed up the cache-validation process. See dlib, *dmHttpCache* and *ConsistencyPolicy*, for more information.

Engine Extensions
-----------------

Script extensions can be created using a simple exensions mechanism. To add a new extension to the engine the only required step is to link with the
extension library and set "exported_symbols" in the wscript, see note below.

*NOTE:* In order to avoid a dead-stripping bug with static libraries on OSX/iOS a constructor symbol must be explicitly exported with "exported_symbols"
in the wscript-target. See extension-test.

### Facebook Extension

How to package a new Android Facebook SDK:

* Download the SDK
* Replicate a structure based on previous FB SDK package (rooted at share/java within the package)
* From within the SDK:
  * copy bin/facebooksdk.jar into share/java/facebooksdk.jar
  * copy res/* into share/java/res/facebook
* tar/gzip the new structure

Energy Consumption
------------------


**Android**

      adb shell dumpsys cpuinfo

Eclipse 4.4 issues
------------------

* ApplicationWorkbenchWindowAdvisor#createWindowContents isn't invoked
* Shortcuts doens't work in pfx-editor
* No editor tips
* Splash-monitor invoked after startup. See SplashHandler.java.
  Currently protected by if (splashShell == null) ...
