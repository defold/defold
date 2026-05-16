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

#ifndef DM_GRAPHICS_VULKAN_ANDROID_H
#define DM_GRAPHICS_VULKAN_ANDROID_H

#include <stdint.h>

#include "../graphics_vulkan_defines.h"

namespace dmGraphics
{
    struct VulkanContext;

    void     AndroidVulkanBeginFrame(VulkanContext* context);
    bool     AndroidVulkanHandleWindowSurfaceChange(VulkanContext* context, uint32_t window_width, uint32_t window_height);
    void     AndroidVulkanInitializeContext(VulkanContext* context);
    void     SyncAndroidVulkanWindowSize(VulkanContext* context);
    VkResult RecreateAndroidWindowSurface(void* ctx);
}

#endif // DM_GRAPHICS_VULKAN_ANDROID_H
