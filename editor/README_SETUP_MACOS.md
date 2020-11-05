# Setup Editor - macOS

## Required Software

### Required Software - Java JDK 11

[Download and install release 11.0.1 from OpenJDK](https://jdk.java.net/archive/). When Java is installed you may also add need to add java to your PATH and export JAVA_HOME:

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
