mkdir -p bin/linux
mkdir -p bin/darwin
mkdir -p bin/win32
[ -f ~/builds/linux/dmengine ] && cp -v ~/builds/linux/dmengine bin/linux
[ -f ~/builds/darwin/dmengine ] && cp -v ~/builds/darwin/dmengine bin/darwin
[ -f ~/builds/win32/dmengine.exe ] && cp -v ~/builds/win32/dmengine.exe bin/win32

