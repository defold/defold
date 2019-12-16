# Setup Engine

(Setup instructions for the editor [here](/editor/README.md)).

## Required Software

### Java JDK 11

You need Java JDK 11 installed to build the tools. [Download and install from Oracle](https://www.oracle.com/technetwork/java/javase/downloads/index.html). When Java is installed you may also add need to add java to your PATH and export JAVA_HOME:

Set PATH and JAVA_HOME:

    > nano ~/.bashrc

    export PATH=<JAVA_INSTALL_DIR>:$PATH
    export JAVA_HOME=<JAVA_INSTALL_DIR>

Verify that Java is installed and working:

    > javac -version


### Python

You need a 64 bit Python 2 version to build the engine and tools. The latest tested on all platforms is Python 2.7.16. You also need `easy_install` to install additional packages.

#### macOS

Catalina comes with both 64-bit Python 2.7.16 and Easy Install installed. For older versions of macOS you may need to install using [Brew](https://brew.sh/):

    > brew install python2

#### Windows

Install Python from the [official download page](https://www.python.org/downloads/windows/). There is an install option to add `C:\Python27` to the PATH environment variable, select it or add the path manually.

Note: During the build of the 32 bit version of Defold, a python script needs to load a shared Defold library (texc). This will not work using a 64 bit python. Building the 64 bit version of Defold begins with building a set of 32 bit libraries.

[Follow the instructions](https://setuptools.readthedocs.io/en/latest/easy_install.html#installing-easy-install) to install Easy Install.

#### Linux

Install Python using:

    > sudo apt-get install python-setuptools

[Follow the instructions](https://setuptools.readthedocs.io/en/latest/easy_install.html#installing-easy-install) to install Easy Install.


## Additional required software

### Linux

You need additional files and tools to be able to build and work with Defold on Linux:

**Development files**
* **libxi-dev** - X11 Input extension library
* **libxext-dev** - X11 Miscellaneous extensions library
* **x11proto-xext-dev** - X11 various extension wire protocol
* **freeglut3-dev** - OpenGL Utility Toolkit development files
* **libglu1-mesa-dev** + libgl1-mesa-dev + mesa-common-dev - Mesa OpenGL development files
* **libcurl4-openssl-dev** - Development files and documentation for libcurl
* **uuid-dev** - Universally Unique ID library
* **libopenal-dev** - Software implementation of the OpenAL audio API

**Tools**
* **build-essential** - Compilers
* **rpm** - package manager for RPM
* **git** - Fast, scalable, distributed revision control system
* **curl** - Command line tool for transferring data with URL syntax
* **autoconf** - Automatic configure script builder
* **libtool** - Generic library support script
* **automake** - Tool for generating GNU Standards-compliant Makefiles
* **cmake** - Cross-platform, open-source make system
* **tofrodos** - Converts DOS <-> Unix text files
* **valgrind** - Instrumentation framework for building dynamic analysis tools

Download and install using `apt-get`:

    > sudo apt-get install libxi-dev freeglut3-dev libglu1-mesa-dev libgl1-mesa-dev libxext-dev x11proto-xext-dev mesa-common-dev libxt-dev libx11-dev libcurl4-openssl-dev uuid-dev libopenal-dev build-essential rpm git curl autoconf libtool automake cmake tofrodos valgrind


### Windows

#### Visual C++ 2015 Community

We only use Visual Studio 2015. [Download](https://visualstudio.microsoft.com/vs/older-downloads/) the Community version or use the Professional or Enterprise version if you have the proper licence. When installing, don't forget to select VC++ and the 'Windows 8.1 and windows phone' SDK. There is also an optional 3rd party git client.

#### MSYS/MinGW

This will get you a shell that behaves like Linux and is much easier to build Defold through. [Download](http://www.mingw.org/download/installer) and run the installer and check these packages (binary):

	* MingW Base System: `mingw32-base`, `mingw-developer-toolkit`
	* MSYS Base System: `msys-base`, `msys-bash`
	* optional packages `msys-dos2unix`

Select the menu option `Installation -> Apply Changes`.

You also need to install `wget`. From the mingw terminal run:

	> mingw-get install msys-wget-bin msys-zip msys-unzip

#### Git

You need to [download](https://git-scm.com/download/win) a command line version of Git. During install, select the option to not do any CR/LF conversion. If you use ssh (public/private keys) to access GitHub then:

	- Run Git GUI
	- Help > Show SSH Key
	- If you don't have an SSH Key, press Generate Key
	- Add the public key to your Github profile
	- You might need to run `start-ssh-agent` (in `C:\Program Files\Git\cmd`)

Now you should be able to clone the defold repo from a command prompt:

	> git clone git@github.com:defold/defold.git

If this won't work, you can try cloning using Github Desktop.


### macOS

You need the `dos2unix` command line tool to convert line endings of certain source files. You can install `dos2unix` using [Brew](https://brew.sh/):

    > brew install dos2unix


## Optional Software

It is recommended but not required that you install the following software:

* **wget** + **curl** - for downloading packages
* **7z** - for extracting packages (archives and binaries)
* **ccache** - for faster compilations of source code
* **cmake** for easier building of external projects
* **patch** for easier patching on windows (when building external projects)

Quick and easy install:

* macOS: `> brew install wget curl p7zip ccache`
* Linux: `> sudo apt-get install wget curl p7zip ccache`
* Windows: `> pip install cmake patch`

Configure `ccache` by running ([source](https://ccache.samba.org/manual.html))

    > /usr/local/bin/ccache --max-size=5G
