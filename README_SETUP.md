# Engine setup

(Setup instructions for the editor [here](/editor/README.md)).

This guide focuses on the tool and platform sdk installation.

Build instructions for the engine are found here [here](/editor/README_BUILD.md)).

At the very minimum, you need to install the SDK for your host platform (Windows, macOS or Linux)

## Supported hosts + targets

Here is a table of our supported target platforms, and on which host platform you can build it:

|host↓  target→ | macOS | Linux | Windows | Android | iOS     | Hmtl5   | PS4/PS5 | Switch | XBox  |
|--------------|-------|-------|---------|---------|---------|---------|---------|--------|-------|
|macOS         | ✅    |       |         | ✅      | ✅      | ✅     |         |        |       |
|Linux         |       | ✅    |         | ✅      |         | ✅      |         |        |       |
|Windows (x64) |       |       | ✅      | ✅      |         |         | ✅      | ✅     | ✅    |


## Required Software

### Platform SDK

Our build system detects local installations of the platform SDK's.

*See more detailed [installation details](#required-software---platform-sdks) further down in this document.*

* macOS + iOS: [XCode](https://apps.apple.com/us/app/xcode/id497799835)
* Linux: [Clang++](https://apt.llvm.org/) (v17)
* Windows: [Visual Studio Community 2022](https://visualstudio.microsoft.com/vs/older-downloads/) + Clang
* Android: [Android Studio](https://developer.android.com/studio)
* HTML5: [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html) - Set `EMSDK` environment variable to locate the local installation.
* Consoles: We detect the appropriate environment variables for each platform. We refer to each vendor's installation notes.

### Command prompt

Throughout our build instructions, we refer to commands that should be run in a shell.
While we try to be as vanilla as possible, on Windows we only use `git-bash`. See instructions below.

* macOS - Terminal is a builtin tool in macOS
* Linux - Terminal is a builtin tool in Linux

<details><summary>Windows...</summary><p>

* Windows - [Terminal](https://aka.ms/terminal) is a builtin tool in Windows.
  * It allows you to run different types of shells, like `git-bash`
  * *note*: We do not build from the regular Command prompt
  * [Git For Windows](https://gitforwindows.org) - Installs `Git` (required), and also `git-bash`.
    * `git-bash` is currently our recommended shell for windows. ´git-bash´ can install as a Terminal add on.
      * During install, if asked, select the option to not do any CR/LF conversion.


</p></details>

#### Optional Setup - Command Prompt

<details><summary>Modified command cursor (macos/Linux)...</summary><p>

  It's useful to modify your command prompt to show the status of the repo you're in.
  E.g. it makes it easier to keep the git branches apart.

  You do this by editing the `PS1` variable in your shell. Put it in the recommended config for your system (e.g. `.zprofile` or `.bashrc`)
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

</p></details>


### Package managers

These are not strictly required, but helps install some of the software you need.

<details><summary>Windows...</summary><p>

* [Chocolatey](https://chocolatey.org/docs/installation) - Chocolatey is a package installer that will help install various helper tools such as python, ripgrep etc.

Open a Command Prompt (cmd.exe) as administator and run:

```sh
@"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -InputFormat None -ExecutionPolicy Bypass -Command "[System.Net.ServicePointManager]::SecurityProtocol = 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"
```

Once this is done, you can install new packages which are added to the PATH, by running:

```sh
choco install <package_name>
```

</p></details>

<details><summary>macOS...</summary><p>

* [Homebrew](https://brew.sh) - Homebrew is a package mananger for macOS.

Once it is installed, you can install packages using

```sh
brew install <package_name>
```

</p></details>

<details><summary>Linux...</summary><p>

* `apt` - (alternatively name `apt-get`) is a package mananger for Linux. It is the default package manager and is installed by default.

Once it is installed, you can install packages using

```sh
apt install <package_name>
```


</p></details>


### Required Software - Java JDK 25

You can use a package manager or do a manual download and installation.

<details><summary>Windows...</summary><p>

Install [temurin jdk](https://adoptium.net/en-GB/installation/) using Chocolatey:

```sh
choco install temurin25
```

*With choco, the install path is something like /c/Program\ Files/OpenJDK/openjdk-25_36*

Once the Java SDK is installed you need to add java to PATH environment variable.

</p></details>

<details><summary>Linux...</summary><p>
  
Install [temurin jdk](https://adoptium.net/en-GB/installation/) using `apt-get`:

```
> sudo apt-get install temurin-25-jdk
```

Once the Java SDK is installed you may need to add `<java install path>/Contents/Home/bin` to your PATH:

```sh
> nano ~/.bashrc
export PATH=<JAVA_INSTALL_PATH>/Contents/Home/bin:$PATH
```

After that, update the path by reloading the PATH variable:
```sh
source ~/.bashrc
```
</p></details>

<details><summary>macOS...</summary><p>

Install [temurin jdk](https://adoptium.net/en-GB/installation/) using `brew`:

```
> brew install --cask temurin@25
```

*after installation, it's installed under /Library/Java/JavaVirtualMachines/temurin-<version>.jdk/Contents/Home`*

Once the Java SDK is installed you may need to add `<java install path>/Contents/Home/bin` to your PATH:

```sh
> nano ~/.zprofile
export PATH=<JAVA_INSTALL_PATH>/Contents/Home/bin:$PATH
```

After that, update the path by reloading the PATH variable:
```sh
source ~/.zprofile
```
</p></details>

<details><summary>Manual download...</summary><p>

Download and install the latest JDK 25 (25+36 or later) release from either of these locations:

* [Adoptium/Temurin](https://github.com/adoptium/temurin25-binaries/releases) - The Adoptium Working Group promotes and supports high-quality runtimes and associated technology for use across the Java ecosystem

</p></details>

Finally, verify that Java is installed and working:
```sh
> javac -version
```

### Required Software - Python 3

You need a 64 bit [Python 3](https://www.python.org/downloads/) version to build the engine and tools. The latest tested on all platforms is Python 3.10+.

<details><summary>macOS...</summary><p>

You need to install a Python3 version from [Python downloads](https://www.python.org/downloads/).

Once Python has been installed you also need install certificates (for https requests):

```sh
> "/Applications/Python\ 3.12/Install\ Certificates.command"
```
</p></details>

<details><summary>Windows...</summary><p>

Install Python using Chocolatey:

```sh
choco install python
```
</p></details>

<details><summary>Linux...</summary><p>

Linux comes with a preinstalled version of Python.

</p></details>

<details><summary>Manual installation...</summary><p>

Install an appropriate version from [https://www.python.org/downloads/](https://www.python.org/downloads/)

</p></details>

Finally, verify that Python 3 is installed and working:
```sh
> python -V
```

### Required Software - Git

Git is needed to work with our code base. It is used to push and pull code changes between the code repositories.

<details><summary>Windows...</summary><p>

This is not needed if you've installed `git-bash`

You can [download](https://git-scm.com/download/win) a command line version of Git.

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

</p></details>

<details><summary>Linux...</summary><p>

You can install `git` using apt-get:

```sh
apt-get install git
```

</p></details>

<details><summary>macOS...</summary><p>

Git is installed with XCode, but you can also install `git` using brew:

```sh
brew install git
```

</p></details>

### Required Software - Platform SDK's

Each platform requires SDK's and other tools (e.g. debuggers)

<details><summary>macOS/iOS...</summary><p>

#### XCode

We use [XCode](https://apps.apple.com/us/app/xcode/id497799835) for building and debugging macOS + iOS targets.

While `XCode` is available from the App Store, we recommend logging into your Apple Developer account and downloading it manually from [More Downloads](https://developer.apple.com/download/all/).

After downloading XCode, you also need to install the `Command line tools`:
```sh
xcode-select --install
```

Once installed, verify the installation with
```sh
defold$ ./scripts/build.py check_sdk --verbose
```

</p></details>

<details><summary>Windows...</summary><p>

#### Visual C++ 2022 Community

[Download](https://visualstudio.microsoft.com/vs/older-downloads/) the Community version or use the Professional or Enterprise version if you have the proper licence.

* When installing, select the "Desktop Development with C++" workload

* We also require Clang:
  * In Visual Studio Installer, under Individual components, select *C++ Clang Compiler for Windows* and *MSBuild support for LLVM (clang-cl) toolset*.

  * Add clang to your PATH. For a default installation, the path to add will likely be C:\Program Files\Microsoft Visual Studio\2022\Community\VC\Tools\Llvm\bin

* There is also an optional 3rd party git client.

Once installed, verify the installation with
```sh
defold$ ./scripts/build.py check_sdk --verbose
```

</p></details>

<details><summary>Linux...</summary><p>

  There is no single "sdk" to install, but a list of libraries that are needed for development.

  **Development files**
  * **libxi-dev** - X11 Input extension library
  * **libxext-dev** - X11 Miscellaneous extensions library
  * **freeglut3-dev** - OpenGL Utility Toolkit development files
  * **libcurl4-openssl-dev** - Development files and documentation for libcurl
  * **uuid-dev** - Universally Unique ID library
  * **libopenal-dev** - Software implementation of the OpenAL audio API
  * **libncurses5** -  Needed by clang

  **Tools**
  * **build-essential** - Compilers
  * **autoconf** - Automatic configure script builder
  * **automake** - Tool for generating GNU Standards-compliant Makefiles
  * **libtool** - Generic library support script

  Download and install using `apt-get`:

  ```sh
  apt-get install \
          autoconf \
          automake \
          build-essential \
          freeglut3-dev \
          libssl-dev \
          libtool \
          libxi-dev \
          libx11-xcb-dev \
          libxrandr-dev \
          libopenal-dev \
          libgl1-mesa-dev \
          libglw1-mesa-dev \
          libncurses5 \
          openssl \
          valgrind \
          uuid-dev \
          xvfb
  ```

Once installed, verify the installation with
```sh
defold$ ./scripts/build.py check_sdk --verbose
```

</p></details>

<details><summary>Android...</summary><p>

There are two options:

* Download and install [Android Studio](https://developer.android.com/studio).

* Set the `ANDROID_HOME` environment variable
  * E.g. `ANDROID_HOME=/path/to/android/sdk`

Once installed, verify the installation with
```sh
defold$ ./scripts/build.py check_sdk --verbose --platform=arm64-android
```

</p></details>

<details><summary>HTML5...</summary><p>

* Install [Emscripten SDK](https://emscripten.org/docs/getting_started/downloads.html)
* Set `EMSDK` environment variable to locate the local installation.
  * E.g. `EMSDK=/path/to/emsdk-4.0.7/`

Once installed, verify the installation with
```sh
defold$ ./scripts/build.py check_sdk --verbose --platform=wasm-web
```

</p></details>


<details><summary>Consoles...</summary><p>

For each console we detect the appropriate environment variables used by each platform. We refer to each vendor's installation notes for their SDK's.

Once installed, verify the installation with
```sh
defold$ ./scripts/build.py check_sdk --verbose --platform=...
```

</p></details>

<details><summary>Extender packages...</summary><p>

*NOTE: this is not required to build the engine locally!*

In order to build Extender images locally, you need access to prepackaged SDK's for each platform.

Due to licensing restrictions **the SDKs are not distributed with Defold**. You need to provide these from a URL accessible by your local machine so that `build.py` and the `install_sdk` command can download and unpack them.

__In order to simplify this process we provide scripts to download and package the SDKs__ [Read more about this process here](/scripts/package/README.md).

The path to the SDKs can either be passed to `build.py` using the `--package-path` option or by setting the `DM_PACKAGES_URL` environment variable.

</p></details>


---

## Optional Software

These tools are generally not needed for a regular build.

<details><summary>Details...</summary><p>

### Optional Software - DotNet 9

In order to build and test the csharp languange bindings locally, you need to install DotNet 9.

<details><summary>Install...</summary><p>

There are a few ways to install the DotNet sdk:

* Install via https://dotnet.microsoft.com/en-us/download/dotnet/9.0
* Install via your package manager
  * macOS: `brew search dotnet`
  * Linux: `apt-get install dotnet`
  * Windows: `choco install dotnet`

* Install via [dotnet-install.sh](https://learn.microsoft.com/en-us/dotnet/core/tools/dotnet-install-script):

  <details><summary>Linux/macOS...</summary><p>

  Bash:
  ```sh
  > chmod +x ./dotnet-install.sh
  > ./dotnet-install.sh --channel 9.0
  ```

  </p></details>

  <details><summary>Windows...</summary><p>

  PowerShell (Windows):
  ```sh
  > ./dotnet-install.ps1 -Channel 9.0
  ```
  </p></details>

</p></details>


## Optional Software for external libraries

These are needed in some special build scripts (e.g. when rebuilding external source libraries).

* **curl** - Command line tool for downloading files
* **wget** - Command line tool for downloading files
* **cmake** for easier building of external projects
* **patch** for easier patching on windows (when building external projects)

<details><summary>macOS...</summary><p>

  ```sh
  brew install wget curl cmake patch
  ````

</p></details>

<details><summary>Linux...</summary><p>

  ```sh
  apt-get install wget curl cmake patch
  ````

</p></details>


<details><summary>Windows...</summary><p>

  In powershell:
  ```sh
  choco install wget curl cmake patch
  ````

</p></details>


## Optional Software - [ccache](https://ccache.samba.org/manual.html)

This is an optional tool to help speed up building the code by caching the results.

<details><summary>macOS...</summary><p>

  ```sh
  > brew install ccache
  ```

  Configure `ccache` by running ([source](https://ccache.samba.org/manual.html))

  ```sh
  > ccache --max-size=5G
  ```

</p></details>

<details><summary>Linux...</summary><p>

  ```sh
  > apt-get install ccache
  ```

  Configure `ccache` by running ([source](https://ccache.samba.org/manual.html))

  ```sh
  > ccache --max-size=5G
  ```

</p></details>

<details><summary>Windows...</summary><p>

  In powershell:
  ```sh
  > choco install ccache
  ```

  Configure `ccache` by running ([source](https://ccache.samba.org/manual.html))

  ```sh
  > ccache --max-size=5G
  ```

</p></details>


## Optional Software

These are _rarely_ used by developers in our build setup.
They're mainly used to help unpack some archives and sync source code in a pl

* **7z** - for extracting (e.g. android archives and binaries)
* **dos2unix** tool to convert line endings of certain source files (e.g. when building files in `external/bullet3d`)


<details><summary>macOS...</summary><p>

  ```sh
  brew install 7z dos2unix
  ````

</p></details>

<details><summary>Linux...</summary><p>

  ```sh
  apt-get install 7z
  ````

</p></details>


<details><summary>Windows...</summary><p>

  In powershell:
  ```sh
  choco install 7z dos2unix
  ````

</p></details>

</p></details> <!-- Optional Software -->


---

## Alternative host systems

It is sometimes possible to cross compile to other systems.

### Build Linux from Windows using WSL (Windows Subsystem for Linux)

<details><summary>Details...</summary><p>

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


### Build Linux from macOS using Docker

<details><summary>Details...</summary><p>

Build the docker container (Ubuntu).
It will use the arch of your current host system.
```sh
./scripts/docker/build.sh
```

Run the guest system in the cwd of choice.
E.g. to start setup for building Defold engine for Linux

```sh
cd <defold root>
./scripts/docker/run.sh
./scripts/build.py shell
./scripts/build.py install_ext
...
```

</p></details>
