#!/usr/bin/env bash
# Copyright 2020-2026 The Defold Foundation
# Copyright 2014-2020 King
# Copyright 2009-2014 Ragnar Svensson, Christian Murray
# Licensed under the Defold License version 1.0 (the "License"); you may not use
# this file except in compliance with the License.
#
# You may obtain a copy of the License, together with FAQs at
# https://www.defold.com/license
#
# Unless required by applicable law or agreed to in writing, software distributed
# under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
# CONDITIONS OF ANY KIND, either express or implied. See the License for the
# specific language governing permissions and limitations under the License.

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
