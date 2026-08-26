#!/usr/bin/env bash

set -e

cp -v ./webgpu_textureformat.h ./engine/graphics/src/webgpu

cp -v ./webgpu_textureformat.h ~/work/external/rive-runtime-orig/renderer/src/webgpu/

cp -v ./webgpu_textureformat.h ~/work/projects/users/defold/extension-rive/defold-rive/src/private
