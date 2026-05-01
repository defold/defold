// Copyright 2020-2026 The Defold Foundation
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
    #if defined(DM_GRAPHICS_WEBGPU_WAGYU)
        #include <webgpu/webgpu_wagyu.h>
    #else
        #include <webgpu/webgpu.h>
    #endif

#else
    typedef int WGPUInstance;
    typedef int WGPUAdapter;
    typedef int WGPUQueue;
    typedef int WGPUDevice;
    typedef int WGPUTexture;
    typedef int WGPUTextureView;
    typedef int WGPUCommandEncoder;
    typedef int WGPURenderPassEncoder;
#endif

/*# Graphics API documentation
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
    WGPUInstance            WebGPUGetInstance(HContext context);
    WGPUAdapter             WebGPUGetAdapter(HContext context);
    WGPUDevice              WebGPUGetDevice(HContext context);
    WGPUQueue               WebGPUGetQueue(HContext context);
    WGPUTextureView         WebGPUGetTextureView(HContext context, HTexture texture);
    WGPUTexture             WebGPUGetTexture(HContext context, HTexture texture);
    HTexture                WebGPUGetActiveSwapChainTexture(HContext context);
    WGPUCommandEncoder      WebGPUGetActiveCommandEncoder(HContext context);
    // Ends the current render passes, letting someone else use the encoder to post new render passes
    void                    WebGPURenderPassEnd(HContext context);
    void                    WebGPURenderPassBegin(HContext context);
}

#endif
