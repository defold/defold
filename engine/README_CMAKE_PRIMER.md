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

## Common Overrides

```bash
cmake -S . -B build -DTARGET_PLATFORM=arm64-macos
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake -S . -B build -DBUILD_TESTS=OFF
```

If you are not in `./scripts/build.py shell`, pass `-G Ninja` or set
`CMAKE_GENERATOR=Ninja` yourself before configuring.
