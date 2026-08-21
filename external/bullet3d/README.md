# Bullet 3D

Defold builds the core Bullet 3.25 libraries with CMake. The vendored source is
limited to `LinearMath`, `BulletCollision`, and `BulletDynamics` from the
official `3.25` tag.

Build and package one platform through the regular build script:

```sh
./scripts/build.py --platform=arm64-macos --package=bullet3d build_external
```

This writes `packages/bullet-3.25-arm64-macos.tar.gz` and
`packages/bullet-3.25-common.tar.gz`.
