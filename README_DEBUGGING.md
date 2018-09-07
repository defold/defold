


# Debugging

## Get the _crash / .dmp files

### Android

The adb output says where it is located (different location on different devices)

If the device has `su`, you can try these commands:

	$ adb pull /data/data/com.defold.examples/files/_crash ./_crash
	or
	$ adb -d shell "run-as com.example.test cat /data/data/com.defold.examples/files/_crash" > ./_crash

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

	$ wget http://d.defold.com/archive/a285bcf69ac6de9d1cab399768b74968c80cd864/engine/armv7-android/dmengine.apk

1. Alternatively, get it from your build folder

	$ ls <project>/build/<platform>/[lib]dmengine[.exe|.so]

1. Unzip to a folder:

	$ unzip dmengine.apk -d dmengine_1_2_105

1. Find the callstack address

	E.g. in the non symbolicated callstack on Crash Analytics, it could look like this

	#00 pc 00257224 libblossom_blast_saga.so

	Where *00257224* is the address

1. Resolve the address

    $ arm-linux-androideabi-addr2line -C -f -e dmengine_1_2_105/lib/armeabi-v7a/libdmengine.so <address>



### HTML5

1. Download the engine:

	$ wget http://d.defold.com/archive/a285bcf69ac6de9d1cab399768b74968c80cd864/engine/armv7-android/dmengine.js

1. Download the symbols

	$ ... dmengine.js.symbols

1. Match the callstack with the symbols

