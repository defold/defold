PACK_EMS="./scripts/package/package_emscripten.sh"
PACK_NDK="./scripts/package/package_android_ndk.sh"
PACK_SDK="./scripts/package/package_android_sdk.sh"
PACK_XCODE="./scripts/package/package_xcode_and_sdks.sh"

INSTALL_EMS="./scripts/build.py install_ems --package-path=./local_sdks"
    
COPY_EMS="cp ./tmp/dynamo_home/ext/SDKs/emsdk-1.39.16/.emscripten ~/"

# SHELL="./scripts/build.py shell --platform=x86_64-darwin --package-path=./local_sdks/"

INSTALL_EXT="./scripts/build.py install_ext --platform=x86_64-darwin --package-path=./local_sdks"

echo "Now downloading packages..."
eval $PACK_EMS
eval $PACK_NDK
eval $PACK_SDK
eval $PACK_XCODE

echo "running install_ems > shell > install_ext .."
eval $INSTALL_EMS
eval $COPY_EMS
eval $SHELL
eval $INSTALL_EXT

echo "finished setup environment. ready to build engine/editor."
echo "run `sh build_editor.sh` to continue..."
echo "_____________________________________________________"
