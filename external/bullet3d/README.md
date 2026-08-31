# Bullet 3D

Defold builds the core Bullet 3.25 libraries with CMake. The vendored source is
limited to `LinearMath`, `BulletCollision`, and `BulletDynamics` from the
official `3.25` tag (`2c204c49e56ed15ec5fcfa71d199ab6d6570b3f5`). The
`bullet-3.25.zip` archive is a matching snapshot containing only these sources.

Build and package one platform through the regular build script:

```sh
./scripts/build.py --platform=arm64-macos --package=bullet3d build_external
```

This writes `packages/bullet-3.25-arm64-macos.tar.gz` and
`packages/bullet-3.25-common.tar.gz`.
