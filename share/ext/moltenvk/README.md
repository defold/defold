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

# Initial Setup

You need to fetch the external dependencies

    ./fetchDependencies

## x86_64-macos

    make macos
    rm -rf lib
    mkdir -p lib/x86_64-macos/
    cp ./Package/Release/MoltenVK/MoltenVK.xcframework/macos-arm64_x86_64/libMoltenVK.a lib/x86_64-macos
    lipo -thin x86_64 lib/x86_64-macos/libMoltenVK.a -o lib/x86_64-macos/libMoltenVK.a
    lipo -info lib/x86_64-macos/libMoltenVK.a
    tar czvf MoltenVK-1.2.3-x86_64-macos.tar.gz lib

## arm64-macos

    make macos
    mkdir -p lib/arm64-macos/
    cp ./Package/Release/MoltenVK/MoltenVK.xcframework/macos-arm64_x86_64/libMoltenVK.a lib/arm64-macos
    lipo -thin arm64 lib/arm64-macos/libMoltenVK.a -o lib/arm64-macos/libMoltenVK.a
    lipo -info lib/arm64-macos/libMoltenVK.a
    tar czvf MoltenVK-1.2.3-arm64-macos.tar.gz lib

## arm64-ios

    make ios
    mkdir -p lib/arm64-ios/
    cp ./Package/Release/MoltenVK/MoltenVK.xcframework/ios-arm64/libMoltenVK.a lib/arm64-ios
    lipo -thin arm64 lib/arm64-ios/libMoltenVK.a -o lib/arm64-ios/libMoltenVK.a
    lipo -info lib/arm64-ios/libMoltenVK.a
    tar czvf MoltenVK-1.2.3-arm64-ios.tar.gz lib

## x86_64-ios

    make iossim
    mkdir -p lib/x86_64-ios/
    cp ./Package/Release/MoltenVK/MoltenVK.xcframework/ios-arm64_x86_64-simulator/libMoltenVK.a lib/x86_64-ios
    lipo -thin x86_64 lib/x86_64-ios/libMoltenVK.a -o lib/x86_64-ios/libMoltenVK.a
    lipo -info lib/x86_64-ios/libMoltenVK.a
    tar czvf MoltenVK-1.2.3-x86_64-ios.tar.gz lib
