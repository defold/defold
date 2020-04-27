# Setup Engine

(Setup instructions for the editor [here](/editor/README.md)).

## Required Software

### Java JDK 11

You need Java JDK 11 installed to build the tools. [Download and install from OpenJDK](https://jdk.java.net/archive/). When Java is installed you may also add need to add java to your PATH and export JAVA_HOME:

Set PATH and JAVA_HOME:

    > nano ~/.bashrc

    export JAVA_HOME=<JAVA_INSTALL_PATH>
    export PATH=$JAVA_HOME/bin:$PATH


Verify that Java is installed and working:

    > javac -version


### Python

You need a 64 bit Python 2 version to build the engine and tools. The latest tested on all platforms is Python 2.7.16. You also need `easy_install` to install additional packages.

#### macOS

Catalina comes with both 64-bit Python 2.7.16 and Easy Install installed. For older versions of macOS you may need to install using [Brew](https://brew.sh/):

    > brew install python2

#### Windows

Install Python from the [official download page](https://www.python.org/downloads/windows/). There is an install option to add `C:\Python27` to the PATH environment variable, select it or add the path manually. You also need to add `C:\Python27\Scripts` to the PATH to be able to use Easy Install.

#### Linux

Install Python using:

    > sudo apt-get install python 2.7 python-setuptools

Configure use of Easy Install (if it's not already installed: `which easy_install`):

    > sh -c "echo \#\!/usr/bin/env bash > /usr/local/bin/easy_install"
    > sh -c "echo python /usr/lib/python2.7/dist-packages/easy_install.py $\* >> /usr/local/bin/easy_install"
    > chmod +x /usr/local/bin/easy_install


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

    > sudo apt-get install -y --no-install-recommends gcc-5 g++-5 libssl-dev openssl libtool autoconf automake build-essential uuid-dev libxi-dev libopenal-dev libgl1-mesa-dev libglw1-mesa-dev freeglut3-dev
    > sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 10 && sudo update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-5 20 && sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 10 && sudo update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-5 20 && sudo update-alternatives --install /usr/bin/cc cc /usr/bin/gcc 30 && sudo update-alternatives --set cc /usr/bin/gcc && sudo update-alternatives --install /usr/bin/c++ c++ /usr/bin/g++ 30 && sudo update-alternatives --set c++ /usr/bin/g++

Verify the compiler installation with

    > gcc --version
    gcc (Ubuntu 5.5.0-12ubuntu1) 5.5.0 20171010

### Windows

#### Visual C++ 2019 Community

[Download](https://visualstudio.microsoft.com/vs/older-downloads/) the Community version or use the Professional or Enterprise version if you have the proper licence. When installing, don't forget to select VC++ and the 'Windows 8.1 and windows phone' SDK. There is also an optional 3rd party git client.

#### MSYS/MinGW

This will get you a shell that behaves like Linux and is much easier to build Defold through. [Download](http://www.mingw.org/download/installer) and run the installer and check these packages (binary):

	* MingW Base System: `mingw32-base-bin`, 'mingw32-gcc-g++-bin'
	* MSYS Base System: `msys-base-bin`, `msys-patch-bin`
    * MinGW Developer Toolkit: `mingw-developer-toolkit-bin`

Select the menu option `Installation -> Apply Changes`.

You also need to install `wget`. From the mingw terminal run:

	> mingw-get install msys-wget-bin msys-zip msys-unzip

**NOTE:** You can start the visual installer again by simply running `mingw-get`

#### Git

You need to [download](https://git-scm.com/download/win) a command line version of Git. During install, select the option to not do any CR/LF conversion. If you use ssh (public/private keys) to access GitHub then:

	- Run Git GUI
	- Help > Show SSH Key
	- If you don't have an SSH Key, press Generate Key
	- Add the public key to your Github profile
	- You might need to run `start-ssh-agent` (in `C:\Program Files\Git\cmd`)

Alternatively, you can easily create your own key from command line:

    $ ssh-keygen -t rsa -b 1024 -C "user@domain.com"
    # Copy the contents of the public file
    $ cat ~/.ssh/id_rsa.pub
    # Add the public key to your Github profile
    # Test your new key:
    $ ssh -T git@github.com

Now you should be able to clone the defold repo from a command prompt:

	> git clone git@github.com:defold/defold.git

If this won't work, you can try cloning using Github Desktop.

#### Misc

These tools are not essential, but merely things that might help during your development

##### [Chocolatey](https://chocolatey.org/docs/installation)

Chocolatey is another package installer that will help install various helper tools such as [ripgrep](https://github.com/BurntSushi/ripgrep)


### Command Prompt

It's useful to modify your command prompt to show the status of the repo you're in.
E.g. it makes it easier to keep the git branches apart.

You do this by editing the `PS1` variable. Put it in the recommended config for your system (e.g. `.profile` or `.bashrc`)
Here's a very small improvement on the default prompt, whic shows you the time of the last command, as well as the current git branch name and its status:

    git_branch() {
        git branch 2> /dev/null | sed -e '/^[^*]/d' -e 's/* \(.*\)/(\1)/'
    }
    acolor() {
      [[ -n $(git status --porcelain=v2 2>/dev/null) ]] && echo 31 || echo 33
    }
    export PS1='\t \[\033[32m\]\w\[\033[$(acolor)m\] $(git_branch)\[\033[00m\] $ '



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
