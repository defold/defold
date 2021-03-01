# Setup Editor - Linux

## Required Software

### Required Software - Java JDK 11

* Download [Linux](https://download.java.net/java/GA/jdk11/9/GPL/openjdk-11.0.2_linux-x64_bin.tar.gz) version and extract it somewhere
* Run `sudo update-alternatives --install "/usr/bin/java" "java" "/path/to/jdk/bin/java" 1102`
* Run `sudo update-alternatives --install "/usr/bin/javac" "javac" "/path/to/jdk/bin/javac" 1102`
* If required, switch to new version using `sudo update-alternatives --config java` and `sudo update-alternatives --config javac`

### Notes

If you are using IntelliJ for lein tasks, you will need to first add the new SDK (file->project structure/SDKs)
and then set the project SDK setting (file->project structure/Project) to the new version.


## Required Software - Leiningen

* Install Leiningen `brew install leiningen`
* Leiningen for [other package managers](https://github.com/technomancy/leiningen/wiki/Packaging)
