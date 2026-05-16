# CMake Primer

Use the Defold shell first. It sets `CMAKE_GENERATOR=Ninja` plus the usual
engine environment:

```bash
./scripts/build.py shell
```

## Configure

From an engine library directory, for example `engine/dlib`:

```bash
cmake -S . -B build
```

Defaults:

* `CMAKE_GENERATOR=Ninja` from `./scripts/build.py shell`
* `TARGET_PLATFORM` inferred from the host platform
* `CMAKE_BUILD_TYPE=RelWithDebInfo`
* `BUILD_TESTS=ON`

## Build

```bash
cmake --build build --target all build_tests install
```

Without tests:

```bash
cmake -S . -B build -DBUILD_TESTS=OFF
cmake --build build --target all install
```

Run tests:

```bash
cmake --build build --target run_tests
```

## Features

Pass comma-separated features with `DEFOLD_ENABLE_FEATURES`:

```bash
cmake -S engine/font -B engine/font/build -DDEFOLD_ENABLE_FEATURES=font_layout
cmake -S engine/physics -B engine/physics/build -DDEFOLD_ENABLE_FEATURES=box2dv3
cmake -S engine/gamesys -B engine/gamesys/build -DDEFOLD_ENABLE_FEATURES=box2dv3,font_layout
```

The semicolon form also works if quoted:

```bash
cmake -S engine/gamesys -B engine/gamesys/build -DDEFOLD_ENABLE_FEATURES="box2dv3;font_layout"
```

To clear features in an existing build directory, configure with an empty value:

```bash
cmake -S engine/gamesys -B engine/gamesys/build -DDEFOLD_ENABLE_FEATURES=
```

## Common Overrides

```bash
cmake -S . -B build -DTARGET_PLATFORM=arm64-macos
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build -DBUILD_TESTS=OFF
```

If you are not in `./scripts/build.py shell`, pass `-G Ninja` or set
`CMAKE_GENERATOR=Ninja` yourself before configuring.
