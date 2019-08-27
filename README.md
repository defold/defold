# Defold

Repository for engine, editor and server.

## Code Style

Follow current code style and use 4 spaces for tabs. Never commit code
with trailing white-spaces.

API documentation is generated from source comments. See README_DOCS.md for help on style and
conventions.

## Setup

### Required Software

#### [Java JDK 11](https://www.oracle.com/technetwork/java/javase/downloads/index.html)

##### Linux

See the [General Setup](#general-setup) Section below

Set PATH and JAVA_HOME:

    $> nano ~/.bashrc

    export PATH=<JAVA_INSTALL_DIR>:$PATH
    export JAVA_HOME=<JAVA_INSTALL_DIR>

Verify with:

    $> javac -version

#### Python

* macOS: On macOS you must run the python version shipped with macOS, eg no homebrew installed python versions)

Make sure you have python2 installed by running `which python2`.
If that is not the case symlink python2 to python or python2.7(if installed already), which is enough to build Emscripten.
`ln -s /usr/bin/python2.7 /usr/local/bin/python2`

### General Setup

#### Linux

    >$ sudo apt-get install libxi-dev freeglut3-dev libglu1-mesa-dev libgl1-mesa-dev libxext-dev x11proto-xext-dev mesa-common-dev libxt-dev libx11-dev libcurl4-openssl-dev uuid-dev python-setuptools build-essential libopenal-dev rpm git curl autoconf libtool automake cmake tofrodos valgrind tree silversearcher-ag

###### Easy Install

Since the executable doesn't install anymore, easiest to create a wrapper:

    >$ sudo sh -c "echo \#\!/usr/bin/env bash > /usr/local/bin/easy_install"
    >$ sudo sh -c "echo python /usr/lib/python2.7/dist-packages/easy_install.py $\* >> /usr/local/bin/easy_install"
    >$ sudo chmod +x /usr/local/bin/easy_install

#### Windows

