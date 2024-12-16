# Setup Engine

(Setup instructions for the editor [here](/editor/README.md)).

## Required Software

### Platform SDK

To make contributions easier, we detect local installations of the common platform SDK's:

* macOS + iOS: [XCode](https://apps.apple.com/us/app/xcode/id497799835)
* Linux: [Clang++](https://apt.llvm.org/)
* Windows: [Visual Studio Community 2022](https://visualstudio.microsoft.com/vs/older-downloads/)
  * We also require Clang:
    * In Visual Studio Installer, under Individual components, select *C++ Clang Compiler for Windows* and *MSBuild support for LLVM (clang-cl) toolset*.
    * Add clang to your PATH. For a default installation, the path to add will likely be C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\bin
* Android: [Android Studio](https://developer.android.com/studio)

For consoles, we refer to each vendor's installation notes.

### Package managers

<details><summary>Windows...</summary><p>

* [Chocolatey](https://chocolatey.org/docs/installation) - Chocolatey is a package installer that will help install various helper tools such as python, ripgrep etc.

Open a Command (cmd.exe) as administator and run:

```sh
@"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -InputFormat None -ExecutionPolicy Bypass -Command "[System.Net.ServicePointManager]::SecurityProtocol = 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"
```

Once this is done, you can install new packages which are added to the PATH, by running:

```sh
choco install <package_name>
```
</p></details>

### Required Software - Java JDK 21

Download and install the latest JDK 21 (21.0.5 or later) release from either of these locations:

* [Adoptium/Temurin](https://github.com/adoptium/temurin21-binaries/releases) - The Adoptium Working Group promotes and supports high-quality runtimes and associated technology for use across the Java ecosystem
* [Microsoft OpenJDK builds](https://docs.microsoft.com/en-us/java/openjdk/download#openjdk-21) - The Microsoft Build of OpenJDK is a no-cost distribution of OpenJDK that's open source and available for free for anyone to deploy anywhere


<details><summary>Windows...</summary><p>

Or install using Chocolatey:

```sh
choco install temurin21
```

*With choco, the install path is something like /c/Program\ Files/OpenJDK/openjdk-21.0.5_11*
</p></details>

<details><summary>Linux...</summary><p>
  
Or install from apt-get:

```
> sudo apt install openjdk-21-jdk
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
  
You also need `easy_install` to install additional packages.
</p></details>



### Required Software - DotNet 9

(optional)

*NOTE* The DotNet 9 preview currently only supports macOS platform

In order to build and test the csharp languange bindings locally, you need to install DotNet.

<details><summary>Install...</summary><p>

There are a few ways to install the DotNet sdk:

* Install via https://dotnet.microsoft.com/en-us/download/dotnet/9.0
* Install via your package manager
  * macOS: `brew install dotnet-sdk@preview`
  * Windows: `choco install dotnet --pre`

* Install via [dotnet-install.sh](https://learn.microsoft.com/en-us/dotnet/core/tools/dotnet-install-script):

Bash:
```sh
> ./dotnet-install.sh --channel 9.0 --quality preview
```

PowerShell (Windows):
```sh
> ./dotnet-install.ps1 -Channel 9.0 -Quality preview
```
</p></details>

### Required Software

<details><summary>macOS...</summary><p>

#### XCode

we use [XCode](https://apps.apple.com/us/app/xcode/id497799835) for building and debugging macOS + iOS targets.

</p></details>

<details><summary>Windows...</summary><p>

#### Visual C++ 2022 Community

[Download](https://visualstudio.microsoft.com/vs/older-downloads/) the Community version or use the Professional or Enterprise version if you have the proper licence. When installing, select the "Desktop Development with C++" workload. There is also an optional 3rd party git client.
Also, make sure to install the "clang++" compiler as well.

#### Terminal

https://aka.ms/terminal

The `git-bash` setup can also install a setup for the Windows Terminal app.

This terminal has the tool `winget` to install some packages.

#### Git Bash

This installs both Git as well as a Terminal addon to allow you to use a *nix like bash terminal,
making it much easier to build Defold through.

Download: [Git For Windows](https://gitforwindows.org)

#### Git

This is not needed if you've installed `git-bash`

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
</p></details>

---

## Optional Software

It is recommended but not required that you install the following software:

<details><summary>macOS...</summary><p>

* **wget** + **curl** - for downloading packages (used for downloading packages in different scripts)
* **7z** - for extracting packages (archives and binaries)
* **ccache** - for faster compilations of source code (optional)
* **cmake** for easier building of external projects
* **patch** for easier patching on windows (when building external projects)
* **ripgrep** for faster search
* **dos2unix** tool to convert line endings of certain source files (e.g. when building files in `share/ext`)

Quick and easy install:

```sh
> brew install wget curl p7zip ccache ripgrep dos2unix
```

Configure `ccache` by running ([source](https://ccache.samba.org/manual.html))

```sh
> /usr/local/bin/ccache --max-size=5G
```
</p></details>

<details><summary>Windows...</summary><p>

* **wget** + **curl** - for downloading packages (used for downloading packages in different scripts)
* **7z** - for extracting packages (archives and binaries)
* **ccache** - for faster compilations of source code (optional)
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

* **wget** + **curl** - for downloading packages (used for downloading packages in different scripts)
* **7z** - for extracting packages (archives and binaries)
* **ccache** - for faster compilations of source code (optional)
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
