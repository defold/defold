# Setup Engine - Windows

(Setup instructions for the editor [here](/editor/README.md)).

## Required Software


### Package managers

* [Chocolatey](https://chocolatey.org/docs/installation) - Chocolatey is a package installer that will help install various helper tools such as python, ripgrep etc.

Open a Command (cmd.exe) as administator and run:

`@"%SystemRoot%\System32\WindowsPowerShell\v1.0\powershell.exe" -NoProfile -InputFormat None -ExecutionPolicy Bypass -Command "[System.Net.ServicePointManager]::SecurityProtocol = 3072; iex ((New-Object System.Net.WebClient).DownloadString('https://community.chocolatey.org/install.ps1'))" && SET "PATH=%PATH%;%ALLUSERSPROFILE%\chocolatey\bin"`

Once this is done, you can install new packages which are added to the PATH, by running:

```sh
choco install <package_name>
```

### Required Software - Java JDK 11

You need Java JDK 11 installed to build the tools. [Download and install release 11.0.1 from OpenJDK](https://jdk.java.net/archive/). 

Ìnstall using Chocolatey:

```sh
choco install openjdk11
```

When Java is installed you may also need to add java to your PATH and export JAVA_HOME:

```sh
> nano ~/.bashrc

export JAVA_HOME=<JAVA_INSTALL_PATH>
export PATH=$JAVA_HOME/bin:$PATH
```

*With choco, the install path is something like /c/Program\ Files/OpenJDK/openjdk-11.0.13_8*

Verify that Java is installed and working:

```sh
> javac -version
```


### Required Software - Python 2

You need a 64 bit Python 2 version to build the engine and tools. The latest tested on all platforms is Python 2.7.18. Install Python from the [official download page](https://www.python.org/downloads/windows/). There is an install option to add `C:\Python27` to the PATH environment variable, select it or add the path manually.

Install using Chocolatey:

```sh
choco install python2
```

Since we need a python2 executable, you need to make a duplicate in `C:\Python27\python2.exe`

### Required Software

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

---

## Optional Software

It is recommended but not required that you install the following software:

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


### Optional Software

* [ripgrep](https://github.com/BurntSushi/ripgrep) - A very fast text search program

Open a Command (cmd.exe) as administator and run:

`choco install ripgrep`

---

## Optional Setup

### Optional Setup - Command Prompt

It's useful to modify your command prompt to show the status of the repo you're in.
E.g. it makes it easier to keep the git branches apart.

You do this by editing the `PS1` variable. Put it in the recommended config for your shell (e.g. `.profile` or `.bashrc`)
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
