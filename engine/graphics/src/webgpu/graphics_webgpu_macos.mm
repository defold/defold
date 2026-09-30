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

#include "graphics_webgpu_private.h"
#include "../graphics_native.h"

#include <AppKit/AppKit.h>
#include <QuartzCore/CAMetalLayer.h>

namespace dmGraphics
{
    WGPUSurface WebGPUCreateSurfaceMacOS(WebGPUContext* context)
    {
        NSWindow*     window = (NSWindow*)GetNativeOSXNSWindow();
        NSView*       view = [window contentView];
        CAMetalLayer* layer = [CAMetalLayer layer];
        layer.contentsScale = [window backingScaleFactor];
        // The view retains the layer for the lifetime of the native window.
        [view setLayer:layer];
        [view setWantsLayer:YES];

        WGPUSurfaceSourceMetalLayer source = WGPU_SURFACE_SOURCE_METAL_LAYER_INIT;
        source.layer = layer;
        WGPUSurfaceDescriptor descriptor = WGPU_SURFACE_DESCRIPTOR_INIT;
        descriptor.nextInChain = &source.chain;
        return wgpuInstanceCreateSurface(context->m_Instance, &descriptor);
    }
} // namespace dmGraphics
