# meshoptimizer

Pinned upstream revision: [v1.2](https://github.com/zeux/meshoptimizer/tree/v1.2).
Archive: `https://github.com/zeux/meshoptimizer/archive/refs/tags/v1.2.tar.gz`

SHA-256: `e40f71b809cdf3361b9a4def85fd44534e8733ce29d4b943c145b76859e4c2b4`.

`package/meshoptimizer-1.2` contains unchanged upstream sources and license files.
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
`$DYNAMO_HOME/ext/include/meshoptimizer`, and the license into
`$DYNAMO_HOME/ext/share/licenses/meshoptimizer`. The distribution notice is
`licenses/NOTICE-meshoptimizer`.

The static `meshoptimizer` library builds with C++98. Demos and gltfpack are
disabled. Modelc's private compression adapter is its only production consumer;
linker dead stripping removes unused encoding/optimization code.

To update, extract the upstream archive and copy `src`, `CMakeLists.txt`, and
`LICENSE.md` unchanged. Update the version, archive checksum, and notice together.
