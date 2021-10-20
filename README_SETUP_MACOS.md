# Setup Engine - macOS

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

You need a 64 bit Python 2 version (x86_64) to build the engine and tools. The latest tested on all platforms is Python 2.7.16.
Big Sur comes with both Python 2.7.16 and pip installed.
On Big Sur Python is a universal binary (x86_64 + arm64), and there's no guarantuee which version it will pick. Using the x86_64 version is a requirement for our tools to work (we load shared libraries, which we build for x86_64).

For older versions of macOS you may need to install using [Brew](https://brew.sh/):

```sh
> brew install python2
```


### Required Software - macOS

You need the `dos2unix` command line tool to convert line endings of certain source files when building files in `share/ext`. You can install `dos2unix` using [Brew](https://brew.sh/):

```sh
> brew install dos2unix
```

---

## Optional Software

It is recommended but not required that you install the following software:

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
