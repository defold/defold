# Setup Engine

(Setup instructions for the editor [here](/editor/README.md)).

## Required Software

### Package managers

<details><summary>Windows...</summary><p>

* [Chocolatey](https://chocolatey.org/docs/installation) - Chocolatey is a package installer that will help install various helper tools such as python, ripgrep etc.

Open a Command (cmd.exe) as administator and run:

`@"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -InputFormat None -ExecutionPolicy Bypass -Command "[System.Net.ServicePointManager]::SecurityProtocol = 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"`

Once this is done, you can install new packages which are added to the PATH, by running:

```sh
choco install <package_name>
```
</p></details>

### Required Software - Java JDK 17

Download and install the latest JDK 17 (17.0.5 or later) release from either of these locations:

* [Adoptium/Temurin](https://github.com/adoptium/temurin17-binaries/releases) - The Adoptium Working Group promotes and supports high-quality runtimes and associated technology for use across the Java ecosystem
* [Microsoft OpenJDK builds](https://docs.microsoft.com/en-us/java/openjdk/download#openjdk-17) - The Microsoft Build of OpenJDK is a no-cost distribution of OpenJDK that's open source and available for free for anyone to deploy anywhere


<details><summary>Windows...</summary><p>

Or install using Chocolatey:

```sh
choco install openjdk17
```

*With choco, the install path is something like /c/Program\ Files/OpenJDK/openjdk-17.0.5_8*
</p></details>

<details><summary>Linux...</summary><p>
  
Or install using `apt-get`:

```
> sudo apt-get install openjdk-17-jdk
```

Or install using `dnf`:
```
> sudo dnf install java-17-openjdk-devel
```
</p></details>

When Java is installed you may also add need to add java to your PATH and export JAVA_HOME:

```sh
> nano ~/.bashrc

export JAVA_HOME=<JAVA_INSTALL_PATH>
export PATH=$JAVA_HOME/bin:$PATH
```

Verify that Java is installed and working:

```sh
> javac -version
```


### Required Software - Python 3

You need a 64 bit Python 3 version (x86_64) to build the engine and tools. The latest tested on all platforms is Python 3.10.5.

* Install via https://www.python.org/downloads/release/python-3105/

<details><summary>macOS...</summary><p>
  
Once Python has been installed you also need install certificates (for https requests):

```sh
> /Applications/Python\ 3.10/Install\ Certificates.command
```
</p></details>

<details><summary>Windows...</summary><p>

Or install using Chocolatey:

```sh
choco install python
```
</p></details>

<details><summary>Linux...</summary><p>

On some systems, you may need to configure the `python` command to default to Python 3:
```
> sudo alternatives --install /usr/bin/python python /usr/bin/python3 1
```

You also need `easy_install` to install additional packages.
</p></details>


### Required Software

<details><summary>macOS...</summary><p>

You need the `dos2unix` command line tool to convert line endings of certain source files when building files in `share/ext`. You can install `dos2unix` using [Brew](https://brew.sh/):

```sh
> brew install dos2unix
```
</p></details>

<details><summary>Windows...</summary><p>

#### Visual C++ 2019 Community

[Download](https://visualstudio.microsoft.com/vs/older-downloads/) the Community version or use the Professional or Enterprise version if you have the proper licence. When installing, select the "Desktop Development with C++" workload. There is also an optional 3rd party git client.

#### Terminal

https://aka.ms/terminal

The `git-bash` setup can also install a setup for the Windows Terminal app.

This terminal has the tool `winget` to install some packages.


#### MSYS/MinGW

This will get you a shell that behaves like Linux and is much easier to build Defold through. [Download](https://sourceforge.net/projects/mingw/files/Installer/mingw-get-setup.exe/download) and run the installer and check these packages (binary):

* MingW Base System: `mingw32-base-bin`, 'mingw32-gcc-g++-bin'
* MSYS Base System: `msys-base-bin`, `msys-patch-bin`
* MinGW Developer Toolkit: `mingw-developer-toolkit-bin`

Select the menu option `Installation -> Apply Changes`.

You also need to install `wget`. From the mingw terminal run:

```sh
> mingw-get install msys-wget-bin msys-zip msys-unzip
```

**NOTE:** You can start the visual installer again by simply running `mingw-get`

#### Git

You need to [download](https://git-scm.com/download/win) a command line version of Git.

During install, select the option to not do any CR/LF conversion.

You most likely want to set up working with ssh keys as well.

### SSH Keys

#### Using Git Gui

- Run Git GUI
- Help > Show SSH Key
- If you don't have an SSH Key, press Generate Key
- Add the public key to your Github profile
- You might need to run `start-ssh-agent` (in `C:\Program Files\Git\cmd`)


#### Command line

Alternatively, you can easily create your own key from command line:

```sh
$ ssh-keygen -t ed25519 -C "your_email@example.com"
# Copy the contents of the public file
$ cat ~/.ssh/id_ed25519.pub
# Add the public key to your Github profile (under the Setting tab on your github user profile)
# Test your new key:
$ ssh -T git@github.com
```

Now you should be able to clone the defold repo from a command prompt:

```sh
> git clone git@github.com:defold/defold.git
```

If this won't work, you can try cloning using Github Desktop.
</p></details>

<details><summary>Linux...</summary><p>

#### Additional tools

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
* **libncurses5** -  Needed by clang

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

```sh
> sudo apt-get install -y --no-install-recommends libssl-dev openssl libtool autoconf automake build-essential uuid-dev libxi-dev libopenal-dev libgl1-mesa-dev libglw1-mesa-dev freeglut3-dev libncurses5
```

Download and install using `dnf`:
```
> sudo dnf install python3-pip python3-cffi python3-setuptools openssl-devel openssl libtool autoconf automake gcc gcc-c++ clang glibc-devel make libuuid-devel libXi-devel openal-soft-devel mesa-libGL-devel mesa-libGLw-devel freeglut-devel ncurses-devel ncurses-compat-libs
```
</p></details>

---

## Optional Software

It is recommended but not required that you install the following software:

<details><summary>macOS...</summary><p>

* **wget** + **curl** - for downloading packages
* **7z** - for extracting packages (archives and binaries)
* **ccache** - for faster compilations of source code
* **cmake** for easier building of external projects
* **patch** for easier patching on windows (when building external projects)
* **ripgrep** for faster search

Quick and easy install:

```sh
> brew install wget curl p7zip ccache ripgrep
```

Configure `ccache` by running ([source](https://ccache.samba.org/manual.html))

```sh
> /usr/local/bin/ccache --max-size=5G
```
</p></details>

<details><summary>Windows...</summary><p>

* **wget** + **curl** - for downloading packages
* **7z** - for extracting packages (archives and binaries)
* **ccache** - for faster compilations of source code
* **cmake** for easier building of external projects
* **patch** for easier patching on windows (when building external projects)

Quick and easy install:

```sh
> pip install cmake patch
```

Configure `ccache` by running ([source](https://ccache.samba.org/manual.html))

```sh
> /usr/local/bin/ccache --max-size=5G
```

* [ripgrep](https://github.com/BurntSushi/ripgrep) - A very fast text search program (command line)

Open a Command (cmd.exe) as administrator and run:

`choco install ripgrep`
</p></details>

<details><summary>Linux...</summary><p>

* **wget** + **curl** - for downloading packages
* **7z** - for extracting packages (archives and binaries)
* **ccache** - for faster compilations of source code
* **cmake** for easier building of external projects
* **patch** for easier patching on windows (when building external projects)
* **snapd** for installing snap packages
* **ripgrep** for faster search

Quick and easy install:

```sh
> sudo apt-get install wget curl p7zip ccache
```

Configure `ccache` by running ([source](https://ccache.samba.org/manual.html))

```sh
> ccache --max-size=5G
```

Install snapd package manager:

```sh
> sudo apt install snapd
```

Install ripgrep:

```sh
> sudo snap install ripgrep --classic
```
</p></details>

---

## Optional Setup

### Optional Setup - Command Prompt

It's useful to modify your command prompt to show the status of the repo you're in.
E.g. it makes it easier to keep the git branches apart.

You do this by editing the `PS1` variable. Put it in the recommended config for your system (e.g. `.profile` or `.bashrc`)
Here's a very small improvement on the default prompt, whic shows you the time of the last command, as well as the current git branch name and its status:

```sh
git_branch() {
    git branch 2> /dev/null | sed -e '/^[^*]/d' -e 's/* \(.*\)/(\1)/'
}
acolor() {
  [[ -n $(git status --porcelain=v2 2>/dev/null) ]] && echo 31 || echo 33
}
export PS1='\t \[\033[32m\]\w\[\033[$(acolor)m\] $(git_branch)\[\033[00m\] $ '
```
---

## WSL (Windows Subsystem for Linux)

<details><summary>Linux...</summary><p>

It is possible to build Linux targets using WSL 1.

Install relevant packages (git, java, python, clang etc) using `./scripts/linux/install_wsl_packages.sh`.
If also updates your `~/.bashrc` with updated paths.

### Git clone into a mounted folder

In order to get the proper username of your files, we need to setup WSL for this.
Otherwise the git clone won't work in a mounted C: drive folder.

Open (or create) the config file:
```
sudo nano /etc/wsl.conf
```

Add these lines:
```
[automount]
options = "metadata"
```

And restart your WSL session


### X11

The script also sets the `DISPLAY=localhost:0.0` which allows you to connect to a local X server.

A popular choice is [VCXSRV](https://sourceforge.net/projects/vcxsrv/)
</p></details>
