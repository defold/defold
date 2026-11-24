# Protobuf

`./scripts/build.py build_ext --platform=<platform>` builds and installs Protobuf
for desktop platforms alongside the other source dependencies. Cross-builds also
build the host tools. Supported desktop targets are `arm64-macos`,
`x86_64-macos`, `arm64-linux`, `x86_64-linux`, and `x86_64-win32`.

Protobuf and Abseil are CMake subprojects using Defold's compiler, SDK, platform
flags, and Ninja job scheduling. They build from the bundled source archives
without downloading dependencies. Objects are retained under
`external/build/<platform>/protobuf`; source, patch, and toolchain changes rebuild
the affected targets. Installation copies headers, libraries, CMake exports, and
`protoc` directly into `tmp/dynamo_home/ext`, without creating package archives.

The build compiles Abseil once and shares one `libprotobuf` between `protoc` and
the desktop DDF interoperability tests. The tests use the upstream CMake targets
to link the runtime and its dependencies. The source archive includes generated
C++ files, so cross-compiling a desktop target does not require a separate host
compiler build inside Protobuf.

`defold-build.patch` limits the compiler to the C++, Java and Python generators
and external plugins used by Defold. It removes the unused language generators,
their UPB dependency, and the separate `libprotobuf-lite` library. Headers,
standard schemas and CMake package exports remain available for consumers.

The build temporarily applies `java-parser-compat.patch` to retain
the deprecated public Java `PARSER` field required by extension JARs generated
with Protobuf 3.20. Remove it after all distributed external JARs have been
regenerated to call the public `parser()` method instead.
