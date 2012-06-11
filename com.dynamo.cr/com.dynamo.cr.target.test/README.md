The test executable in test/test_exe was compiled with

    /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/usr/bin/llvm-g++-4.2 -framework Foundation -arch armv6 -lobjc -isysroot /Applications/Xcode.app/Contents/Developer/Platforms/iPhoneOS.platform/Developer/SDKs/iPhoneOS5.1.sdk -dead_strip -miphoneos-version-min=5.1 test_exe.cpp -o test_exe

The application contains a single printf("Hello World") statement
