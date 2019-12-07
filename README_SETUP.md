# Setup

## Required Software

### [Java JDK 11](https://www.oracle.com/technetwork/java/javase/downloads/index.html)

#### Linux

See the [General Setup](#general-setup) Section below

Set PATH and JAVA_HOME:

    $> nano ~/.bashrc

    export PATH=<JAVA_INSTALL_DIR>:$PATH
    export JAVA_HOME=<JAVA_INSTALL_DIR>

Verify with:

    $> javac -version

### Python

You need a 64 bit Python 2 to build the engine and tools.
Latest tested on all platforms is Python 2.7.16.

For macOS, you can install using:

        $ brew install python2

## General Setup

### Linux

    >$ sudo apt-get install libxi-dev freeglut3-dev libglu1-mesa-dev libgl1-mesa-dev libxext-dev x11proto-xext-dev mesa-common-dev libxt-dev libx11-dev libcurl4-openssl-dev uuid-dev python-setuptools build-essential libopenal-dev rpm git curl autoconf libtool automake cmake tofrodos valgrind tree silversearcher-ag

##### Easy Install

Since the executable doesn't install anymore, easiest to create a wrapper:

    >$ sudo sh -c "echo \#\!/usr/bin/env bash > /usr/local/bin/easy_install"
    >$ sudo sh -c "echo python /usr/lib/python2.7/dist-packages/easy_install.py $\* >> /usr/local/bin/easy_install"
    >$ sudo chmod +x /usr/local/bin/easy_install

### Windows

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

### macOS

    - [Homebrew](http://brew.sh/)
        Install with Terminal: `ruby -e "$(curl -fsSL https://raw.githubusercontent.com/Homebrew/install/master/install)"`

    >$ brew install dos2unix

## Optional Software

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
