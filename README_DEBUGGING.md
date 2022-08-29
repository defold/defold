# Debugging

## Get the _crash / .dmp files

### Android

The adb output says where it is located (different location on different devices)

If the app is [debuggable](), you can get the crash log like so:

	$ adb shell "run-as com.defold.adtest sh -c 'cat /data/data/com.defold.adtest/files/_crash'" > ./_crash

If the device has `su`, you can try these commands (The device likely has to be rooted):

	$ adb pull /data/data/com.defold.examples/files/_crash ./_crash

If those fail, try the [Device File Explorer](https://developer.android.com/studio/debug/device-file-explorer) in Android Studio.


### iOS

## Get engine version and SHA1 from binary

### Android

	$ objdump -s -j .rodata blossom_45_0_1/lib/armeabi-v7a/libblossom_blast_saga.so | grep -e "1\.2\."

Look for "1.2.XXX", which is usually the first string, followed by the SHA1

### iOS + OSX

    $ strings test.app/test | grep -e "1\.2\.[0-9]+"


### Get engine SHA1

iOS + OSX:

    $ strings pewpew/Payload/Pewpew.app/Pewpew | grep -E "^[0-9a-f]{40}"


## Symbolicate crashes


### Android

1. Download the engine:

	$ wget http://d.defold.com/archive/<sha1>/engine/armv7-android/dmengine.apk

1. Alternatively, get it from your build folder

	$ ls <project>/build/<platform>/[lib]dmengine[.exe|.so]

1. Unzip to a folder:

	$ unzip dmengine.apk -d dmengine_1_2_105

1. Find the callstack address

	E.g. in the non symbolicated callstack on Crash Analytics, it could look like this

	`#00 pc 00257224 libblossom_blast_saga.so`

	Where *00257224* is the address

1. Resolve the address

```
    # 32 bit
    $ arm-linux-androideabi-addr2line -C -f -e dmengine_1_2_105/lib/armeabi-v7a/libdmengine.so <address>

    # 64 bit
    $ aarch64-linux-android-addr2line -C -f -e build/default/extension-push/extension-push.apk.symbols/lib/arm64-v8a/libextensionpush.so <address>
```

### iOS

1. Download the symbols (.dSYM)

1. If you're not using Native Extensions, download the vanilla symbols:

	$ wget http://d.defold.com/archive/<sha1>/engine/arm64-ios/dmengine.dSYM

1. If you are using Native Extensions, the server can provide those for you (pass "--with-symbols" to bob.jar)

	$ unzip <project>/build/arm64-ios/build.zip

	it will produce a Contents/Resources/DWARF/dmengine

1. Symbolicate using load address

	For some reason, simply putting the address from the callstack doesn't work (i.e. load address 0x0)

	$ atos -arch arm64 -o Contents/Resources/DWARF/dmengine 0x1492c4

	Neither does specifying the load address directly

	$ atos -arch arm64 -o MyApp.dSYM/Contents/Resources/DWARF/MyApp -l0x100000000 0x1492c4

	Adding the load address to the address works:

	$ atos -arch arm64 -o MyApp.dSYM/Contents/Resources/DWARF/MyApp 0x1001492c4
	dmCrash::OnCrash(int) (in MyApp) (backtrace_execinfo.cpp:27)

### macOS

Same as iOS, except you don't need to specify the <arch>

#### Other tools

UUID:

	# Check the UUID of an executable
	$ dwarfdump --uuid dmengine
	# Check the UUID of the debug symbols
	$ dwarfdump --uuid dmengine.dSYM/Contents/Resources/DWARF/dmengine


### HTML5

1. Download the engine:

	$ wget http://d.defold.com/archive/<sha1>/engine/armv7-android/dmengine.js

1. Download the symbols

	$ ... dmengine.js.symbols

1. Match the callstack with the symbols
