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

#include <dlfcn.h>
#include <string.h>

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

#define VK_NO_PROTOTYPES
#include "../vulkan/graphics_vulkan_defines.h"

static bool g_LibraryAvailable;
static const char* g_MissingSymbol;
static int g_OpenCount;
static int g_CloseCount;
static const char* g_LibraryName;
static VkInstance g_LastInstance;
static bool g_HasCoreFeatures2;

static void VKAPI_CALL StubFunction()
{
}

static PFN_vkVoidFunction VKAPI_CALL GetInstanceProcAddr(VkInstance instance, const char* name)
{
    g_LastInstance = instance;
    if (!g_HasCoreFeatures2 && strcmp(name, "vkGetPhysicalDeviceFeatures2") == 0)
    {
        return 0;
    }

    return StubFunction;
}

static void* OpenLibrary(const char* name, int flags)
{
    ++g_OpenCount;
    g_LibraryName = name;
    return g_LibraryAvailable ? (void*) 1 : 0;
}

static void* GetSymbol(void* library, const char* name)
{
    if (g_MissingSymbol && strcmp(name, g_MissingSymbol) == 0)
    {
        return 0;
    }

    if (strcmp(name, "vkGetInstanceProcAddr") == 0)
    {
        return (void*) GetInstanceProcAddr;
    }

    return (void*) StubFunction;
}

static int CloseLibrary(void* library)
{
    ++g_CloseCount;
    return 0;
}

// Exercise the production loader with deterministic missing-library and symbol failures.
#define dlopen OpenLibrary
#define dlsym GetSymbol
#define dlclose CloseLibrary
#include "../vulkan/graphics_vulkan_dynamic.cpp"
#undef dlopen
#undef dlsym
#undef dlclose

struct VulkanLoaderTest : public jc_test_base_class
{
    void SetUp()
    {
        g_LibraryAvailable = true;
        g_MissingSymbol = 0;
        g_OpenCount = 0;
        g_CloseCount = 0;
        g_LibraryName = 0;
        g_LastInstance = VK_NULL_HANDLE;
        g_HasCoreFeatures2 = true;
        dmGraphics::g_lib_vulkan = 0;
        vkCreateInstance = 0;
        vkGetInstanceProcAddr = 0;
        vkGetDeviceProcAddr = 0;
        vkEnumerateInstanceExtensionProperties = 0;
        vkEnumerateInstanceLayerProperties = 0;
    }

};

// Missing Vulkan must return unsupported so the engine can select another adapter.
TEST_F(VulkanLoaderTest, MissingLibrary)
{
    g_LibraryAvailable = false;
    ASSERT_FALSE(dmGraphics::LoadVulkanLibrary());
    ASSERT_EQ(1, g_OpenCount);
    ASSERT_EQ(0, g_CloseCount);
    ASSERT_TRUE(vkCreateInstance == 0);
    ASSERT_TRUE(vkGetInstanceProcAddr == 0);
    ASSERT_STREQ("libvulkan.so.1", g_LibraryName);
}

// Incomplete loaders must not leave a cached handle or callable dangling entry points.
TEST_F(VulkanLoaderTest, MissingEntryPointCanRetry)
{
    const char* symbols[] = { "vkCreateInstance", "vkGetInstanceProcAddr", "vkGetDeviceProcAddr",
                              "vkEnumerateInstanceExtensionProperties", "vkEnumerateInstanceLayerProperties" };
    for (unsigned int i = 0; i < sizeof(symbols) / sizeof(symbols[0]); ++i)
    {
        g_MissingSymbol = symbols[i];
        ASSERT_FALSE(dmGraphics::LoadVulkanLibrary());
        ASSERT_TRUE(dmGraphics::g_lib_vulkan == 0);
        ASSERT_TRUE(vkCreateInstance == 0);
        ASSERT_TRUE(vkGetInstanceProcAddr == 0);
        ASSERT_TRUE(vkGetDeviceProcAddr == 0);
        ASSERT_TRUE(vkEnumerateInstanceExtensionProperties == 0);
        ASSERT_TRUE(vkEnumerateInstanceLayerProperties == 0);
    }

    ASSERT_EQ(5, g_CloseCount);
    g_MissingSymbol = 0;
    ASSERT_TRUE(dmGraphics::LoadVulkanLibrary());
    ASSERT_TRUE(vkCreateInstance != 0);
}

// Successful loading must retain one library handle across the probe and context creation.
TEST_F(VulkanLoaderTest, ReusesLoadedLibrary)
{
    ASSERT_TRUE(dmGraphics::LoadVulkanLibrary());
    ASSERT_TRUE(dmGraphics::LoadVulkanLibrary());
    ASSERT_EQ(1, g_OpenCount);
    ASSERT_EQ(0, g_CloseCount);
    ASSERT_TRUE(vkGetInstanceProcAddr == GetInstanceProcAddr);
}

// Instance entry points must be reloaded after the temporary support-probe instance is destroyed.
TEST_F(VulkanLoaderTest, ReloadsInstanceFunctions)
{
    ASSERT_TRUE(dmGraphics::LoadVulkanLibrary());
    dmGraphics::LoadVulkanFunctions((VkInstance) 1);
    ASSERT_TRUE(g_LastInstance == (VkInstance) 1);
    ASSERT_TRUE(vkCreateDevice != 0);
    ASSERT_TRUE(vkCmdUpdateBuffer != 0);
    ASSERT_TRUE(vkGetPhysicalDeviceSurfaceSupportKHR != 0);
    ASSERT_TRUE(vkQueuePresentKHR != 0);
    g_HasCoreFeatures2 = false;
    dmGraphics::LoadVulkanFunctions((VkInstance) 2);
    ASSERT_TRUE(g_LastInstance == (VkInstance) 2);
    ASSERT_TRUE(vkGetPhysicalDeviceFeatures2 != 0);
}

int main(int argc, char** argv)
{
    jc_test_init(&argc, argv);
    return jc_test_run_all();
}
