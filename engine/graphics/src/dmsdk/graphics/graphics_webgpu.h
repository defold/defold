// Copyright 2020-2025 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DMSDK_GRAPHICS_WEBGPU_H
#define DMSDK_GRAPHICS_WEBGPU_H

#include <dmsdk/graphics/graphics.h>

#ifdef __EMSCRIPTEN__
    #include <webgpu/webgpu.h>
#else
    typedef int WGPUQueue;
    typedef int WGPUDevice;
    typedef int WGPUTextureView;
#endif

/*# Graphics API documentation
 * [file:<dmsdk/graphics/graphics_webgpu.h>]
 *
 * Graphics WebGPU API
 *
 * @document
 * @name Graphics WebGPU
 * @namespace dmGraphics
 * @language C++
 */

namespace dmGraphics
{
    WGPUDevice      WebGPUGetDevice(HContext context);
    WGPUQueue       WebGPUGetQueue(HContext context);
    WGPUTextureView WebGPUGetTextureView(HContext context, HTexture texture);
    HTexture        WebGPUGetActiveSwapChainTexture(HContext context);
}

#endif
