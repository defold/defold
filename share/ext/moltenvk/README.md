# Building MoltenVK

NOTE!
2025-02-06: For now we have to build using an alternative rooute, the information below is not accurate.

# Vulkan SDK

* Download the latest (or a specific) version of the Mac sdk installer .dmg:

    https://vulkan.lunarg.com/sdk/home#mac

## Create the packages

    # From the defold root folder:
    ./share/ext/build.sh moltenvk <platform>

#  MoltenVk source

Optionally if you need to use a debug version of MoltenVK, you can compile it from source.

* Clone from git

    git clone git@github.com:KhronosGroup/MoltenVK.git

* Checkout `main` branch or the version you want

    cd MoltenVK
    git checkout v1.2.3


# Alternative route

1. Clone this fork (forked from rive): https://github.com/Jhonnyg/MoltenVK-Defold
2. Checkout branch: VK_EXT_rasterization_order_attachment_access-Defold
3. Build

Current packages are built from SHA: 1474891fd7d98bdae144460fce2202c66098db03 (short name: 1474891)

# Initial Setup

You need to fetch the external dependencies

    ./fetchDependencies --macos --ios --iossim

## x86_64-macos

    make macos
    rm -rf lib
    mkdir -p lib/x86_64-macos/
    cp ./Package/Release/MoltenVK/static/MoltenVK.xcframework/macos-arm64_x86_64/libMoltenVK.a lib/x86_64-macos
    lipo -thin x86_64 lib/x86_64-macos/libMoltenVK.a -o lib/x86_64-macos/libMoltenVK.a
    lipo -info lib/x86_64-macos/libMoltenVK.a
    tar czvf moltenvk-1474891-x86_64-macos.tar.gz lib

## arm64-macos

    make macos
    rm -rf lib
    mkdir -p lib/arm64-macos/
    cp ./Package/Release/MoltenVK/static/MoltenVK.xcframework/macos-arm64_x86_64/libMoltenVK.a lib/arm64-macos
    lipo -thin arm64 lib/arm64-macos/libMoltenVK.a -o lib/arm64-macos/libMoltenVK.a
    lipo -info lib/arm64-macos/libMoltenVK.a
    tar czvf moltenvk-1474891-arm64-macos.tar.gz lib

## arm64-ios

    make ios
    rm -rf lib
    mkdir -p lib/arm64-ios/
    cp ./Package/Release/MoltenVK/static/MoltenVK.xcframework/ios-arm64/libMoltenVK.a lib/arm64-ios
    lipo -thin arm64 lib/arm64-ios/libMoltenVK.a -o lib/arm64-ios/libMoltenVK.a
    lipo -info lib/arm64-ios/libMoltenVK.a
    tar czvf moltenvk-1474891-arm64-ios.tar.gz lib

## arm64_sim-ios

The simulator graphics tests were validated with upstream MoltenVK 1.4.2.
The older 1.2.3 and 1.2.11 libraries crash on the iOS 26.5 simulator during
initialization. Use a simulator library containing the subsequent upstream
simulator fixes; the existing device/macOS packages are unchanged.

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
