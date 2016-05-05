# TODO

* [x] Collect and link static and dynamic libraries
* [ ] Document
    - [ ] config.yml
    - [ ] directory structure (src, lib) etc
* [ ] Run and launch from editor
    - [ ] Build engine
    - [ ] Cache
    - [ ] Copy shared libraries to a stage folder. Make sure libraries are loaded relative to executable
* [ ] osx vs darwin and the rest of the engine
* [ ] Unit tests and platforms. Currently x86-osx only

# Compiling and linking OS X on Linux

Linker (ld) from here

* [cctools-port](https://github.com/tpoechtrager/cctools-port)

clang 3.8.0

* [clang](http://llvm.org/releases/download.html)

Other interesting links

* [https://github.com/waneck/linux-ios-toolchain](https://github.com/waneck/linux-ios-toolchain)
* [https://code.google.com/archive/p/ios-toolchain-based-on-clang-for-linux/](https://code.google.com/archive/p/ios-toolchain-based-on-clang-for-linux/)

"sysroot"

* MacOSX10.11.sdk from XCode
* /usr/lib
* /System/Library/Frameworks
* /System/Library/Frameworks
* /System/Library/PrivateFrameworks

    ~/tmp/cctools-port/cctools$ ~/tmp/clang+llvm-3.8.0-x86_64-linux-gnu-ubuntu-14.04/bin/clang --target=x86_64-apple-darwin15.4.0 -c /tmp/foo.mm  -o /tmp/foo.o --sysroot=/home/ubuntu/sysroot/MacOSX10.11.sdk/ -mmacosx-version-min=10.7

    /tmp/cctools/bin/x86_64-apple-darwin11-ld  -syslibroot /home/ubuntu/sysroot /tmp/foo.o  -demangle -dynamic -arch x86_64 -L/tmp  -lSystem -o /tmp/a.out -F /home/ubuntu/sysroot/System/Library/Frameworks/ -framework Foundation -framework AppKit /home/ubuntu/sysroot/MacOSX10.11.sdk/usr/lib/crt1.10.6.o -macosx_version_min 10.7.0

    With frameworks lld crashed. Probably not ready for production
    ./bin/lld -flavor darwin -syslibroot /tmp /tmp/foo.o  -demangle -dynamic -arch x86_64 -L/tmp -v -lSystem
