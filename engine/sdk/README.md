SDK
======

SDK packaging and testing support module


## Build and tests

The module installs the [public umbrella header](src/dmsdk/sdk.h) in `$DYNAMO_HOME/sdk/include/dmsdk/`. When adding a public API to this header, edit the checked-in file.

With tests enabled, the [SDK test project](src/test/CMakeLists.txt) compiles public headers from C++ and the C-compatible subset from C. Its runtime test can be run from the repository root with `cmake --build engine/build/arm64-macos --target run_test_sdk`, using your configured platform.
