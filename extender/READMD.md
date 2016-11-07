# TODO

* [x] Collect and link static and dynamic libraries
* [ ] config.yml for server
* [ ] Setup linux build machine with OS X toolchain
* [ ] editor2 and save. doesn't work with cpp files?
* [ ] Run and launch from editor
    - [x] Build engine
    - [ ] Cache
    - [ ] Copy shared libraries to a stage folder. Make sure libraries are loaded relative to executable
* [x] Test with extensions in libraries

## Later
* [ ] Build Caching. Both locally and server-side. Can be different methods
* [ ] Upload cache. "Don't upload big files every time"
* [ ] Deploy and download of Defold SDK
* [ ] Archive Defold SDK
* [ ] Document
    - [ ] config.yml
    - [ ] directory structure (src, lib) etc
* [ ] osx vs darwin and the rest of the engine
* [ ] Unit tests and platforms. Currently x86-osx only
* [ ] Platform defines? e.g. DM_OSX

     curl -H "Expect: " -F steam/src/steam.cpp=@steam/src/steam.cpp -F steam/ext.manifest=@steam/ext.manifest  -F steam/include/steam/isteamapplist.h=@steam/include/steam/isteamapplist.h -F steam/include/steam/isteamapps.h=@steam/include/steam/isteamapps.h -F steam/include/steam/isteamappticket.h=@steam/include/steam/isteamappticket.h -F steam/include/steam/isteamclient.h=@steam/include/steam/isteamclient.h -F steam/include/steam/isteamcontroller.h=@steam/include/steam/isteamcontroller.h -F steam/include/steam/isteamfriends.h=@steam/include/steam/isteamfriends.h -F steam/include/steam/isteamgamecoordinator.h=@steam/include/steam/isteamgamecoordinator.h -F steam/include/steam/isteamgameserver.h=@steam/include/steam/isteamgameserver.h -F steam/include/steam/isteamgameserverstats.h=@steam/include/steam/isteamgameserverstats.h -F steam/include/steam/isteamhtmlsurface.h=@steam/include/steam/isteamhtmlsurface.h -F steam/include/steam/isteamhttp.h=@steam/include/steam/isteamhttp.h -F steam/include/steam/isteaminventory.h=@steam/include/steam/isteaminventory.h -F steam/include/steam/isteammasterserverupdater.h=@steam/include/steam/isteammasterserverupdater.h -F steam/include/steam/isteammatchmaking.h=@steam/include/steam/isteammatchmaking.h -F steam/include/steam/isteammusic.h=@steam/include/steam/isteammusic.h -F steam/include/steam/isteammusicremote.h=@steam/include/steam/isteammusicremote.h -F steam/include/steam/isteamnetworking.h=@steam/include/steam/isteamnetworking.h -F steam/include/steam/isteamps3overlayrenderer.h=@steam/include/steam/isteamps3overlayrenderer.h -F steam/include/steam/isteamremotestorage.h=@steam/include/steam/isteamremotestorage.h -F steam/include/steam/isteamscreenshots.h=@steam/include/steam/isteamscreenshots.h -F steam/include/steam/isteamugc.h=@steam/include/steam/isteamugc.h -F steam/include/steam/isteamunifiedmessages.h=@steam/include/steam/isteamunifiedmessages.h -F steam/include/steam/isteamuser.h=@steam/include/steam/isteamuser.h -F steam/include/steam/isteamuserstats.h=@steam/include/steam/isteamuserstats.h -F steam/include/steam/isteamutils.h=@steam/include/steam/isteamutils.h -F steam/include/steam/isteamvideo.h=@steam/include/steam/isteamvideo.h -F steam/include/steam/matchmakingtypes.h=@steam/include/steam/matchmakingtypes.h -F steam/include/steam/steam_api.h=@steam/include/steam/steam_api.h -F steam/include/steam/steam_api_flat.h=@steam/include/steam/steam_api_flat.h -F steam/include/steam/steam_gameserver.h=@steam/include/steam/steam_gameserver.h -F steam/include/steam/steamclientpublic.h=@steam/include/steam/steamclientpublic.h -F steam/include/steam/steamencryptedappticket.h=@steam/include/steam/steamencryptedappticket.h -F steam/include/steam/steamhttpenums.h=@steam/include/steam/steamhttpenums.h -F steam/include/steam/steamps3params.h=@steam/include/steam/steamps3params.h -F steam/include/steam/steamtypes.h=@steam/include/steam/steamtypes.h -F steam/include/steam/steamuniverse.h=@steam/include/steam/steamuniverse.h -Fsteam/lib/x86-osx/libsteam_api.dylib=@steam/lib/x86-osx/libsteam_api.dylib  http://localhost:9000/build/x86-osx -v > dmengine && chmod +x ./dmengine && ./dmengine

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
