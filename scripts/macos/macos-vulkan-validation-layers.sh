#!/usr/bin/env bash
if [ -z "$VULKAN_SDK" ]; then
    echo "VULKAN_SDK must be set"
    exit 1
fi

if [ ! -d "${VULKAN_SDK}" ]; then
    echo "VULKAN_SDK is set, but doesn't exist"
fi

PLATFORM=$1
EXE=$DYNAMO_HOME/tmp/dynamo_home/bin/$PLATFORM/dmengine

export DYLD_LIBRARY_PATH=$VULKAN_SDK/macOS/lib

# Debug options, do not remove!
#
# export VK_ICD_FILENAMES=$VULKAN_SDK/macOS/share/vulkan/icd.d/MoltenVK_icd.json
# export VK_DRIVER_FILES=$VULKAN_SDK/macOS/share/vulkan/icd.d/MoltenVK_icd.json
# export VK_ADD_LAYER_PATH=$VULKAN_SDK/macOS/share/vulkan/explicit_layer.d
# export VK_LOADER_DEBUG=all

$DYNAMO_HOME/bin/$PLATFORM/dmengine --graphics-adapter=vulkan --use-validation-layers