Binaries are available on this shared [drive](https://drive.google.com/drive/folders/0BxFxQdv6jzsec0RPeEpaOHFCZ2M?usp=sharing)

- [Visual C++ 2015 Community](https://www.visualstudio.com/downloads/) - [download](https://drive.google.com/open?id=0BxFxQdv6jzseY3liUDZmd0I3Z1E)

	We only use Visual Studio 2015. Professional/Enterprise version should also work if you have a proper licence. When installing, don't forget to select VC++ and the 'Windows 8.1 and windows phone' SDK. There is also an optional 3rd party git client.

- [Python](https://www.python.org/downloads/windows/) - [download](https://drive.google.com/open?id=0BxFxQdv6jzsedW1iNXFIbGFYLVE)

	Install the 32-bit 2.7.12 version. This is latest one known to work. There is an install option to add `C:\Python27` to the PATH environment variable, select it or add the path manually
During the build of the 32 bit version of Defold, a python script needs to load a shared defold library (texc). This will not work using a 64 bit python.
Building the 64 bit version of Defold begins with building a set of 32 bit libraries.

- [easy_install/ez_setup](https://pypi.python.org/pypi/setuptools#id3) - [download](https://drive.google.com/open?id=0BxFxQdv6jzseaTdqQXpxbl96bTA)

	Download `ez_setup.py` and run it. If `ez_setup.py` fails to connect using https when run, try adding `--insecure` as argument to enable http download. Add `C:\Python27\Scripts` (where `easy_install` should now be located) to PATH.

	- Update setuptools and pip - you might get errors running easy_install when running the install-ext command with build.py otherwise

		python -m pip install --upgrade pip

		pip install setuptools --upgrade

- [MSYS/MinGW](http://www.mingw.org/download/installer) - [download](https://drive.google.com/open?id=0BxFxQdv6jzseZ1hKaGJRZE1pM1U)
	This will get you a shell that behaves like Linux and is much easier to build Defold through.
	Run the installer and check these packages (binary):

	* MingW Base System: `mingw32-base`, `mingw-developer-toolkit`
	* MSYS Base System: `msys-base`, `msys-bash`
	* optional packages `msys-dos2unix`

	Select the menu option `Installation -> Apply Changes`
	You also need to install wget, from a cmd command line run

		mingw-get install msys-wget-bin msys-zip msys-unzip

- [Git](https://git-scm.com/download/win) - [download](https://drive.google.com/a/king.com/file/d/0BxFxQdv6jzseQ0JfX2todndWZmM/view?usp=sharing)

	During install, select the option to not do any CR/LF conversion. If you use ssh (public/private keys) to access github then:
	- Run Git GUI
	- Help > Show SSH Key
	- If you don't have an SSH Key, press Generate Key
	- Add the public key to your Github profile
	- You might need to run start-ssh-agent (in `C:\Program Files\Git\cmd`)

	Now you should be able to clone the defold repo from a cmd prompt:

		git clone git@github.com:defold/defold.git

	If this won't work, you can try cloning using Github Desktop.

#### macOS

    - [Homebrew](http://brew.sh/)
        Install with Terminal: `ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"`

    >$ brew install dos2unix

### Optional Software

Quick and easy install:
* macOS: `$ brew install wget curl p7zip ccache`
* Linux: `$ sudo apt-get install wget curl p7zip ccache`
* Windows: `$ pip install cmake patch`

Explanations:
* **wget** + **curl** - for downloading packages
* **7z** - for extracting packages (archives and binaries)
* [ccache](http://ccache.samba.org) - Configure cache (3.2.3) by running ([source](https://ccache.samba.org/manual.html))

    $ /usr/local/bin/ccache --max-size=5G

* s4d - A set of build scripts for our engine
    - `$ git clone https://github.com/king-dan/s4d.git`
    - Add the s4d directory to the path.
* **cmake** for easier building of external projects
* **patch** for easier patching on windows (when building external projects)

## Build Engine

*In the instructions below, the `--platform` argument to `build.py` will default to the host platform if not specified.*

### Windows

Start msys by running `C:\MinGW\msys\1.0\msys.bat`. You should now have a bash prompt. Verify that:

- `which git` points to the git from the windows installation instructions above
- `which javac` points to the `javac.exe` in the JDK directory
- `which python` points to `/c/Python27/python.exe`

Note that your C: drive is mounted to /c under MinGW/MSYS.

Next, `cd` your way to the directory you cloned Defold into.

    $ cd /c/Users/erik.angelin/Documents/src/defold

Setup the build environment. The `--platform` argument is needed for instance to find the version of `go` relevant for the target platform.

	$ ./scripts/build.py shell --platform=x86_64-win32

This will start a new shell and land you in your msys home (for instance `/usr/home/Erik.Angelin`) so `cd` back to Defold.

    $ cd /c/Users/erik.angelin/Documents/src/defold

Install external packages. This step is required whenever you switch target platform, as different packages are installed.

	$ ./scripts/build.py install_ext --platform=x86_64-win32

Now, you should be able to build the engine.

	$ ./scripts/build.py build_engine --platform=x86_64-win32 --skip-tests -- --skip-build-tests

When the initial build is complete the workflow is to use waf directly.

    $ cd engine/dlib
    $ waf

### OS X/Linux

Setup the build environment. The `--platform` argument is needed for instance to find the version of `go` relevant for the target platform..

    $ ./scripts/build.py shell --platform=...

Install external packages. This step is required whenever you switch target platform, as different packages are installed.

    $ ./scripts/build.py install_ext --platform=...

Build engine for target platform.

    $ ./scripts/build.py build_engine --skip-tests --platform=...

Build at least once with 64 bit support (to support the particle editor, i.e. allowing opening collections).

    $ ./scripts/build.py build_engine --skip-tests --platform=x86_64-darwin

When the initial build is complete the workflow is to use waf directly.

    $ cd engine/dlib
    $ waf

**Unit tests**

Unit tests are run automatically when invoking waf if not --skip-tests is
specified. A typically workflow when working on a single test is to run

    $ waf --skip-tests && ./build/default/.../test_xxx

With the flag `--gtest_filter=` it's possible to a single test in the suite,
see [Running a Subset of the Tests](https://code.google.com/p/googletest/wiki/AdvancedGuide#Running_a_Subset_of_the_Tests)

## Build and Run Editor

See [README.md](editor/README.md) in the editor folder.

## Licenses

* **Sony Vectormath Library**: [https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/bullet/bullet-2.74.tgz](https://storage.googleapis.com/google-code-archive-downloads/v2/code.google.com/bullet/bullet-2.74.tgz) - **BSD**
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
* **lz4** [http://cyan4973.github.io/lz4/](http://cyan4973.github.io/lz4/)  **BSD**
* **box2d** [http://box2d.org](http://box2d.org) **zlib**
* **bullet** [http://bulletphysics.org](http://bulletphysics.org) **zlib**
* **vpx/vp8** [http://www.webmproject.org](http://www.webmproject.org) **BSD**
* **WebP** [http://www.webmproject.org/license/software/](http://www.webmproject.org/license/software/) **BSD**
* **openal** [http://kcat.strangesoft.net/openal.html](http://kcat.strangesoft.net/openal.html) **LGPL**
* **md5** Based on md5 in axTLS

### Unused??
* **alut** [https://github.com/vancegroup/freealut](https://github.com/vancegroup/freealut) was **BSD** but changed to **LGPL**
* **xxtea-c** [https://github.com/xxtea](https://github.com/xxtea) **MIT**


## Tagging

New tag

    # git tag -a MAJOR.MINOR [SHA1]
    SHA1 is optional

Push tags

    # git push origin --tags


## Folder Structure

**ci** - Continious integration related files

**com.dynamo.cr** - _Content repository_. Old Editor, Bob and server

**engine** - Engine

**editor** - Editor

**packages** - External packages

**scripts** - Build and utility scripts

**share** - Misc shared stuff used by other tools. Waf build-scripts, valgrind suppression files, etc.


## Content pipeline

The primary build tool is bob. Bob is used for the editor but also for engine-tests.
In the first build-step a standalone version of bob is built. A legacy pipeline, waf/python and some classes from bob.jar,
is still used for gamesys and for built-in content. This might be changed in the future but integrating bob with waf 1.5.x
is pretty hard as waf 1.5.x is very restrictive where source and built content is located. Built-in content is compiled
, via .arc-files, to header-files, installed to $DYNAMO_HOME, etc In other words tightly integrated with waf.


## Byte order/endian

By convention all graphics resources are expliticly in little-endian and specifically ByteOrder.LITTLE_ENDIAN in Java. Currently we support
only little endian architectures. If this is about to change we would have to byte-swap at run-time or similar.
As run-time editor code and pipeline code often is shared little-endian applies to both. For specific editor-code ByteOrder.nativeOrder() is
the correct order to use.


## Platform Specifics

* [iOS](README_IOS.md)
* [Android](README_ANDROID.md)

## Updating "Build Report" template

The build report template is a single HTML file found under `com.dynamo.cr/com.dynamo.cr.bob/lib/report_template.html`. Third party JS and CSS libraries used (DataTables.js, Jquery, Bootstrap, D3 and Dimple.js) are concatenated into two HTML inline tags and added to this file. If the libraries need to be updated/changed please use the `inline_libraries.py` script found in `share/report_libs/`.

## Emscripten

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

## Firefox OS

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


## Flash

**TODO**

* Investigate mutex and dmProfile overhead. Removing DM_PROFILE, DM_COUNTER, dmMutex::Lock and dmMutex::Unlock from dmMessage::Post resulted in 4x improvement
* Verify that exceptions are disabled

## Asset loading

Assets can be loaded from file-system, from an archive or over http.

See *dmResource::LoadResource* for low-level loading of assets, *dmResource* for general resource loading and *engine.cpp*
for initialization. A current limitation is that we don't have a specific protocol for *resource:* For file-system, archive
and http url schemes *file:*, *arc:* and *http:* are used respectively. See dmConfigFile for the limitation about the absence
of a resource-scheme.

### Http Cache

Assets loaded with dmResource are cached locally. A non-standard batch-oriented cache validation mechanism
used if available in order to speed up the cache-validation process. See dlib, *dmHttpCache* and *ConsistencyPolicy*, for more information.

## Engine Extensions

Script extensions can be created using a simple exensions mechanism. To add a new extension to the engine the only required step is to link with the
extension library and set "exported_symbols" in the wscript, see note below.

*NOTE:* In order to avoid a dead-stripping bug with static libraries on macOS/iOS a constructor symbol must be explicitly exported with "exported_symbols"
in the wscript-target. See extension-test.

## Energy Consumption

### Android

      adb shell dumpsys cpuinfo

## Debugging

### Emscripten

Emscripten have several useful features for debugging, and it's really good to read their article about debugging in full (https://kripken.github.io/emscripten-site/docs/porting/Debugging.html). For general debugging it's good to read up on JavaScript maps which will be generated by emscripten if you compile with `-g4`. JavaScript maps will allow the browser to translate the minified JavaScript into C/C++ file and line information so you can actually place breakpoints and watch variables from the real source code.

To debug memory and alignment issues the following parameters should be added both to `CCFLAGS`/`CXXFLAGS` and `LINKFLAGS` in `waf_dynamo.py` for the web target. It is important to note that this will decrease runtime performance, and significantly increase compile time, therefore it should only be used when debugging these kinds of issues.

- `-g4` should be **added** to build with additional debug symbols.
- `-s ASSERTIONS=1` should be **added** explicitly since they are otherwise turned off by optimizations.
- `-s SAFE_HEAP=1` should be **added** to enable additional memory access checks.
- `-s STACK_OVERFLOW_CHECK=2` should be **added** to enable additional stack checks.
- `-s AGGRESSIVE_VARIABLE_ELIMINATION=1` should be **removed**, otherwise errors might be ignored.
- `-s DISABLE_EXCEPTION_CATCHING=1` should be **removed**, otherwise errors might be ignored.

### Extensions / modules

* For debugging our IAP (in app purchase) extension: [DefoldIAPTester]((***REMOVED***)/1j-2m-YMcAryNO8il1P7m4cjNhrCTzS0QOsQODwvnTww/edit?usp=sharing)

## Porting to another compiler

You will likely need to recompile external libraries. Source code for most is available [here](https://drive.google.com/open?id=0BxFxQdv6jzseeXh2TzBnYnpwdUU).

