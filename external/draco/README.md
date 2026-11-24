# draco

Pinned upstream revision: [1.5.7](https://github.com/google/draco/tree/1.5.7).
Archive: `https://github.com/google/draco/archive/refs/tags/1.5.7.tar.gz`

SHA-256: `bf6b105b79223eab2b86795363dfe5e5356050006a96521477973aba8f036fe1`.

`package/draco-1.5.7` contains unchanged upstream sources and license files.
Defold's build configuration lives outside that directory. There are no upstream
patches for this version. Keep future dependency modifications in patches alongside
this README, and apply them to a build copy rather than editing the package.

Build from a Defold build shell using CMake through `build_ext`:

```sh
./scripts/build.py --platform=arm64-macos build_ext
```

Supported tool targets are `arm64-macos`, `x86_64-macos`, `arm64-linux`,
`x86_64-linux`, and `x86_64-win32`. Cross-builds also build host dependencies.
Archives install into `$DYNAMO_HOME/ext/lib/<platform>`, headers into
`$DYNAMO_HOME/ext/include/draco`, and the license into
`$DYNAMO_HOME/ext/share/licenses/draco`. The distribution notice is
`licenses/NOTICE-draco`.

The static `draco_decoder` library compiles an explicit decoder/support source
list. Its generated feature header matches upstream `DRACO_GLTF_BITSTREAM`:
mesh compression, standard Edgebreaker, and normal decoding. Point-cloud KD trees,
encoders, IO, transcoding, plugins, and command-line tools are excluded. The
shared attribute-transform support requires `core/encoder_buffer.cc`; this does
not bring in geometry encoding. C++11 and STL use stay inside the dependency and
modelc's private adapter. CRT and sanitizer options come from Defold's toolchain.

The package retains the decoder/support sources listed in our CMake file and
all recursively included `draco/` headers (163 source/header files). Unused
encoder, test, IO, and plugin sources are omitted. Each retained file is unchanged.

To update, reconcile the source list and glTF feature definitions with upstream
CMake. Copy those sources and their header dependencies, plus upstream
`CMakeLists.txt`, `AUTHORS`, and `LICENSE`. Update the checksum and distribution
notice together. The generated `draco_features.h` comes from our CMake file.

`generate_test_mesh.cpp` is an offline fixture generator. It is excluded from
`build_ext` and uses the full upstream encoder only when regenerating tests. See
`engine/modelc/src/test/assets/compression/README.md` for commands.
