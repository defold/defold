# Protobuf external package

Defold packages Protobuf for desktop targets only. Configure and build a package with CMake:

```sh
cmake -S external/protobuf -B external/protobuf/build/arm64-macos \
  -DTARGET_PLATFORM=arm64-macos
cmake --build external/protobuf/build/arm64-macos
```

The build writes `protobuf-35.1-<platform>.tar.gz` and
`protobuf-35.1-common.tar.gz` to `external/protobuf/package`. Set
`PROTOBUF_PACKAGE_DIR` at configure time to use another output directory.

Supported targets are `arm64-macos`, `x86_64-macos`, `arm64-linux`,
`x86_64-linux`, `win32`, and `x86_64-win32`. The Defold build environment must
be initialized so `DYNAMO_HOME` is set. The package is built from the source
archives in this directory and does not require `install_ext`.
