# Setup Editor - macOS

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

## Required Software - Installing Leiningen

* Install Leiningen `brew install leiningen`
