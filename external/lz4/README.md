# LZ4

The `src` directory contains the unmodified raw-block compression and
decompression sources from [LZ4 1.10.0](https://github.com/lz4/lz4/releases/tag/v1.10.0).
Defold uses LZ4 HC level 9 through its dlib wrapper.

Build and install with the regular Defold toolchain:

```sh
./scripts/build.py --platform=arm64-macos build_ext
```

This installs `liblz4` into `${DYNAMO_HOME}/ext/lib/<platform>` and the headers
into `${DYNAMO_HOME}/ext/include/lz4`. Both dlib and dlib_shared link the static
library. Run `build_ext` for each target platform before building the engine.

To update, check out the desired upstream tag and run:

```sh
./external/lz4/update_lz4.sh /path/to/lz4
```

Update the version in this file and `CMakeLists.txt` when changing releases,
and keep `licenses/NOTICE-lz4` in sync with the upstream license.
