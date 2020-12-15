# Setup Engine - Windows

(Setup instructions for the editor [here](/editor/README.md)).

## Required Software

### Required Software - Java JDK 11

You need Java JDK 11 installed to build the tools. [Download and install release 11.0.1 from OpenJDK](https://jdk.java.net/archive/). When Java is installed you may also add need to add java to your PATH and export JAVA_HOME:

```sh
> nano ~/.bashrc

export JAVA_HOME=<JAVA_INSTALL_PATH>
export PATH=$JAVA_HOME/bin:$PATH
```

Verify that Java is installed and working:

```sh
> javac -version
```


### Required Software - Python 2

You need a 64 bit Python 2 version to build the engine and tools. The latest tested on all platforms is Python 2.7.16. You also need `easy_install` to install additional packages. Install Python from the [official download page](https://www.python.org/downloads/windows/). There is an install option to add `C:\Python27` to the PATH environment variable, select it or add the path manually. You also need to add `C:\Python27\Scripts` to the PATH to be able to use Easy Install.


### Required Software

#### Visual C++ 2019 Community

[Download](https://visualstudio.microsoft.com/vs/older-downloads/) the Community version or use the Professional or Enterprise version if you have the proper licence. When installing, select the "Desktop Development with C++" workload. There is also an optional 3rd party git client.

#### MSYS/MinGW

This will get you a shell that behaves like Linux and is much easier to build Defold through. [Download](http://www.mingw.org/download/installer) and run the installer and check these packages (binary):

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

You need to [download](https://git-scm.com/download/win) a command line version of Git. During install, select the option to not do any CR/LF conversion. If you use ssh (public/private keys) to access GitHub then:

- Run Git GUI
- Help > Show SSH Key
- If you don't have an SSH Key, press Generate Key
- Add the public key to your Github profile
- You might need to run `start-ssh-agent` (in `C:\Program Files\Git\cmd`)

Alternatively, you can easily create your own key from command line:

```sh
$ ssh-keygen -t rsa -b 1024 -C "user@domain.com"
# Copy the contents of the public file
$ cat ~/.ssh/id_rsa.pub
# Add the public key to your Github profile
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

* [Chocolatey](https://chocolatey.org/docs/installation) - Chocolatey is another package installer that will help install various helper tools such as [ripgrep](https://github.com/BurntSushi/ripgrep)

* [ripgrep](https://github.com/BurntSushi/ripgrep) - A very fast text search program

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
