# Bullet 3D

Defold builds the core Bullet 3.25 libraries with CMake. The vendored source is
limited to `LinearMath`, `BulletCollision`, and `BulletDynamics` from the
official `3.25` tag (`2c204c49e56ed15ec5fcfa71d199ab6d6570b3f5`). The
`bullet-3.25.zip` archive is a matching snapshot containing only these sources.

Build and install the source dependencies for one platform:

```sh
./scripts/build.py --platform=arm64-macos build_ext
```

This uses the regular Defold CMake toolchain, including configured console
platforms, and installs headers and libraries into `tmp/dynamo_home/ext`.
Run it after `install_ext` and before the first engine build, and again when
the Bullet sources or toolchain change. The CMake build directory is retained
for incremental rebuilds. Normal engine builds use the installed libraries.
