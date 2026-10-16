# Ogg build tools

`package/` contains unmodified sources from
[libogg 1.3.5](https://downloads.xiph.org/releases/ogg/libogg-1.3.5.tar.xz) and
[liboggz 1.1.1](https://downloads.xiph.org/releases/liboggz/liboggz-1.1.1.tar.gz).
Defold builds the shared libraries and `oggz-validate` for desktop platforms.

Build and install with the regular Defold toolchain:

```sh
./scripts/build.py --platform=arm64-macos install_ext
```

The libraries install into `$DYNAMO_HOME/ext/lib/<platform>` and the validator
into `$DYNAMO_HOME/ext/bin/<platform>`. The macOS libraries and validator target
macOS 11.0 on both ARM64 and Intel.

To update, unpack the releases into `package/` and update the versions in
`CMakeLists.txt`. Retain the library sources and headers, validator sources,
and Windows support files used by the build, plus the upstream licenses and
README. Keep upstream source changes in a patch, as described in the
[external dependency guide](../README.md#modifications).
