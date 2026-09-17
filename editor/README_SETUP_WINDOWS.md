# Setup Editor - Windows

## Required Software

### Required Software - Java JDK 25
Download and install the latest JDK 25 release from either of these locations:

* [Adoptium/Temurin](https://github.com/adoptium/temurin25-binaries/releases) - The Adoptium Working Group promotes and supports high-quality runtimes and associated technology for use across the Java ecosystem
* [Microsoft OpenJDK builds](https://docs.microsoft.com/en-us/java/openjdk/download#openjdk-25) - The Microsoft Build of OpenJDK is a no-cost distribution of OpenJDK that's open source and available for free for anyone to deploy anywhere

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

### Notes

If you are using IntelliJ for lein tasks, you will need to first add the new SDK (file->project structure/SDKs)
and then set the project SDK setting (file->project structure/Project) to the new version.


### Required Software - Leiningen

Start by following the Windows instructions in the [engine setup guide](../README_SETUP.md).

* Download the [lein script](https://raw.githubusercontent.com/technomancy/leiningen/stable/bin/lein) from [Leiningen.org](http://leiningen.org) and put it somewhere in your PATH. You might need to `chmod a+x lein`.
* Start Git Bash and run `lein` in the `editor` subdirectory.

  This will attempt to download leiningen and dependencies to your home directory.

  - If this fails with message

          Could not find or load main class clojure.main

    Try pointing your `HOME` environment variable to your "windows home". For instance change it from `/home/Erik.Angelin` (msys) to `/c/Users/erik.angelin`:

        export HOME="/c/Users/erik.angelin"

    The problem seems to be that the (windows) java class path points to an invalid home directory.

  - If this fails because the github certificate cannot be verified:

          export HTTP_CLIENT='wget --no-check-certificate -O'
