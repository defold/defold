# LZ4

The `src` directory contains the unmodified raw-block compression and
decompression sources from [LZ4 1.11.0](https://github.com/lz4/lz4/releases/tag/v1.11.0).
Defold uses LZ4 HC level 9 through its dlib wrapper.

CMake applies [defold.patch](defold.patch) to a build copy of the HC source.
It restores forced inlining of `LZ4HC_compress_generic_internal()`, avoiding
the HC level 9 slowdown measured with Apple clang at `-O2`. The change adds
approximately 12 KiB of HC code on ARM64 macOS; other targets remain unmeasured.
The vendored files stay unmodified, and the patch is reapplied automatically
when the sources or patch change. See the [benchmark results](benchmark/RESULTS.md).

Build and install with the regular Defold toolchain:

```sh
./scripts/build.py --platform=arm64-macos install_ext
```

This installs `liblz4` into `${DYNAMO_HOME}/ext/lib/<platform>` and the headers
into `${DYNAMO_HOME}/ext/include/lz4`. Both dlib and dlib_shared link the static
library. Run `install_ext` for each target platform before building the engine.

To update, check out the desired upstream tag and run:

```sh
./external/lz4/update_lz4.sh /path/to/lz4
```

Update the version in this file and `CMakeLists.txt` when changing releases,
and keep `licenses/NOTICE-lz4` in sync with the upstream license.

## Threading and benchmarks

Bob calls the same native HC level 9 compressor through JNA and `dlib_shared`.
It already compresses separate resources concurrently in `ArchiveBuilder.write()`.

LZ4 1.11.0 adds experimental worker threads to the **frame** API, not the raw-block
API used by Defold archives, live update resources, and font glyphs. Enabling it
for those resources would require a resource format change and matching readers.
The engine library therefore continues to build only the raw-block sources.

The standalone [benchmark](benchmark/README.md) compares the engine wrapper
before and after an update, and separately exercises threaded frame compression
without changing the engine's resource format or adding runtime dependencies.
