# Building MoltenVK

## macOS and iOS devices

The [engine package list](../../../scripts/build.py) pins the macOS and iOS device libraries to `moltenvk-1474891`. To reproduce those packages, check out the matching commit in the [Defold MoltenVK fork](https://github.com/Jhonnyg/MoltenVK-Defold).

```sh
git clone https://github.com/Jhonnyg/MoltenVK-Defold.git
cd MoltenVK-Defold
git checkout 1474891fd7d98bdae144460fce2202c66098db03
./fetchDependencies --macos --ios
```

Build the macOS library once and package its two architectures separately.

```sh
make macos
library=Package/Release/MoltenVK/static/MoltenVK.xcframework/macos-arm64_x86_64/libMoltenVK.a
for arch in arm64 x86_64; do
    platform="$arch-macos"
    mkdir -p "lib/$platform"
    lipo -thin "$arch" "$library" -o "lib/$platform/libMoltenVK.a"
    tar -czf "moltenvk-1474891-$platform.tar.gz" "lib/$platform"
done
```

For an iOS device, package the arm64 library directly.

```sh
make ios
mkdir -p lib/arm64-ios
cp Package/Release/MoltenVK/static/MoltenVK.xcframework/ios-arm64/libMoltenVK.a lib/arm64-ios/
tar -czf moltenvk-1474891-arm64-ios.tar.gz lib/arm64-ios
```

## arm64_sim-ios

The simulator graphics tests were validated with upstream MoltenVK 1.4.2.
The older 1.2.3 and 1.2.11 libraries crash on the iOS 26.5 simulator during
initialization. Use a simulator library containing the subsequent upstream
simulator fixes.

From an upstream 1.4.2 checkout, fetch dependencies and build the simulator slice:

    ./fetchDependencies --iossim
    make iossim
    mkdir -p lib/arm64_sim-ios/
    lipo -thin arm64 ./Package/Release/MoltenVK/static/MoltenVK.xcframework/ios-arm64_x86_64-simulator/libMoltenVK.a -o lib/arm64_sim-ios/libMoltenVK.a
    tar czvf moltenvk-1.4.2-arm64_sim-ios.tar.gz lib/arm64_sim-ios

Alternatively, the official [1.4.2 release](https://github.com/KhronosGroup/MoltenVK/releases/tag/v1.4.2)
`MoltenVK-all.tar` contains the prebuilt simulator slice at
`MoltenVK/MoltenVK/static/MoltenVK.xcframework/ios-arm64_x86_64-simulator/libMoltenVK.a`.
Extract its arm64 slice with `lipo` into `lib/arm64_sim-ios` as above.

Install the archive under `$DYNAMO_HOME/ext` before configuring
`arm64_sim-ios` with `WITH_VULKAN=ON`. A device-only `arm64-ios` archive
cannot be linked into a simulator executable.
