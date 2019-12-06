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

You need a 64 bit Python 2 to build the engine and tools.
Latest tested on all platforms is Python 2.7.16.

For macOS, you can install using:

        $ brew install python2

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

See [licenses.md](engine/engine/content/builtins/docs/licenses.md) from `builtins.zip`

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
* [HTML5/Emscripten](README_EMSCRIPTEN.md)

## Updating "Build Report" template

The build report template is a single HTML file found under `com.dynamo.cr/com.dynamo.cr.bob/lib/report_template.html`. Third party JS and CSS libraries used (DataTables.js, Jquery, Bootstrap, D3 and Dimple.js) are concatenated into two HTML inline tags and added to this file. If the libraries need to be updated/changed please use the `inline_libraries.py` script found in `share/report_libs/`.


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

## Porting to another compiler

You will likely need to recompile external libraries.
Check the `share/ext` folder for building the external packages for each platform
(Source code for some old packages is available [here](https://drive.google.com/open?id=0BxFxQdv6jzseeXh2TzBnYnpwdUU).)
