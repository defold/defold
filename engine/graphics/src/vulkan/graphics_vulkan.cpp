#include <string.h>
#include <assert.h>
#include <vectormath/cpp/vectormath_aos.h>

// TODO: hack!
#define VK_USE_PLATFORM_MACOS_MVK
#include <vulkan/vulkan.h>

#include <dlib/hashtable.h>
#include <dlib/array.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dlib/math.h>
#include <dlib/profile.h>

#include <graphics/glfw/glfw_native.h>

#include "../graphics.h"
#include "../graphics_native.h"
#include "graphics_vulkan.h"
#include "../null/glsl_uniform_parser.h"

#include "graphics_vulkan_platform.h"

using namespace Vectormath::Aos;

uint64_t g_Flipped = 0;

// Used only for tests
bool g_ForceFragmentReloadFail = false;
bool g_ForceVertexReloadFail = false;

namespace dmGraphics
{
    namespace Vulkan
    {
        // Ultra debug assets!
        unsigned char g_vk_vertex_shader[] = {0x03,0x02,0x23,0x07,0x00,0x00,0x01,0x00,0x07,0x00,0x0d,0x00,0x28,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x11,0x00,0x02,0x00,0x01,0x00,0x00,0x00,0x0b,0x00,0x06,0x00,0x01,0x00,0x00,0x00,0x47,0x4c,0x53,0x4c,0x2e,0x73,0x74,0x64,0x2e,0x34,0x35,0x30,0x00,0x00,0x00,0x00,0x0e,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x0f,0x00,0x07,0x00,0x00,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x6d,0x61,0x69,0x6e,0x00,0x00,0x00,0x00,0x19,0x00,0x00,0x00,0x1d,0x00,0x00,0x00,0x03,0x00,0x03,0x00,0x02,0x00,0x00,0x00,0xc2,0x01,0x00,0x00,0x04,0x00,0x09,0x00,0x47,0x4c,0x5f,0x41,0x52,0x42,0x5f,0x73,0x65,0x70,0x61,0x72,0x61,0x74,0x65,0x5f,0x73,0x68,0x61,0x64,0x65,0x72,0x5f,0x6f,0x62,0x6a,0x65,0x63,0x74,0x73,0x00,0x00,0x04,0x00,0x0a,0x00,0x47,0x4c,0x5f,0x47,0x4f,0x4f,0x47,0x4c,0x45,0x5f,0x63,0x70,0x70,0x5f,0x73,0x74,0x79,0x6c,0x65,0x5f,0x6c,0x69,0x6e,0x65,0x5f,0x64,0x69,0x72,0x65,0x63,0x74,0x69,0x76,0x65,0x00,0x00,0x04,0x00,0x08,0x00,0x47,0x4c,0x5f,0x47,0x4f,0x4f,0x47,0x4c,0x45,0x5f,0x69,0x6e,0x63,0x6c,0x75,0x64,0x65,0x5f,0x64,0x69,0x72,0x65,0x63,0x74,0x69,0x76,0x65,0x00,0x05,0x00,0x04,0x00,0x04,0x00,0x00,0x00,0x6d,0x61,0x69,0x6e,0x00,0x00,0x00,0x00,0x05,0x00,0x05,0x00,0x0c,0x00,0x00,0x00,0x70,0x6f,0x73,0x69,0x74,0x69,0x6f,0x6e,0x73,0x00,0x00,0x00,0x05,0x00,0x06,0x00,0x17,0x00,0x00,0x00,0x67,0x6c,0x5f,0x50,0x65,0x72,0x56,0x65,0x72,0x74,0x65,0x78,0x00,0x00,0x00,0x00,0x06,0x00,0x06,0x00,0x17,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x67,0x6c,0x5f,0x50,0x6f,0x73,0x69,0x74,0x69,0x6f,0x6e,0x00,0x06,0x00,0x07,0x00,0x17,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x67,0x6c,0x5f,0x50,0x6f,0x69,0x6e,0x74,0x53,0x69,0x7a,0x65,0x00,0x00,0x00,0x00,0x06,0x00,0x07,0x00,0x17,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x67,0x6c,0x5f,0x43,0x6c,0x69,0x70,0x44,0x69,0x73,0x74,0x61,0x6e,0x63,0x65,0x00,0x06,0x00,0x07,0x00,0x17,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x67,0x6c,0x5f,0x43,0x75,0x6c,0x6c,0x44,0x69,0x73,0x74,0x61,0x6e,0x63,0x65,0x00,0x05,0x00,0x03,0x00,0x19,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x05,0x00,0x06,0x00,0x1d,0x00,0x00,0x00,0x67,0x6c,0x5f,0x56,0x65,0x72,0x74,0x65,0x78,0x49,0x6e,0x64,0x65,0x78,0x00,0x00,0x48,0x00,0x05,0x00,0x17,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x0b,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x48,0x00,0x05,0x00,0x17,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x0b,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x48,0x00,0x05,0x00,0x17,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x0b,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x48,0x00,0x05,0x00,0x17,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x0b,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x47,0x00,0x03,0x00,0x17,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x47,0x00,0x04,0x00,0x1d,0x00,0x00,0x00,0x0b,0x00,0x00,0x00,0x2a,0x00,0x00,0x00,0x13,0x00,0x02,0x00,0x02,0x00,0x00,0x00,0x21,0x00,0x03,0x00,0x03,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x16,0x00,0x03,0x00,0x06,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x17,0x00,0x04,0x00,0x07,0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x15,0x00,0x04,0x00,0x08,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2b,0x00,0x04,0x00,0x08,0x00,0x00,0x00,0x09,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x1c,0x00,0x04,0x00,0x0a,0x00,0x00,0x00,0x07,0x00,0x00,0x00,0x09,0x00,0x00,0x00,0x20,0x00,0x04,0x00,0x0b,0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x0a,0x00,0x00,0x00,0x3b,0x00,0x04,0x00,0x0b,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x2b,0x00,0x04,0x00,0x06,0x00,0x00,0x00,0x0d,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2b,0x00,0x04,0x00,0x06,0x00,0x00,0x00,0x0e,0x00,0x00,0x00,0x00,0x00,0x00,0xbf,0x2c,0x00,0x05,0x00,0x07,0x00,0x00,0x00,0x0f,0x00,0x00,0x00,0x0d,0x00,0x00,0x00,0x0e,0x00,0x00,0x00,0x2b,0x00,0x04,0x00,0x06,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x00,0x00,0x00,0x3f,0x2c,0x00,0x05,0x00,0x07,0x00,0x00,0x00,0x11,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x2c,0x00,0x05,0x00,0x07,0x00,0x00,0x00,0x12,0x00,0x00,0x00,0x0e,0x00,0x00,0x00,0x10,0x00,0x00,0x00,0x2c,0x00,0x06,0x00,0x0a,0x00,0x00,0x00,0x13,0x00,0x00,0x00,0x0f,0x00,0x00,0x00,0x11,0x00,0x00,0x00,0x12,0x00,0x00,0x00,0x17,0x00,0x04,0x00,0x14,0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x2b,0x00,0x04,0x00,0x08,0x00,0x00,0x00,0x15,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x1c,0x00,0x04,0x00,0x16,0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x15,0x00,0x00,0x00,0x1e,0x00,0x06,0x00,0x17,0x00,0x00,0x00,0x14,0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x16,0x00,0x00,0x00,0x16,0x00,0x00,0x00,0x20,0x00,0x04,0x00,0x18,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x17,0x00,0x00,0x00,0x3b,0x00,0x04,0x00,0x18,0x00,0x00,0x00,0x19,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x15,0x00,0x04,0x00,0x1a,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x2b,0x00,0x04,0x00,0x1a,0x00,0x00,0x00,0x1b,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x20,0x00,0x04,0x00,0x1c,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x1a,0x00,0x00,0x00,0x3b,0x00,0x04,0x00,0x1c,0x00,0x00,0x00,0x1d,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x20,0x00,0x04,0x00,0x1f,0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x07,0x00,0x00,0x00,0x2b,0x00,0x04,0x00,0x06,0x00,0x00,0x00,0x22,0x00,0x00,0x00,0x00,0x00,0x80,0x3f,0x20,0x00,0x04,0x00,0x26,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x14,0x00,0x00,0x00,0x36,0x00,0x05,0x00,0x02,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0xf8,0x00,0x02,0x00,0x05,0x00,0x00,0x00,0x3e,0x00,0x03,0x00,0x0c,0x00,0x00,0x00,0x13,0x00,0x00,0x00,0x3d,0x00,0x04,0x00,0x1a,0x00,0x00,0x00,0x1e,0x00,0x00,0x00,0x1d,0x00,0x00,0x00,0x41,0x00,0x05,0x00,0x1f,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0x1e,0x00,0x00,0x00,0x3d,0x00,0x04,0x00,0x07,0x00,0x00,0x00,0x21,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x51,0x00,0x05,0x00,0x06,0x00,0x00,0x00,0x23,0x00,0x00,0x00,0x21,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x51,0x00,0x05,0x00,0x06,0x00,0x00,0x00,0x24,0x00,0x00,0x00,0x21,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x50,0x00,0x07,0x00,0x14,0x00,0x00,0x00,0x25,0x00,0x00,0x00,0x23,0x00,0x00,0x00,0x24,0x00,0x00,0x00,0x0d,0x00,0x00,0x00,0x22,0x00,0x00,0x00,0x41,0x00,0x05,0x00,0x26,0x00,0x00,0x00,0x27,0x00,0x00,0x00,0x19,0x00,0x00,0x00,0x1b,0x00,0x00,0x00,0x3e,0x00,0x03,0x00,0x27,0x00,0x00,0x00,0x25,0x00,0x00,0x00,0xfd,0x00,0x01,0x00,0x38,0x00,0x01,0x00};
        unsigned char g_vk_fragment_shader[] = {0x03,0x02,0x23,0x07,0x00,0x00,0x01,0x00,0x07,0x00,0x0d,0x00,0x0d,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x11,0x00,0x02,0x00,0x01,0x00,0x00,0x00,0x0b,0x00,0x06,0x00,0x01,0x00,0x00,0x00,0x47,0x4c,0x53,0x4c,0x2e,0x73,0x74,0x64,0x2e,0x34,0x35,0x30,0x00,0x00,0x00,0x00,0x0e,0x00,0x03,0x00,0x00,0x00,0x00,0x00,0x01,0x00,0x00,0x00,0x0f,0x00,0x06,0x00,0x04,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x6d,0x61,0x69,0x6e,0x00,0x00,0x00,0x00,0x09,0x00,0x00,0x00,0x10,0x00,0x03,0x00,0x04,0x00,0x00,0x00,0x07,0x00,0x00,0x00,0x03,0x00,0x03,0x00,0x02,0x00,0x00,0x00,0xc2,0x01,0x00,0x00,0x04,0x00,0x09,0x00,0x47,0x4c,0x5f,0x41,0x52,0x42,0x5f,0x73,0x65,0x70,0x61,0x72,0x61,0x74,0x65,0x5f,0x73,0x68,0x61,0x64,0x65,0x72,0x5f,0x6f,0x62,0x6a,0x65,0x63,0x74,0x73,0x00,0x00,0x04,0x00,0x0a,0x00,0x47,0x4c,0x5f,0x47,0x4f,0x4f,0x47,0x4c,0x45,0x5f,0x63,0x70,0x70,0x5f,0x73,0x74,0x79,0x6c,0x65,0x5f,0x6c,0x69,0x6e,0x65,0x5f,0x64,0x69,0x72,0x65,0x63,0x74,0x69,0x76,0x65,0x00,0x00,0x04,0x00,0x08,0x00,0x47,0x4c,0x5f,0x47,0x4f,0x4f,0x47,0x4c,0x45,0x5f,0x69,0x6e,0x63,0x6c,0x75,0x64,0x65,0x5f,0x64,0x69,0x72,0x65,0x63,0x74,0x69,0x76,0x65,0x00,0x05,0x00,0x04,0x00,0x04,0x00,0x00,0x00,0x6d,0x61,0x69,0x6e,0x00,0x00,0x00,0x00,0x05,0x00,0x05,0x00,0x09,0x00,0x00,0x00,0x6f,0x75,0x74,0x43,0x6f,0x6c,0x6f,0x72,0x00,0x00,0x00,0x00,0x47,0x00,0x04,0x00,0x09,0x00,0x00,0x00,0x1e,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x13,0x00,0x02,0x00,0x02,0x00,0x00,0x00,0x21,0x00,0x03,0x00,0x03,0x00,0x00,0x00,0x02,0x00,0x00,0x00,0x16,0x00,0x03,0x00,0x06,0x00,0x00,0x00,0x20,0x00,0x00,0x00,0x17,0x00,0x04,0x00,0x07,0x00,0x00,0x00,0x06,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x20,0x00,0x04,0x00,0x08,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x07,0x00,0x00,0x00,0x3b,0x00,0x04,0x00,0x08,0x00,0x00,0x00,0x09,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0x2b,0x00,0x04,0x00,0x06,0x00,0x00,0x00,0x0a,0x00,0x00,0x00,0x00,0x00,0x80,0x3f,0x2b,0x00,0x04,0x00,0x06,0x00,0x00,0x00,0x0b,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x2c,0x00,0x07,0x00,0x07,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0x0a,0x00,0x00,0x00,0x0b,0x00,0x00,0x00,0x0b,0x00,0x00,0x00,0x0a,0x00,0x00,0x00,0x36,0x00,0x05,0x00,0x02,0x00,0x00,0x00,0x04,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x03,0x00,0x00,0x00,0xf8,0x00,0x02,0x00,0x05,0x00,0x00,0x00,0x3e,0x00,0x03,0x00,0x09,0x00,0x00,0x00,0x0c,0x00,0x00,0x00,0xfd,0x00,0x01,0x00,0x38,0x00,0x01,0x00};

        // This is not an enum. Queue families are index values
        #define QUEUE_FAMILY_INVALID -1
        #define VK_ZERO_MEMORY(ptr,size) memset((void*)ptr,0,size)

        #ifdef NDEBUG
            #define VK_CHECK(stmt) stmt
        #else
            #define VK_CHECK(stmt) \
                if (stmt != VK_SUCCESS) \
                { \
                    dmLogError("Vulkan Error (%s:%d): \n%s", __FILE__, __LINE__, #stmt); \
                }
        #endif

        const int g_enable_validation_layers = 1;
        const char* g_validation_layers[]    = {
        #ifdef __MACH__
            "MoltenVK",
        #else
            "VK_LAYER_LUNARG_standard_validation",
        #endif
            NULL
        };

        const char* g_device_extensions[] = {
            VK_KHR_SWAPCHAIN_EXTENSION_NAME,
            NULL
        };

        // TODO: Need to figure out proper frames-in-flight management..
        static const uint8_t g_vk_max_frames_in_flight = 1;

        struct QueueFamily
        {
            int32_t m_GraphicsFamily;
            int32_t m_PresentFamily;
        };

        struct SwapChainSupport
        {
            VkSurfaceCapabilitiesKHR    m_SurfaceCapabilities;
            dmArray<VkSurfaceFormatKHR> m_Formats;
            dmArray<VkPresentModeKHR>   m_PresentModes;
        };

        struct RenderPassAttachment
        {
            VkFormat      m_Format;
            VkImageLayout m_Layout;
        };

        struct RenderPass
        {
            VkRenderPass m_Handle;
        };

        struct FrameResource
        {
            VkSemaphore m_ImageAvailable;
            VkSemaphore m_RenderFinished;
            // VkFence     m_SubmitFence;
        };

        struct GPUMemory
        {
            VkDeviceMemory m_DeviceMemory;
            size_t         m_MemorySize;
        };

        struct Texture
        {
            VkImage     m_Image;
            VkImageView m_View;
            GPUMemory   m_GPUBuffer;
        };

        struct GeometryBuffer
        {
            GeometryBuffer(const VkBufferUsageFlags usage)
            : m_Usage(usage)
            , m_Handle(0) {
                m_GPUBuffer.m_DeviceMemory = 0;
                m_GPUBuffer.m_MemorySize   = 0;
            }

            const VkBufferUsageFlags m_Usage;
            VkBuffer                 m_Handle;
            GPUMemory                m_GPUBuffer;
        };

        struct ShaderProgram
        {
            VkShaderModule    m_Handle;
            uint64_t          m_Hash;
        };

        struct Program
        {
            Program(const ShaderProgram* vertexProgram, const ShaderProgram* fragmentProgram)
            : m_VertexProgram(vertexProgram)
            , m_FragmentProgram(fragmentProgram) {}

            const ShaderProgram*            m_VertexProgram;
            const ShaderProgram*            m_FragmentProgram;
            VkPipelineShaderStageCreateInfo m_ShaderStages[2];
            dmArray<uint16_t>               m_LayoutBindingsIndices;
        };

        struct Pipeline
        {
            VkPipeline       m_Handle;
            VkPipelineLayout m_Layout;
        };

        typedef GeometryBuffer VertexBuffer;
        typedef GeometryBuffer IndexBuffer;

        struct Context
        {
            uint8_t                        m_CurrentFrameImageIx;
            uint8_t                        m_CurrentFrameInFlight;

            VkInstance                     m_Instance;
            VkPhysicalDevice               m_PhysicalDevice;
            VkDevice                       m_LogicalDevice;
            VkQueue                        m_PresentQueue;
            VkQueue                        m_GraphicsQueue;
            VkSwapchainKHR                 m_SwapChain;
            VkFormat                       m_SwapChainImageFormat;
            VkExtent2D                     m_SwapChainImageExtent;
            VkDescriptorPool               m_DescriptorPool;
            VkCommandPool                  m_MainCommandPool;
            VkSurfaceKHR                   m_Surface;
            VkApplicationInfo              m_ApplicationInfo;
            VkDebugUtilsMessengerEXT       m_DebugCallback;
            VkViewport                     m_MainViewport;

            RenderPass                     m_MainRenderPass;
            Texture                        m_MainDepthBuffer;
            FrameResource                  m_FrameResources[g_vk_max_frames_in_flight];

            dmArray<VkCommandBuffer>                   m_MainCommandBuffers;
            dmArray<VkFramebuffer>                     m_MainFramebuffers;
            dmArray<VkImage>                           m_SwapChainImages;
            dmArray<VkImageView>                       m_SwapChainImageViews;
            dmArray<VkExtensionProperties>             m_Extensions;
            dmArray<VkVertexInputAttributeDescription> m_VertexStreamDescriptors;
            dmArray<VkDescriptorSetLayoutBinding>      m_DescriptorSetLayoutBindings;

            dmHashTable64<Pipeline>      m_PipelineCache;
        } g_vk_context;

        // This functions is invoked by the vulkan layer whenever
        // it has something to say, which can be info, warnings, errors and such.
        // Only used in debug.
        static VKAPI_ATTR VkBool32 VKAPI_CALL g_vk_debug_callback(
            VkDebugUtilsMessageSeverityFlagBitsEXT      messageSeverity,
            VkDebugUtilsMessageTypeFlagsEXT             messageType,
            const VkDebugUtilsMessengerCallbackDataEXT* pCallbackData,
            void* pUserData)
        {
            dmLogInfo("Validation Layer: %s", pCallbackData->pMessage);

            return VK_FALSE;
        }

        static inline uint32_t CountStringArray(const char** arr)
        {
            int str_c = -1;
            while(arr[++str_c]);
            return str_c;
        }

        static inline bool IsInDeviceExtensionList(const char* ext_to_find)
        {
            int g_ext_c = -1;
            while(g_device_extensions[++g_ext_c])
            {
                if (dmStrCaseCmp(ext_to_find, g_device_extensions[g_ext_c]) == 0)
                {
                    return true;
                }
            }

            return false;
        }

        void SetInstanceExtensionList()
        {
            uint32_t extension_count             = 0;
            VkExtensionProperties* extension_ptr = 0;

            vkEnumerateInstanceExtensionProperties(0, &extension_count, 0);

            g_vk_context.m_Extensions.SetCapacity(extension_count);
            g_vk_context.m_Extensions.SetSize(extension_count);

            extension_ptr = g_vk_context.m_Extensions.Begin();

            vkEnumerateInstanceExtensionProperties(0, &extension_count, extension_ptr);

            for(unsigned int i=0; i < extension_count; i++ )
            {
                dmLogInfo("Extension Enabled: %s", g_vk_context.m_Extensions[i].extensionName);
            }
        }

        bool GetValidationSupport()
        {
            uint32_t layer_count          = 0;
            VkLayerProperties* layer_list = 0;

            vkEnumerateInstanceLayerProperties(&layer_count, 0);

            layer_list = new VkLayerProperties[layer_count];

            vkEnumerateInstanceLayerProperties(&layer_count, layer_list);

            bool all_layers_found = true;

            for(uint32_t ext=0;;ext++)
            {
                bool layer_found = false;

                if (g_validation_layers[ext] == NULL)
                {
                    break;
                }

                for(uint32_t layer_index=0; layer_index < layer_count; ++layer_index)
                {
                    if (strcmp(layer_list[layer_index].layerName, g_validation_layers[ext]) == 0)
                    {
                        layer_found = true;
                        break;
                    }
                }

                if (!layer_found)
                {
                    dmLogError("Validation Layer '%s' is not supported", g_validation_layers[ext]);
                    all_layers_found = false;
                }
            }

            delete[] layer_list;

            return all_layers_found;
        }

        bool GetRequiredInstanceExtensions(dmArray<const char*>& extensionListOut)
        {
            uint32_t extension_count                 = 0;
            VkExtensionProperties* vk_extension_list = 0;

            if (vkEnumerateInstanceExtensionProperties(NULL, &extension_count, NULL) != VK_SUCCESS)
            {
                return false;
            }

            vk_extension_list = new VkExtensionProperties[extension_count];

            extensionListOut.SetCapacity(extension_count);

            for (uint32_t i = 0; i < extension_count; i++)
            {
                if (strcmp(VK_KHR_SURFACE_EXTENSION_NAME, vk_extension_list[i].extensionName) == 0)
                {
                    extensionListOut.Push(VK_KHR_SURFACE_EXTENSION_NAME);
                }

        // need similar ifdefs for other platforms here..
        #if defined(__MACH__)
                if (strcmp(VK_MVK_MACOS_SURFACE_EXTENSION_NAME, vk_extension_list[i].extensionName) == 0)
                {
                    extensionListOut.Push(VK_MVK_MACOS_SURFACE_EXTENSION_NAME);
                }
        #endif

                if (strcmp(VK_EXT_DEBUG_UTILS_EXTENSION_NAME, vk_extension_list[i].extensionName) == 0)
                {
                    if (g_enable_validation_layers) {
                        extensionListOut.Push(VK_EXT_DEBUG_UTILS_EXTENSION_NAME);
                    }
                }
            }

            delete[] vk_extension_list;

            return true;
        }

        // All GPU operations are pushed to various queues. The physical device can have multiple
        // queues with different properties supported, so we need to find a combination of queues
        // that will work for our needs. Note that the present queue might not be the same queue as the
        // graphics queue. The graphics queue support is needed to do graphics operations at all.
        QueueFamily GetQueueFamily(VkPhysicalDevice device)
        {
            QueueFamily vk_family_selected = { QUEUE_FAMILY_INVALID, QUEUE_FAMILY_INVALID };
            uint32_t queue_family_count    = 0;

            vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, 0);

            if (queue_family_count == 0)
            {
                return vk_family_selected;
            }

            VkQueueFamilyProperties* vk_family_list = new VkQueueFamilyProperties[queue_family_count];
            vkGetPhysicalDeviceQueueFamilyProperties(device, &queue_family_count, vk_family_list);

            for (uint32_t i=0; i < queue_family_count; i++)
            {
                VkQueueFamilyProperties candidate = vk_family_list[i];

                if (candidate.queueCount > 0 && candidate.queueFlags & VK_QUEUE_GRAPHICS_BIT)
                {
                    vk_family_selected.m_GraphicsFamily = i;
                }

                uint32_t present_support = 0;
                vkGetPhysicalDeviceSurfaceSupportKHR(device, i, g_vk_context.m_Surface, &present_support);

                if (candidate.queueCount && present_support)
                {
                    vk_family_selected.m_PresentFamily = i;
                }

                if (vk_family_selected.m_GraphicsFamily >= 0 && vk_family_selected.m_PresentFamily >= 0)
                {
                    break;
                }
            }

            delete[] vk_family_list;

            return vk_family_selected;
        }

        bool IsExtensionsSupported(VkPhysicalDevice device)
        {
            uint32_t ext_available_count = 0;
            int32_t  ext_to_enable_count = 0;

            while(g_device_extensions[++ext_to_enable_count]);

            vkEnumerateDeviceExtensionProperties(device, 0, &ext_available_count, 0);

            VkExtensionProperties* ext_properties = new VkExtensionProperties[ext_available_count];

            vkEnumerateDeviceExtensionProperties(device, 0, &ext_available_count, ext_properties);

            for (uint32_t i=0; i < ext_available_count; i++)
            {
                VkExtensionProperties ext_property = ext_properties[i];

                if (IsInDeviceExtensionList(ext_property.extensionName))
                {
                    ext_to_enable_count--;
                }
            }

            delete[] ext_properties;

            return ext_to_enable_count == 0;
        }

        bool SetLogicalDevice()
        {
            QueueFamily vk_queue_family = GetQueueFamily(g_vk_context.m_PhysicalDevice);

            // NOTE: Different queues can have different priority from [0..1], but
            //       we only have a single queue right now so set to 1.0f
            float queue_priority    = 1.0f;
            int queue_family_set[2] = { QUEUE_FAMILY_INVALID, QUEUE_FAMILY_INVALID };
            int queue_family_c      = 0;

            VkDeviceQueueCreateInfo queue_create_info_list[2];
            VK_ZERO_MEMORY(queue_create_info_list, sizeof(queue_create_info_list));

            if (vk_queue_family.m_PresentFamily != vk_queue_family.m_GraphicsFamily)
            {
                queue_family_set[0] = vk_queue_family.m_PresentFamily;
                queue_family_set[1] = vk_queue_family.m_GraphicsFamily;
            }
            else
            {
                queue_family_set[0] = vk_queue_family.m_PresentFamily;
            }

            while(queue_family_set[queue_family_c] != QUEUE_FAMILY_INVALID)
            {
                int queue_family_index                         = queue_family_set[queue_family_c];
                VkDeviceQueueCreateInfo& vk_queue_create_info = queue_create_info_list[queue_family_c];

                vk_queue_create_info.sType            = VK_STRUCTURE_TYPE_DEVICE_QUEUE_CREATE_INFO;
                vk_queue_create_info.queueFamilyIndex = queue_family_index;
                vk_queue_create_info.queueCount       = 1;
                vk_queue_create_info.pQueuePriorities = &queue_priority;

                queue_family_c++;
            }

            if (!queue_family_c)
            {
                return false;
            }

            VkDeviceCreateInfo vk_device_create_info;
            VK_ZERO_MEMORY(&vk_device_create_info,sizeof(vk_device_create_info));

            vk_device_create_info.sType                   = VK_STRUCTURE_TYPE_DEVICE_CREATE_INFO;
            vk_device_create_info.pQueueCreateInfos       = queue_create_info_list;
            vk_device_create_info.queueCreateInfoCount    = queue_family_c;
            vk_device_create_info.pEnabledFeatures        = 0;
            vk_device_create_info.enabledExtensionCount   = CountStringArray(g_device_extensions);
            vk_device_create_info.ppEnabledExtensionNames = g_device_extensions;

            if (g_enable_validation_layers)
            {
                vk_device_create_info.enabledLayerCount   = CountStringArray(g_validation_layers);
                vk_device_create_info.ppEnabledLayerNames = g_validation_layers;
            }
            else
            {
                vk_device_create_info.enabledLayerCount = 0;
            }

            if (vkCreateDevice(g_vk_context.m_PhysicalDevice, &vk_device_create_info, 0, &g_vk_context.m_LogicalDevice) != VK_SUCCESS)
            {
                return false;
            }

            vkGetDeviceQueue(g_vk_context.m_LogicalDevice, vk_queue_family.m_GraphicsFamily, 0, &g_vk_context.m_GraphicsQueue);
            vkGetDeviceQueue(g_vk_context.m_LogicalDevice, vk_queue_family.m_PresentFamily, 0, &g_vk_context.m_PresentQueue);

            return true;
        }

        // Extract all swap chain formats and present modes. We use these tables to select the most suitable modes later.
        void GetSwapChainSupport(VkPhysicalDevice device, SwapChainSupport& swapChain)
        {
            uint32_t format_count        = 0;
            uint32_t present_modes_count = 0;

            vkGetPhysicalDeviceSurfaceCapabilitiesKHR(device, g_vk_context.m_Surface, &swapChain.m_SurfaceCapabilities);
            vkGetPhysicalDeviceSurfaceFormatsKHR(device, g_vk_context.m_Surface, &format_count, 0);
            vkGetPhysicalDeviceSurfacePresentModesKHR(device, g_vk_context.m_Surface, &present_modes_count, 0);

            if (format_count > 0)
            {
                swapChain.m_Formats.SetCapacity(format_count);
                swapChain.m_Formats.SetSize(format_count);
                vkGetPhysicalDeviceSurfaceFormatsKHR(device, g_vk_context.m_Surface, &format_count, swapChain.m_Formats.Begin());
            }

            if (present_modes_count > 0)
            {
                swapChain.m_PresentModes.SetCapacity(present_modes_count);
                swapChain.m_PresentModes.SetSize(present_modes_count);
                vkGetPhysicalDeviceSurfacePresentModesKHR(device, g_vk_context.m_Surface, &present_modes_count, swapChain.m_PresentModes.Begin());
            }
        }

        bool IsDeviceCompatible(VkPhysicalDevice device)
        {
            QueueFamily vk_queue_family  = GetQueueFamily(device);
            bool is_extensions_supported = IsExtensionsSupported(device);
            bool is_swap_chain_supported = false;

            if (is_extensions_supported)
            {
                SwapChainSupport swap_chain_support;
                GetSwapChainSupport(device, swap_chain_support);

                is_swap_chain_supported  = swap_chain_support.m_Formats.Size() > 0;
                is_swap_chain_supported &= swap_chain_support.m_PresentModes.Size() > 0;
            }

            return vk_queue_family.m_GraphicsFamily != QUEUE_FAMILY_INVALID &&
                   vk_queue_family.m_PresentFamily  != QUEUE_FAMILY_INVALID &&
                   is_extensions_supported && is_swap_chain_supported;
        }

        bool SetPhysicalDevice()
        {
            VkPhysicalDevice vk_selected_device = VK_NULL_HANDLE;
            uint32_t device_count               = 0;

            // Get number of present devices
            vkEnumeratePhysicalDevices(g_vk_context.m_Instance, &device_count, 0);

            if (device_count == 0)
            {
                return false;
            }

            VkPhysicalDevice* vk_device_list = new VkPhysicalDevice[device_count];

            vkEnumeratePhysicalDevices(g_vk_context.m_Instance, &device_count, vk_device_list);

            // Iterate list of possible physical devices and pick the first that is suitable
            // There can be many GPUs present, but we can only support one.
            for (uint32_t i=0; i < device_count; i++)
            {
                VkPhysicalDevice vk_candidate = vk_device_list[i];

                if (IsDeviceCompatible(vk_candidate))
                {
                    vk_selected_device = vk_candidate;
                    break;
                }
            }

            delete[] vk_device_list;

            if (vk_selected_device == VK_NULL_HANDLE)
            {
                return false;
            }

            g_vk_context.m_PhysicalDevice = vk_selected_device;

            return true;
        }

        VkSurfaceFormatKHR GetSuitableSwapChainFormat(dmArray<VkSurfaceFormatKHR>& availableFormats)
        {
            if (availableFormats.Size() > 0)
            {
                VkSurfaceFormatKHR format_head = availableFormats[0];

                VkSurfaceFormatKHR format_wanted;
                format_wanted.format     = VK_FORMAT_B8G8R8A8_UNORM;
                format_wanted.colorSpace = VK_COLOR_SPACE_SRGB_NONLINEAR_KHR;

                // The window surface has no preferred format so we can return whatever we want here.
                if (format_head.format == VK_FORMAT_UNDEFINED)
                {
                    return format_wanted;
                }

                // Try to find a suitable format candidate that matches our
                // suggested format. If not, just return the first.
                // NOTE: We could get the "closest" format here aswell,
                //       but not sure how to rank formats really.
                for (uint32_t i=0; i < availableFormats.Size(); i++)
                {
                    VkSurfaceFormatKHR format_candidate = availableFormats[i];

                    if (format_candidate.format     == format_wanted.format &&
                        format_candidate.colorSpace == format_wanted.colorSpace)
                    {
                        return format_candidate;
                    }
                }

                return format_head;
            }
            else
            {
                VkSurfaceFormatKHR format_null;
                VK_ZERO_MEMORY(&format_null, sizeof(format_null));
                return format_null;
            }
        }

        // Present modes: https://www.khronos.org/registry/vulkan/specs/1.1-extensions/man/html/VkPresentModeKHR.html
        VkPresentModeKHR GetSuitablePresentMode(dmArray<VkPresentModeKHR>& presentModes)
        {
            VkPresentModeKHR mode_most_suitable = VK_PRESENT_MODE_FIFO_KHR;

            for (uint32_t i=0; i < presentModes.Size(); i++)
            {
                VkPresentModeKHR mode_candidate = presentModes[i];

                if (mode_candidate == VK_PRESENT_MODE_MAILBOX_KHR)
                {
                    return mode_candidate;
                }
                else if (mode_candidate == VK_PRESENT_MODE_IMMEDIATE_KHR)
                {
                    mode_most_suitable = mode_candidate;
                }
            }

            return mode_most_suitable;
        }

        VkExtent2D GetSwapChainExtent(VkSurfaceCapabilitiesKHR capabilities, uint32_t wantedWidth, uint32_t wantedHeight)
        {
            VkExtent2D extent_current = capabilities.currentExtent;

            if (extent_current.width == 0xFFFFFFFF || extent_current.width == 0xFFFFFFFF)
            {
                VkExtent2D extent_wanted;
                // Clamp swap buffer extent to our wanted width / height.
                // TODO: Figure out how will this change when running in fullscreen mode?
                extent_wanted.width  = dmMath::Clamp<uint32_t>(wantedWidth, 0, extent_current.width);
                extent_wanted.height = dmMath::Clamp<uint32_t>(wantedHeight, 0, extent_current.height);

                return extent_wanted;
            }
            else
            {
                return extent_current;
            }
        }

        bool CreateSwapChain(uint32_t wantedWidth, uint32_t wantedHeight)
        {
            SwapChainSupport swap_chain_support;
            GetSwapChainSupport(g_vk_context.m_PhysicalDevice, swap_chain_support);

            VkSurfaceFormatKHR swap_chain_format       = GetSuitableSwapChainFormat(swap_chain_support.m_Formats);
            VkPresentModeKHR   swap_chain_present_mode = GetSuitablePresentMode(swap_chain_support.m_PresentModes);
            VkExtent2D         swap_chain_extent       = GetSwapChainExtent(swap_chain_support.m_SurfaceCapabilities, wantedWidth, wantedHeight);

            // From the docs:
            // ==============
            // minImageCount: the minimum number of images the specified device supports for a swapchain created for the surface, and will be at least one.
            // maxImageCount: the maximum number of images the specified device supports for a swapchain created for the surface,
            //  and will be either 0, or greater than or equal to minImageCount. A value of 0 means that there is no limit on the number of images,
            //  though there may be limits related to the total amount of memory used by presentable images.

            // The +1 here is to add a little bit of headroom, from vulkan-tutorial.com:
            // "we may sometimes have to wait on the driver to complete internal operations
            // before we can acquire another image to render to"
            uint32_t swap_chain_image_count = swap_chain_support.m_SurfaceCapabilities.minImageCount + 1;

            if (swap_chain_support.m_SurfaceCapabilities.maxImageCount > 0)
            {
                swap_chain_image_count = dmMath::Clamp(swap_chain_image_count,
                    swap_chain_support.m_SurfaceCapabilities.minImageCount,
                    swap_chain_support.m_SurfaceCapabilities.maxImageCount);
            }

            VkSwapchainCreateInfoKHR swap_chain_create_info;
            VK_ZERO_MEMORY(&swap_chain_create_info,sizeof(swap_chain_create_info));

            swap_chain_create_info.sType   = VK_STRUCTURE_TYPE_SWAPCHAIN_CREATE_INFO_KHR;
            swap_chain_create_info.surface = g_vk_context.m_Surface;

            swap_chain_create_info.minImageCount    = swap_chain_image_count;
            swap_chain_create_info.imageFormat      = swap_chain_format.format;
            swap_chain_create_info.imageColorSpace  = swap_chain_format.colorSpace;
            swap_chain_create_info.imageExtent      = swap_chain_extent;

            // imageArrayLayers: the number of views in a multiview/stereo surface.
            // For non-stereoscopic-3D applications, this value is 1
            swap_chain_create_info.imageArrayLayers = 1;
            swap_chain_create_info.imageUsage       = VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT;

            QueueFamily queue_family = GetQueueFamily(g_vk_context.m_PhysicalDevice);

            // Move queue indices over to uint32_t array
            uint32_t queue_family_indices[2] = {queue_family.m_GraphicsFamily, queue_family.m_PresentFamily};

            // If we have different queues for the different types, we just stick with
            // concurrent mode. An option here would be to do ownership transfers:
            // https://www.khronos.org/registry/vulkan/specs/1.1-extensions/html/vkspec.html#synchronization-queue-transfers
            if (queue_family.m_GraphicsFamily != queue_family.m_PresentFamily)
            {
                swap_chain_create_info.imageSharingMode      = VK_SHARING_MODE_CONCURRENT;
                swap_chain_create_info.queueFamilyIndexCount = 2;
                swap_chain_create_info.pQueueFamilyIndices   = queue_family_indices;
            }
            else
            {
                // This mode is best for performance and can be used with no penality
                // if we are using the same queue for everything.
                swap_chain_create_info.imageSharingMode      = VK_SHARING_MODE_EXCLUSIVE;
                swap_chain_create_info.queueFamilyIndexCount = 0;
                swap_chain_create_info.pQueueFamilyIndices   = 0;
            }

            // The preTransform field can be used to rotate the swap chain when presenting
            swap_chain_create_info.preTransform   = swap_chain_support.m_SurfaceCapabilities.currentTransform;
            swap_chain_create_info.compositeAlpha = VK_COMPOSITE_ALPHA_OPAQUE_BIT_KHR;
            swap_chain_create_info.presentMode    = swap_chain_present_mode;
            swap_chain_create_info.clipped        = VK_TRUE;
            swap_chain_create_info.oldSwapchain   = VK_NULL_HANDLE;

            if (vkCreateSwapchainKHR(g_vk_context.m_LogicalDevice,
                &swap_chain_create_info, 0, &g_vk_context.m_SwapChain) != VK_SUCCESS)
            {
                return false;
            }

            vkGetSwapchainImagesKHR(g_vk_context.m_LogicalDevice, g_vk_context.m_SwapChain, &swap_chain_image_count, 0);

            g_vk_context.m_SwapChainImages.SetCapacity(swap_chain_image_count);
            g_vk_context.m_SwapChainImages.SetSize(swap_chain_image_count);

            vkGetSwapchainImagesKHR(g_vk_context.m_LogicalDevice, g_vk_context.m_SwapChain, &swap_chain_image_count, g_vk_context.m_SwapChainImages.Begin());

            g_vk_context.m_SwapChainImageFormat = swap_chain_format.format;
            g_vk_context.m_SwapChainImageExtent = swap_chain_extent;

            return true;
        }

        bool CreateSwapChainImageViews()
        {
            uint32_t num_views = g_vk_context.m_SwapChainImages.Size();

            g_vk_context.m_SwapChainImageViews.SetCapacity(num_views);
            g_vk_context.m_SwapChainImageViews.SetSize(num_views);

            for (uint32_t i=0; i < num_views; i++)
            {
                VkImageViewCreateInfo create_info_image_view;
                VK_ZERO_MEMORY(&create_info_image_view, sizeof(create_info_image_view));

                create_info_image_view.sType        = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                create_info_image_view.image        = g_vk_context.m_SwapChainImages[i];
                create_info_image_view.viewType     = VK_IMAGE_VIEW_TYPE_2D;
                create_info_image_view.format       = g_vk_context.m_SwapChainImageFormat;
                create_info_image_view.components.r = VK_COMPONENT_SWIZZLE_IDENTITY;
                create_info_image_view.components.g = VK_COMPONENT_SWIZZLE_IDENTITY;
                create_info_image_view.components.b = VK_COMPONENT_SWIZZLE_IDENTITY;
                create_info_image_view.components.a = VK_COMPONENT_SWIZZLE_IDENTITY;

                create_info_image_view.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
                create_info_image_view.subresourceRange.baseMipLevel   = 0;
                create_info_image_view.subresourceRange.levelCount     = 1;
                create_info_image_view.subresourceRange.baseArrayLayer = 0;
                create_info_image_view.subresourceRange.layerCount     = 1;

                if (vkCreateImageView(g_vk_context.m_LogicalDevice,
                    &create_info_image_view, 0, &g_vk_context.m_SwapChainImageViews[i]) != VK_SUCCESS)
                {
                    g_vk_context.m_SwapChainImageViews.SetCapacity(0);
                    g_vk_context.m_SwapChainImageViews.SetSize(0);
                    return false;
                }
            }

            return true;
        }

        VkFormat GetSupportedFormat(VkFormat* formatCandidates, uint32_t numFormatCandidates,
            VkImageTiling tilingType, VkFormatFeatureFlags formatFlags)
        {
            #define HAS_FLAG(v,flag) ((v & flag) == flag)

            for (uint32_t i=0; i < numFormatCandidates; i++)
            {
                VkFormatProperties format_properties;
                VkFormat formatCandidate = formatCandidates[i];

                vkGetPhysicalDeviceFormatProperties(g_vk_context.m_PhysicalDevice, formatCandidate, &format_properties);

                if ((tilingType == VK_IMAGE_TILING_LINEAR && HAS_FLAG(format_properties.linearTilingFeatures, formatFlags)) ||
                    (tilingType == VK_IMAGE_TILING_OPTIMAL && HAS_FLAG(format_properties.optimalTilingFeatures, formatFlags)))
                {
                    return formatCandidate;
                }
            }

            #undef HAS_FLAG

            return VK_FORMAT_UNDEFINED;
        }

        VkFormat GetSuitableDepthFormat()
        {
            // Depth formats are optional, so we need to query
            // what available formats we have.
            VkFormat format_depth_list[] = {
                VK_FORMAT_D32_SFLOAT_S8_UINT,
                VK_FORMAT_D32_SFLOAT,
                VK_FORMAT_D24_UNORM_S8_UINT,
                VK_FORMAT_D16_UNORM_S8_UINT,
                VK_FORMAT_D16_UNORM
            };

            return GetSupportedFormat(format_depth_list,
                sizeof(format_depth_list) / sizeof(VkFormat), VK_IMAGE_TILING_OPTIMAL,
                VK_FORMAT_FEATURE_DEPTH_STENCIL_ATTACHMENT_BIT);
        }

        RenderPass CreateRenderPass(RenderPassAttachment* colorAttachments, uint8_t numColorAttachments, RenderPassAttachment* depthStencilAttachment)
        {
            RenderPass rp;

            uint8_t num_attachments  = numColorAttachments + (depthStencilAttachment ? 1 : 0);
            VkAttachmentDescription* vk_attachment_desc    = new VkAttachmentDescription[num_attachments];
            VkAttachmentReference* vk_attachment_color_ref = new VkAttachmentReference[numColorAttachments];
            VkAttachmentReference vk_attachment_depth_ref;

            for (uint16_t i=0; i < numColorAttachments; i++)
            {
                VkAttachmentDescription& attachment_color = vk_attachment_desc[i];

                attachment_color.format         = colorAttachments[i].m_Format;
                attachment_color.samples        = VK_SAMPLE_COUNT_1_BIT;
                attachment_color.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
                attachment_color.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
                attachment_color.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_DONT_CARE;
                attachment_color.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachment_color.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
                attachment_color.finalLayout    = colorAttachments[i].m_Layout;

                VkAttachmentReference& ref = vk_attachment_color_ref[i];
                ref.attachment = i;
                ref.layout     = VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL;
            }

            if (depthStencilAttachment)
            {
                VkAttachmentDescription& attachment_depth = vk_attachment_desc[numColorAttachments];

                attachment_depth.format         = depthStencilAttachment->m_Format;
                attachment_depth.samples        = VK_SAMPLE_COUNT_1_BIT;
                attachment_depth.loadOp         = VK_ATTACHMENT_LOAD_OP_CLEAR;
                attachment_depth.storeOp        = VK_ATTACHMENT_STORE_OP_STORE;
                attachment_depth.stencilLoadOp  = VK_ATTACHMENT_LOAD_OP_CLEAR;
                attachment_depth.stencilStoreOp = VK_ATTACHMENT_STORE_OP_DONT_CARE;
                attachment_depth.initialLayout  = VK_IMAGE_LAYOUT_UNDEFINED;
                attachment_depth.finalLayout    = depthStencilAttachment->m_Layout;

                vk_attachment_depth_ref.attachment = numColorAttachments;
                vk_attachment_depth_ref.layout     = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;
            }

            VkSubpassDependency sub_pass_dependency;
            VK_ZERO_MEMORY(&sub_pass_dependency,sizeof(sub_pass_dependency));

            sub_pass_dependency.srcSubpass    = VK_SUBPASS_EXTERNAL;
            sub_pass_dependency.dstSubpass    = 0;
            sub_pass_dependency.srcStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            sub_pass_dependency.srcAccessMask = 0;
            sub_pass_dependency.dstStageMask  = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;
            sub_pass_dependency.dstAccessMask = VK_ACCESS_COLOR_ATTACHMENT_READ_BIT | VK_ACCESS_COLOR_ATTACHMENT_WRITE_BIT;

            VkSubpassDescription sub_pass_description;
            VK_ZERO_MEMORY(&sub_pass_description, sizeof(sub_pass_description));

            sub_pass_description.pipelineBindPoint       = VK_PIPELINE_BIND_POINT_GRAPHICS;
            sub_pass_description.colorAttachmentCount    = numColorAttachments;
            sub_pass_description.pColorAttachments       = vk_attachment_color_ref;
            sub_pass_description.pDepthStencilAttachment = depthStencilAttachment ? &vk_attachment_depth_ref : 0;

            VkRenderPassCreateInfo render_pass_create_info;
            VK_ZERO_MEMORY(&render_pass_create_info, sizeof(render_pass_create_info));

            render_pass_create_info.sType           = VK_STRUCTURE_TYPE_RENDER_PASS_CREATE_INFO;
            render_pass_create_info.attachmentCount = num_attachments;
            render_pass_create_info.pAttachments    = vk_attachment_desc;
            render_pass_create_info.subpassCount    = 1;
            render_pass_create_info.pSubpasses      = &sub_pass_description;
            render_pass_create_info.dependencyCount = 1;
            render_pass_create_info.pDependencies   = &sub_pass_dependency;

            VK_CHECK(vkCreateRenderPass(g_vk_context.m_LogicalDevice, &render_pass_create_info, 0, &rp.m_Handle));

            delete[] vk_attachment_desc;
            delete[] vk_attachment_color_ref;

            return rp;
        }

        void CreateMainRenderPass()
        {
            VkFormat format_color = g_vk_context.m_SwapChainImageFormat;
            VkFormat format_depth = GetSuitableDepthFormat();

            RenderPassAttachment attachments[2];

            attachments[0].m_Format = format_color;
            attachments[0].m_Layout = VK_IMAGE_LAYOUT_PRESENT_SRC_KHR;

            attachments[1].m_Format = format_depth;
            attachments[1].m_Layout = VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL;

            g_vk_context.m_MainRenderPass = CreateRenderPass(attachments,1,&attachments[1]);
        }

        void CreateCommandBuffer(VkCommandBuffer* commandBuffersOut, uint8_t numBuffersToCreate, VkCommandPool commandPool)
        {
            VkCommandBufferAllocateInfo buffers_allocate_info;
            VK_ZERO_MEMORY(&buffers_allocate_info,sizeof(VkCommandBufferAllocateInfo));

            buffers_allocate_info.sType              = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
            buffers_allocate_info.commandPool        = commandPool;
            buffers_allocate_info.level              = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
            buffers_allocate_info.commandBufferCount = (uint32_t) numBuffersToCreate;

            VK_CHECK(vkAllocateCommandBuffers(g_vk_context.m_LogicalDevice, &buffers_allocate_info, commandBuffersOut));
        }

        int32_t FindMemoryTypeIndex(uint32_t typeFilter, VkMemoryPropertyFlags propertyFlag)
        {
            #define HAS_FLAG(v,flag) ((v & flag) == flag)

            VkPhysicalDeviceMemoryProperties vk_memory_props;
            vkGetPhysicalDeviceMemoryProperties(g_vk_context.m_PhysicalDevice, &vk_memory_props);

            for (uint32_t i = 0; i < vk_memory_props.memoryTypeCount; i++)
            {
                if ((typeFilter & (1 << i)) && HAS_FLAG(vk_memory_props.memoryTypes[i].propertyFlags,propertyFlag))
                {
                    return i;
                }
            }

            #undef HAS_FLAG

            return -1;
        }

        void TransitionImageLayout(VkImage image, VkFormat format, VkImageLayout oldLayout, VkImageLayout newLayout)
        {
            VkCommandBuffer vk_command_buffer;
            CreateCommandBuffer(&vk_command_buffer, 1, g_vk_context.m_MainCommandPool);

            VkCommandBufferBeginInfo vk_command_buffer_begin_info;
            VK_ZERO_MEMORY(&vk_command_buffer_begin_info, sizeof(VkCommandBufferBeginInfo));

            vk_command_buffer_begin_info.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vk_command_buffer_begin_info.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;

            VK_CHECK(vkBeginCommandBuffer(vk_command_buffer, &vk_command_buffer_begin_info));

            VkImageMemoryBarrier vk_memory_barrier;
            VK_ZERO_MEMORY(&vk_memory_barrier, sizeof(vk_memory_barrier));

            vk_memory_barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
            vk_memory_barrier.oldLayout                       = oldLayout;
            vk_memory_barrier.newLayout                       = newLayout;
            vk_memory_barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            vk_memory_barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
            vk_memory_barrier.image                           = image;
            vk_memory_barrier.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
            vk_memory_barrier.subresourceRange.baseMipLevel   = 0;
            vk_memory_barrier.subresourceRange.levelCount     = 1;
            vk_memory_barrier.subresourceRange.baseArrayLayer = 0;
            vk_memory_barrier.subresourceRange.layerCount     = 1;

            if (newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            {
                vk_memory_barrier.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;

                // Check if requested image transition is stencil
                if (format == VK_FORMAT_D32_SFLOAT_S8_UINT || format == VK_FORMAT_D24_UNORM_S8_UINT)
                {
                    vk_memory_barrier.subresourceRange.aspectMask |= VK_IMAGE_ASPECT_STENCIL_BIT;
                }
            }

            VkPipelineStageFlags vk_source_stage      = VK_IMAGE_LAYOUT_UNDEFINED;
            VkPipelineStageFlags vk_destination_stage = VK_IMAGE_LAYOUT_UNDEFINED;

            // NOTE: Not sure how this works to be honest.
            if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL)
            {
                vk_memory_barrier.srcAccessMask = 0;
                vk_memory_barrier.dstAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;

                vk_source_stage      = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                vk_destination_stage = VK_PIPELINE_STAGE_TRANSFER_BIT;
            }
            else if (oldLayout == VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL && newLayout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
            {
                vk_memory_barrier.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
                vk_memory_barrier.dstAccessMask = VK_ACCESS_SHADER_READ_BIT;

                vk_source_stage      = VK_PIPELINE_STAGE_TRANSFER_BIT;
                vk_destination_stage = VK_PIPELINE_STAGE_FRAGMENT_SHADER_BIT;
            }
            else if (oldLayout == VK_IMAGE_LAYOUT_UNDEFINED && newLayout == VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL)
            {
                vk_memory_barrier.srcAccessMask = 0;
                vk_memory_barrier.dstAccessMask = VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_READ_BIT | VK_ACCESS_DEPTH_STENCIL_ATTACHMENT_WRITE_BIT;

                vk_source_stage      = VK_PIPELINE_STAGE_TOP_OF_PIPE_BIT;
                vk_destination_stage = VK_PIPELINE_STAGE_EARLY_FRAGMENT_TESTS_BIT;
            }
            else
            {
                assert(0 && "Invalid arguments passed to TransitionImageLayout");
            }

            vkCmdPipelineBarrier(
                vk_command_buffer,
                vk_source_stage,
                vk_destination_stage,
                0,
                0, 0,
                0, 0,
                1, &vk_memory_barrier
            );

            VK_CHECK(vkEndCommandBuffer(vk_command_buffer));

            VkSubmitInfo vk_submit_info;
            VK_ZERO_MEMORY(&vk_submit_info, sizeof(VkSubmitInfo));

            vk_submit_info.sType              = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            vk_submit_info.commandBufferCount = 1;
            vk_submit_info.pCommandBuffers    = &vk_command_buffer;

            vkQueueSubmit(g_vk_context.m_GraphicsQueue, 1, &vk_submit_info, VK_NULL_HANDLE);
            vkQueueWaitIdle(g_vk_context.m_GraphicsQueue);

            vkFreeCommandBuffers(g_vk_context.m_LogicalDevice, g_vk_context.m_MainCommandPool, 1, &vk_command_buffer);
        }

        static inline void FillVertexInputAttributeDesc(HVertexDeclaration vertexDeclaration, VkVertexInputAttributeDescription* inputs)
        {
            for (uint32_t i = 0; i < vertexDeclaration->m_StreamCount; ++i)
            {
                inputs[i] = g_vk_context.m_VertexStreamDescriptors[vertexDeclaration->m_Streams[i].m_DescriptorIndex];
            }
        }

        static inline void FillDescriptorSetLayoutBindings(VkDescriptorSetLayoutBinding* input, Program* program)
        {
            for (uint16_t i = 0; i < program->m_LayoutBindingsIndices.Size(); i++)
            {
                input[i] = g_vk_context.m_DescriptorSetLayoutBindings[program->m_LayoutBindingsIndices[i]];
            }
        }

        Pipeline CreatePipeline(Program* program, VertexBuffer* vertexBuffer, HVertexDeclaration vertexDeclaration)
        {
            Pipeline new_pipeline;

            VkVertexInputAttributeDescription* vk_vertex_input_descs = new VkVertexInputAttributeDescription[vertexDeclaration->m_StreamCount];
            FillVertexInputAttributeDesc(vertexDeclaration, vk_vertex_input_descs);

            VkVertexInputBindingDescription vk_vx_input_description;
            VK_ZERO_MEMORY(&vk_vx_input_description,sizeof(vk_vx_input_description));

            vk_vx_input_description.binding   = 0;
            vk_vx_input_description.stride    = vertexDeclaration->m_Stride;
            vk_vx_input_description.inputRate = VK_VERTEX_INPUT_RATE_VERTEX;

            VkPipelineVertexInputStateCreateInfo vk_vertex_input_info;
            VK_ZERO_MEMORY(&vk_vertex_input_info,sizeof(vk_vertex_input_info));

            vk_vertex_input_info.sType                           = VK_STRUCTURE_TYPE_PIPELINE_VERTEX_INPUT_STATE_CREATE_INFO;
            vk_vertex_input_info.vertexBindingDescriptionCount   = 1;
            vk_vertex_input_info.pVertexBindingDescriptions      = &vk_vx_input_description;
            vk_vertex_input_info.vertexAttributeDescriptionCount = vertexDeclaration->m_StreamCount;
            vk_vertex_input_info.pVertexAttributeDescriptions    = vk_vertex_input_descs;

            VkPipelineInputAssemblyStateCreateInfo vk_input_assembly;
            VK_ZERO_MEMORY(&vk_input_assembly,sizeof(vk_input_assembly));

            vk_input_assembly.sType                  = VK_STRUCTURE_TYPE_PIPELINE_INPUT_ASSEMBLY_STATE_CREATE_INFO;
            vk_input_assembly.topology               = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            vk_input_assembly.primitiveRestartEnable = VK_FALSE;

            VkRect2D vk_scissor;
            VK_ZERO_MEMORY(&vk_scissor, sizeof(vk_scissor));

            vk_scissor.offset.x = 0;
            vk_scissor.offset.y = 0;
            vk_scissor.extent   = g_vk_context.m_SwapChainImageExtent;

            VkPipelineViewportStateCreateInfo vk_viewport_state;
            VK_ZERO_MEMORY(&vk_viewport_state,sizeof(vk_viewport_state));

            vk_viewport_state.sType         = VK_STRUCTURE_TYPE_PIPELINE_VIEWPORT_STATE_CREATE_INFO;
            vk_viewport_state.viewportCount = 1;
            vk_viewport_state.pViewports    = &g_vk_context.m_MainViewport;
            vk_viewport_state.scissorCount  = 1;
            vk_viewport_state.pScissors     = &vk_scissor;

            VkPipelineRasterizationStateCreateInfo vk_rasterizer;
            VK_ZERO_MEMORY(&vk_rasterizer,sizeof(vk_rasterizer));

            vk_rasterizer.sType                   = VK_STRUCTURE_TYPE_PIPELINE_RASTERIZATION_STATE_CREATE_INFO;
            vk_rasterizer.depthClampEnable        = VK_FALSE;
            vk_rasterizer.rasterizerDiscardEnable = VK_FALSE;
            vk_rasterizer.polygonMode             = VK_POLYGON_MODE_FILL;
            vk_rasterizer.lineWidth               = 1.0f;
            vk_rasterizer.cullMode                = VK_CULL_MODE_BACK_BIT;
            vk_rasterizer.frontFace               = VK_FRONT_FACE_COUNTER_CLOCKWISE;
            vk_rasterizer.depthBiasEnable         = VK_FALSE;
            vk_rasterizer.depthBiasConstantFactor = 0.0f;
            vk_rasterizer.depthBiasClamp          = 0.0f;
            vk_rasterizer.depthBiasSlopeFactor    = 0.0f;

            VkPipelineMultisampleStateCreateInfo vk_multisampling;
            VK_ZERO_MEMORY(&vk_multisampling,sizeof(vk_multisampling));

            vk_multisampling.sType                 = VK_STRUCTURE_TYPE_PIPELINE_MULTISAMPLE_STATE_CREATE_INFO;
            vk_multisampling.sampleShadingEnable   = VK_FALSE;
            vk_multisampling.rasterizationSamples  = VK_SAMPLE_COUNT_1_BIT;
            vk_multisampling.minSampleShading      = 1.0f;
            vk_multisampling.pSampleMask           = 0;
            vk_multisampling.alphaToCoverageEnable = VK_FALSE;
            vk_multisampling.alphaToOneEnable      = VK_FALSE;

            VkPipelineColorBlendAttachmentState vk_color_blend_attachment;
            VK_ZERO_MEMORY(&vk_color_blend_attachment,sizeof(vk_color_blend_attachment));

            vk_color_blend_attachment.colorWriteMask      = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            vk_color_blend_attachment.blendEnable         = VK_FALSE;
            vk_color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_ONE;
            vk_color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ZERO;
            vk_color_blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
            vk_color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            vk_color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            vk_color_blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;
            vk_color_blend_attachment.blendEnable         = VK_TRUE;
            vk_color_blend_attachment.srcColorBlendFactor = VK_BLEND_FACTOR_SRC_ALPHA;
            vk_color_blend_attachment.dstColorBlendFactor = VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
            vk_color_blend_attachment.colorBlendOp        = VK_BLEND_OP_ADD;
            vk_color_blend_attachment.srcAlphaBlendFactor = VK_BLEND_FACTOR_ONE;
            vk_color_blend_attachment.dstAlphaBlendFactor = VK_BLEND_FACTOR_ZERO;
            vk_color_blend_attachment.alphaBlendOp        = VK_BLEND_OP_ADD;

            VkPipelineColorBlendStateCreateInfo vk_color_blending;
            VK_ZERO_MEMORY(&vk_color_blending,sizeof(vk_color_blending));

            vk_color_blending.sType             = VK_STRUCTURE_TYPE_PIPELINE_COLOR_BLEND_STATE_CREATE_INFO;
            vk_color_blending.logicOpEnable     = VK_FALSE;
            vk_color_blending.logicOp           = VK_LOGIC_OP_COPY;
            vk_color_blending.attachmentCount   = 1;
            vk_color_blending.pAttachments      = &vk_color_blend_attachment;
            vk_color_blending.blendConstants[0] = 0.0f;
            vk_color_blending.blendConstants[1] = 0.0f;
            vk_color_blending.blendConstants[2] = 0.0f;
            vk_color_blending.blendConstants[3] = 0.0f;

            VkDescriptorSetLayoutBinding* vk_layout_bindings = new VkDescriptorSetLayoutBinding[program->m_LayoutBindingsIndices.Size()];
            FillDescriptorSetLayoutBindings(vk_layout_bindings, program);

            // create descriptor layouts
            VkDescriptorSetLayout vk_descriptor_set_layout;
            VK_ZERO_MEMORY(&vk_descriptor_set_layout, sizeof(vk_descriptor_set_layout));

            VkDescriptorSetLayoutCreateInfo vk_descriptor_layout_create_info;
            VK_ZERO_MEMORY(&vk_descriptor_layout_create_info,sizeof(VkDescriptorSetLayoutCreateInfo));

            vk_descriptor_layout_create_info.sType        = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
            vk_descriptor_layout_create_info.bindingCount = program->m_LayoutBindingsIndices.Size();
            vk_descriptor_layout_create_info.pBindings    = vk_layout_bindings;

            VK_CHECK(vkCreateDescriptorSetLayout(g_vk_context.m_LogicalDevice, &vk_descriptor_layout_create_info, 0, &vk_descriptor_set_layout))

            VkPipelineLayoutCreateInfo vk_pipeline_create_info;
            VK_ZERO_MEMORY(&vk_pipeline_create_info,sizeof(vk_pipeline_create_info));

            vk_pipeline_create_info.sType                  = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
            vk_pipeline_create_info.setLayoutCount         = 1;
            vk_pipeline_create_info.pSetLayouts            = &vk_descriptor_set_layout;
            vk_pipeline_create_info.pushConstantRangeCount = 0;
            vk_pipeline_create_info.pPushConstantRanges    = 0;

            VK_CHECK(vkCreatePipelineLayout(g_vk_context.m_LogicalDevice, &vk_pipeline_create_info, 0, &new_pipeline.m_Layout));

            VkPipelineDepthStencilStateCreateInfo vk_depth_stencil_create_info;
            VK_ZERO_MEMORY(&vk_depth_stencil_create_info, sizeof(vk_depth_stencil_create_info));

            vk_depth_stencil_create_info.sType                 = VK_STRUCTURE_TYPE_PIPELINE_DEPTH_STENCIL_STATE_CREATE_INFO;
            vk_depth_stencil_create_info.depthTestEnable       = VK_TRUE;
            vk_depth_stencil_create_info.depthWriteEnable      = VK_TRUE;
            vk_depth_stencil_create_info.depthCompareOp        = VK_COMPARE_OP_LESS;
            vk_depth_stencil_create_info.depthBoundsTestEnable = VK_FALSE;
            vk_depth_stencil_create_info.minDepthBounds        = 0.0f;
            vk_depth_stencil_create_info.maxDepthBounds        = 1.0f;
            vk_depth_stencil_create_info.stencilTestEnable     = VK_FALSE;

            VkGraphicsPipelineCreateInfo vk_pipeline_info;
            VK_ZERO_MEMORY(&vk_pipeline_info,sizeof(vk_pipeline_info));

            vk_pipeline_info.sType      = VK_STRUCTURE_TYPE_GRAPHICS_PIPELINE_CREATE_INFO;
            vk_pipeline_info.stageCount = sizeof(program->m_ShaderStages) / sizeof(VkPipelineShaderStageCreateInfo);
            vk_pipeline_info.pStages    = program->m_ShaderStages;

            vk_pipeline_info.pVertexInputState   = &vk_vertex_input_info;
            vk_pipeline_info.pInputAssemblyState = &vk_input_assembly;
            vk_pipeline_info.pViewportState      = &vk_viewport_state;
            vk_pipeline_info.pRasterizationState = &vk_rasterizer;
            vk_pipeline_info.pMultisampleState   = &vk_multisampling;
            vk_pipeline_info.pDepthStencilState  = &vk_depth_stencil_create_info;
            vk_pipeline_info.pColorBlendState    = &vk_color_blending;
            vk_pipeline_info.pDynamicState       = 0;
            vk_pipeline_info.layout              = new_pipeline.m_Layout;
            vk_pipeline_info.renderPass          = g_vk_context.m_MainRenderPass.m_Handle; // TODO: this should be based on active renderpass!
            vk_pipeline_info.subpass             = 0;
            vk_pipeline_info.basePipelineHandle  = VK_NULL_HANDLE; // Not sure about this
            vk_pipeline_info.basePipelineIndex   = -1; // ... Or this

            VK_CHECK(vkCreateGraphicsPipelines(g_vk_context.m_LogicalDevice, VK_NULL_HANDLE, 1, &vk_pipeline_info, 0, &new_pipeline.m_Handle));

            delete[] vk_vertex_input_descs;
            delete[] vk_layout_bindings;

            return new_pipeline;
        }

        Pipeline* GetPipeline(Program* program, VertexBuffer* vertexBuffer, HVertexDeclaration vertexDeclaration)
        {
            HashState64 pipeline_hash_state;
            dmHashInit64(&pipeline_hash_state, false);
            dmHashUpdateBuffer64(&pipeline_hash_state, &program->m_VertexProgram->m_Hash, sizeof(program->m_VertexProgram->m_Hash));
            dmHashUpdateBuffer64(&pipeline_hash_state, &program->m_FragmentProgram->m_Hash, sizeof(program->m_FragmentProgram->m_Hash));
            uint64_t pipeline_hash = dmHashFinal64(&pipeline_hash_state);

            Pipeline* cached_pipeline = g_vk_context.m_PipelineCache.Get(pipeline_hash);

            if (!cached_pipeline)
            {
                g_vk_context.m_PipelineCache.Put(pipeline_hash, CreatePipeline(program, vertexBuffer, vertexDeclaration));
                cached_pipeline = g_vk_context.m_PipelineCache.Get(pipeline_hash);

                dmLogInfo("Create new VK Pipeline with hash %llu", pipeline_hash);
            }

            return cached_pipeline;
        }

        bool CreateImage2D(unsigned int imageWidth, unsigned int imageHeight,
            VkFormat imageFormat, VkImageTiling imageTiling,
            VkImageUsageFlags imageUsage, VkMemoryPropertyFlags memoryProperties,
            VkImage* imagePtr, GPUMemory* memoryPtr)
        {
            assert(memoryPtr->m_MemorySize == 0);

            VkImageFormatProperties vk_format_properties;

            VK_CHECK(vkGetPhysicalDeviceImageFormatProperties(
                g_vk_context.m_PhysicalDevice, imageFormat, VK_IMAGE_TYPE_2D,
                imageTiling, imageUsage, 0, &vk_format_properties));

            VkImageCreateInfo vk_image_create_info;
            VK_ZERO_MEMORY(&vk_image_create_info,sizeof(vk_image_create_info));

            vk_image_create_info.sType         = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
            vk_image_create_info.imageType     = VK_IMAGE_TYPE_2D;
            vk_image_create_info.extent.width  = imageWidth;
            vk_image_create_info.extent.height = imageHeight;
            vk_image_create_info.extent.depth  = 1;
            vk_image_create_info.mipLevels     = 1; // TODO
            vk_image_create_info.arrayLayers   = 1;
            vk_image_create_info.format        = imageFormat;
            vk_image_create_info.tiling        = imageTiling;
            vk_image_create_info.initialLayout = VK_IMAGE_LAYOUT_UNDEFINED;
            vk_image_create_info.usage         = imageUsage;
            vk_image_create_info.samples       = VK_SAMPLE_COUNT_1_BIT;
            vk_image_create_info.sharingMode   = VK_SHARING_MODE_EXCLUSIVE;
            vk_image_create_info.flags         = VK_IMAGE_CREATE_MUTABLE_FORMAT_BIT;

            if (vkCreateImage(g_vk_context.m_LogicalDevice, &vk_image_create_info, 0, imagePtr) != VK_SUCCESS)
            {
                return false;
            }

            VkMemoryRequirements vk_memory_req;
            vkGetImageMemoryRequirements(g_vk_context.m_LogicalDevice, *imagePtr, &vk_memory_req);

            VkMemoryAllocateInfo vk_memory_alloc_info;
            VK_ZERO_MEMORY(&vk_memory_alloc_info,sizeof(vk_memory_alloc_info));

            vk_memory_alloc_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            vk_memory_alloc_info.allocationSize  = vk_memory_req.size;
            vk_memory_alloc_info.memoryTypeIndex = 0;

            int32_t memory_type_index = FindMemoryTypeIndex(vk_memory_req.memoryTypeBits, memoryProperties);

            if (memory_type_index < 0)
            {
                return false;
            }

            vk_memory_alloc_info.memoryTypeIndex = memory_type_index;

            if (vkAllocateMemory(g_vk_context.m_LogicalDevice, &vk_memory_alloc_info, 0, &memoryPtr->m_DeviceMemory) != VK_SUCCESS)
            {
                return false;
            }

            vkBindImageMemory(g_vk_context.m_LogicalDevice, *imagePtr, memoryPtr->m_DeviceMemory, 0);

            memoryPtr->m_MemorySize = vk_memory_req.size;

            return true;
        }

        Texture CreateDepthTexture(uint32_t width, uint32_t height, VkFormat format)
        {
            Texture depth_texture;
            VK_ZERO_MEMORY(&depth_texture,sizeof(depth_texture));

            if(!CreateImage2D(width, height, format,
                VK_IMAGE_TILING_OPTIMAL, VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT,
                &depth_texture.m_Image, &depth_texture.m_GPUBuffer))
            {
                dmLogError("Failed to create depth texture");
                return depth_texture;
            }

            // create image view
            VkImageViewCreateInfo vk_view_create_info;
            VK_ZERO_MEMORY(&vk_view_create_info, sizeof(vk_view_create_info));

            vk_view_create_info.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            vk_view_create_info.image                           = depth_texture.m_Image;
            vk_view_create_info.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
            vk_view_create_info.format                          = format;
            vk_view_create_info.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_DEPTH_BIT;
            vk_view_create_info.subresourceRange.baseMipLevel   = 0;
            vk_view_create_info.subresourceRange.levelCount     = 1;
            vk_view_create_info.subresourceRange.baseArrayLayer = 0;
            vk_view_create_info.subresourceRange.layerCount     = 1;

            VK_CHECK(vkCreateImageView(g_vk_context.m_LogicalDevice, &vk_view_create_info, 0, &depth_texture.m_View));

            TransitionImageLayout(depth_texture.m_Image, format, VK_IMAGE_LAYOUT_UNDEFINED, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);

            return depth_texture;
        }

        void CreateMainDepthBuffer()
        {
            g_vk_context.m_MainDepthBuffer = CreateDepthTexture(
                g_vk_context.m_SwapChainImageExtent.width,
                g_vk_context.m_SwapChainImageExtent.height,
                GetSuitableDepthFormat());
        }

        void CreateMainFrameBuffers()
        {
            // We need to create a framebuffer per swap chain image
            // so that they can be used in different states in the rendering pipeline
            g_vk_context.m_MainFramebuffers.SetCapacity(g_vk_context.m_SwapChainImages.Size());
            g_vk_context.m_MainFramebuffers.SetSize(g_vk_context.m_SwapChainImages.Size());

            for (uint8_t i=0; i < g_vk_context.m_MainFramebuffers.Size(); i++)
            {
            	// NOTE: All swap chain images can share the same depth buffer (jg: is this really true?)
                VkImageView& vk_image_view_color     = g_vk_context.m_SwapChainImageViews[i];
                VkImageView& vk_image_view_depth     = g_vk_context.m_MainDepthBuffer.m_View;
                VkImageView  vk_image_attachments[2] = { vk_image_view_color, vk_image_view_depth };

                VkFramebufferCreateInfo framebuffer_create_info;
                VK_ZERO_MEMORY(&framebuffer_create_info,sizeof(framebuffer_create_info));

                framebuffer_create_info.sType           = VK_STRUCTURE_TYPE_FRAMEBUFFER_CREATE_INFO;
                framebuffer_create_info.renderPass      = g_vk_context.m_MainRenderPass.m_Handle;
                framebuffer_create_info.attachmentCount = sizeof(vk_image_attachments);
                framebuffer_create_info.pAttachments    = vk_image_attachments;
                framebuffer_create_info.width           = g_vk_context.m_SwapChainImageExtent.width;
                framebuffer_create_info.height          = g_vk_context.m_SwapChainImageExtent.height;
                framebuffer_create_info.layers          = 1;

                VkFramebuffer* framebuffer_ptr = &g_vk_context.m_MainFramebuffers[i];

                VK_CHECK(vkCreateFramebuffer(g_vk_context.m_LogicalDevice, &framebuffer_create_info, 0, framebuffer_ptr));
            }
        }

        void CreateMainCommandPool()
        {
            QueueFamily vk_queue_family = GetQueueFamily(g_vk_context.m_PhysicalDevice);

            VkCommandPoolCreateInfo vk_create_pool_info;
            VK_ZERO_MEMORY(&vk_create_pool_info,sizeof(vk_create_pool_info));

            vk_create_pool_info.sType            = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
            vk_create_pool_info.queueFamilyIndex = vk_queue_family.m_GraphicsFamily;
            vk_create_pool_info.flags            = VK_COMMAND_POOL_CREATE_RESET_COMMAND_BUFFER_BIT;

            VK_CHECK(vkCreateCommandPool(g_vk_context.m_LogicalDevice, &vk_create_pool_info, 0, &g_vk_context.m_MainCommandPool));
        }

        void CreateMainCommandBuffer()
        {
            g_vk_context.m_MainCommandBuffers.SetCapacity(g_vk_context.m_SwapChainImages.Size());
            g_vk_context.m_MainCommandBuffers.SetSize(g_vk_context.m_SwapChainImages.Size());

            CreateCommandBuffer(g_vk_context.m_MainCommandBuffers.Begin(),
                g_vk_context.m_MainCommandBuffers.Size(),
                g_vk_context.m_MainCommandPool);
        }

        void CreateMainDescriptorPool()
        {
            // NOTE: This needs further investigation! Not sure how to deal with descriptor pools correctly..
            VkDescriptorPoolSize vk_pool_size[2];
            VK_ZERO_MEMORY(vk_pool_size, sizeof(vk_pool_size));

            vk_pool_size[0].type            = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            vk_pool_size[0].descriptorCount = g_vk_context.m_SwapChainImages.Size();

            vk_pool_size[1].type            = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            vk_pool_size[1].descriptorCount = g_vk_context.m_SwapChainImages.Size();

            VkDescriptorPoolCreateInfo vk_pool_create_info;
            VK_ZERO_MEMORY(&vk_pool_create_info, sizeof(VkDescriptorPoolCreateInfo));

            vk_pool_create_info.sType         = VK_STRUCTURE_TYPE_DESCRIPTOR_POOL_CREATE_INFO;
            vk_pool_create_info.poolSizeCount = sizeof(vk_pool_size) / sizeof(VkDescriptorPoolSize);
            vk_pool_create_info.pPoolSizes    = vk_pool_size;
            vk_pool_create_info.maxSets       = g_vk_context.m_SwapChainImages.Size();

            VK_CHECK(vkCreateDescriptorPool(g_vk_context.m_LogicalDevice, &vk_pool_create_info, 0, &g_vk_context.m_DescriptorPool) != VK_SUCCESS)
        }

        bool Initialize()
        {
            g_vk_context.m_ApplicationInfo;
            VK_ZERO_MEMORY(&g_vk_context.m_ApplicationInfo, sizeof(g_vk_context.m_ApplicationInfo));

            g_vk_context.m_ApplicationInfo.sType              = VK_STRUCTURE_TYPE_APPLICATION_INFO;
            g_vk_context.m_ApplicationInfo.pApplicationName   = "Defold";
            g_vk_context.m_ApplicationInfo.applicationVersion = VK_MAKE_VERSION(1, 0, 0);
            g_vk_context.m_ApplicationInfo.pEngineName        = "Defold Engine";
            g_vk_context.m_ApplicationInfo.engineVersion      = VK_MAKE_VERSION(1, 0, 0);
            g_vk_context.m_ApplicationInfo.apiVersion         = VK_API_VERSION_1_0;

            VkInstanceCreateInfo instance_create_info;
            VK_ZERO_MEMORY(&instance_create_info, sizeof(instance_create_info));

            instance_create_info.sType            = VK_STRUCTURE_TYPE_INSTANCE_CREATE_INFO;
            instance_create_info.pApplicationInfo = &g_vk_context.m_ApplicationInfo;

            // Populate the extension list with available API extensions (and print them)
            SetInstanceExtensionList();

            // Just set some default value to hold stream descriptors..
            g_vk_context.m_VertexStreamDescriptors.SetCapacity(4);

            // No idea about these numbers!
            g_vk_context.m_PipelineCache.SetCapacity(32,64);

            if (g_enable_validation_layers && GetValidationSupport())
            {
                dmLogInfo("Required validation layers are supported");

                uint32_t enabled_layer_count = 0;

                while(g_validation_layers[enabled_layer_count])
                {
                    enabled_layer_count++;
                }

                instance_create_info.enabledLayerCount   = enabled_layer_count;
                instance_create_info.ppEnabledLayerNames = g_validation_layers;
            }

            dmArray<const char*> required_extensions;
            GetRequiredInstanceExtensions(required_extensions);

            instance_create_info.enabledExtensionCount   = required_extensions.Size();
            instance_create_info.ppEnabledExtensionNames = required_extensions.Begin();

            if (vkCreateInstance(&instance_create_info, 0, &g_vk_context.m_Instance) == VK_SUCCESS)
            {
                dmLogInfo("Successfully created Vulkan instance!");
            }
            else
            {
                // TODO: We should do cleanup here and return
                dmLogError("Failed to create Vulkan instance");
            }

            if (g_enable_validation_layers)
            {
                VkDebugUtilsMessengerCreateInfoEXT debug_callback_create_info;
                VK_ZERO_MEMORY(&debug_callback_create_info, sizeof(debug_callback_create_info));

                debug_callback_create_info.sType           = VK_STRUCTURE_TYPE_DEBUG_UTILS_MESSENGER_CREATE_INFO_EXT;
                debug_callback_create_info.messageSeverity = VK_DEBUG_UTILS_MESSAGE_SEVERITY_VERBOSE_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_WARNING_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_SEVERITY_ERROR_BIT_EXT;

                debug_callback_create_info.messageType     = VK_DEBUG_UTILS_MESSAGE_TYPE_GENERAL_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_VALIDATION_BIT_EXT |
                    VK_DEBUG_UTILS_MESSAGE_TYPE_PERFORMANCE_BIT_EXT;

                debug_callback_create_info.pfnUserCallback = g_vk_debug_callback;

                PFN_vkCreateDebugUtilsMessengerEXT func_ptr =
                    (PFN_vkCreateDebugUtilsMessengerEXT) vkGetInstanceProcAddr(g_vk_context.m_Instance, "vkCreateDebugUtilsMessengerEXT");

                if (func_ptr)
                {
                    func_ptr(g_vk_context.m_Instance, &debug_callback_create_info, 0, &g_vk_context.m_DebugCallback);
                }
                else
                {
                    dmLogError("Unable to create validation layer callback");
                }
            }

            g_vk_context.m_MainViewport.x         = 0;
            g_vk_context.m_MainViewport.y         = 0;
            g_vk_context.m_MainViewport.width     = 0;
            g_vk_context.m_MainViewport.height    = 0;
            g_vk_context.m_MainViewport.minDepth  = 0.0f;
            g_vk_context.m_MainViewport.maxDepth  = 1.0f;

            return true;
        }

        void CreateMainFrameResources()
        {
            VkSemaphoreCreateInfo vk_create_semaphore_info;
            VK_ZERO_MEMORY(&vk_create_semaphore_info,sizeof(vk_create_semaphore_info));
            vk_create_semaphore_info.sType = VK_STRUCTURE_TYPE_SEMAPHORE_CREATE_INFO;

            for(uint8_t i=0; i < g_vk_max_frames_in_flight; i++)
            {
                VK_CHECK(vkCreateSemaphore(g_vk_context.m_LogicalDevice, &vk_create_semaphore_info, 0, &g_vk_context.m_FrameResources[i].m_ImageAvailable));
                VK_CHECK(vkCreateSemaphore(g_vk_context.m_LogicalDevice, &vk_create_semaphore_info, 0, &g_vk_context.m_FrameResources[i].m_RenderFinished));
            }
        }

        bool CreateGPUBuffer(VkDeviceSize size, VkBufferUsageFlags usage,
            VkMemoryPropertyFlags properties, VkBuffer* bufferObject, GPUMemory* bufferMemory)
        {
            assert(size);

            VkBufferCreateInfo vk_create_buffer_info;
            VK_ZERO_MEMORY(&vk_create_buffer_info,sizeof(vk_create_buffer_info));

            vk_create_buffer_info.sType       = VK_STRUCTURE_TYPE_BUFFER_CREATE_INFO;
            vk_create_buffer_info.size        = size;
            vk_create_buffer_info.sharingMode = VK_SHARING_MODE_EXCLUSIVE;
            vk_create_buffer_info.usage       = usage;

            if (vkCreateBuffer(g_vk_context.m_LogicalDevice, &vk_create_buffer_info, 0, bufferObject) != VK_SUCCESS)
            {
                return false;
            }

            VkMemoryRequirements vk_buffer_memory_req;
            vkGetBufferMemoryRequirements(g_vk_context.m_LogicalDevice, *bufferObject, &vk_buffer_memory_req);

            VkMemoryAllocateInfo vk_memory_allocate_info;
            VK_ZERO_MEMORY(&vk_memory_allocate_info, sizeof(VkMemoryAllocateInfo));

            vk_memory_allocate_info.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
            vk_memory_allocate_info.allocationSize  = vk_buffer_memory_req.size;
            vk_memory_allocate_info.memoryTypeIndex = 0;

            int memory_type_index = FindMemoryTypeIndex(vk_buffer_memory_req.memoryTypeBits, properties);

            if (memory_type_index < 0)
            {
                return false;
            }

            vk_memory_allocate_info.memoryTypeIndex = memory_type_index;

            if (vkAllocateMemory(g_vk_context.m_LogicalDevice, &vk_memory_allocate_info, 0, &bufferMemory->m_DeviceMemory) != VK_SUCCESS)
            {
                return false;
            }

            vkBindBufferMemory(g_vk_context.m_LogicalDevice, *bufferObject, bufferMemory->m_DeviceMemory, 0);

            return true;
        }

        void UploadBufferData(GeometryBuffer* buffer, const void* data, size_t dataSize, size_t dataOffset)
        {
            assert(buffer);
            assert(dataSize);

            // TODO: Updating data to exisiting buffer is not supported yet..
            assert(buffer->m_GPUBuffer.m_MemorySize == 0 || buffer->m_GPUBuffer.m_MemorySize == dataSize);

            if (buffer->m_Handle == NULL)
            {
                CreateGPUBuffer(dataSize, buffer->m_Usage,
                    VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                    &buffer->m_Handle, &buffer->m_GPUBuffer);
            }

            if (!data)
            {
                return;
            }

            void* mapped_data_ptr = 0x0;
            vkMapMemory(g_vk_context.m_LogicalDevice,
                buffer->m_GPUBuffer.m_DeviceMemory, dataOffset, dataSize, 0, &mapped_data_ptr);

            memcpy(mapped_data_ptr, data, dataSize);

            vkUnmapMemory(g_vk_context.m_LogicalDevice, buffer->m_GPUBuffer.m_DeviceMemory);
        }

        ShaderProgram* CreateShaderProgram(const void* source, size_t sourceSize)
        {
            ShaderProgram* program = new ShaderProgram();

            VkShaderModuleCreateInfo vk_create_info_shader;
            VK_ZERO_MEMORY(&vk_create_info_shader, sizeof(vk_create_info_shader));

            vk_create_info_shader.sType    = VK_STRUCTURE_TYPE_SHADER_MODULE_CREATE_INFO;
            vk_create_info_shader.codeSize = sourceSize;
            vk_create_info_shader.pCode    = (unsigned int*) source;

            VK_CHECK(vkCreateShaderModule( g_vk_context.m_LogicalDevice, &vk_create_info_shader, 0, &program->m_Handle));

            HashState64 shader_hash_state;
            dmHashInit64(&shader_hash_state, false);
            dmHashUpdateBuffer64(&shader_hash_state, source, sourceSize);
            program->m_Hash = dmHashFinal64(&shader_hash_state);

            return program;
        }

        void LinkProgram(Program* program)
        {
            assert(program);

            VkPipelineShaderStageCreateInfo vk_vertex_shader_create_info;
            VK_ZERO_MEMORY(&vk_vertex_shader_create_info,sizeof(vk_vertex_shader_create_info));

            vk_vertex_shader_create_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vk_vertex_shader_create_info.stage  = VK_SHADER_STAGE_VERTEX_BIT;
            vk_vertex_shader_create_info.module = program->m_VertexProgram->m_Handle;
            vk_vertex_shader_create_info.pName  = "main";

            VkPipelineShaderStageCreateInfo vk_fragment_shader_create_info;
            VK_ZERO_MEMORY(&vk_fragment_shader_create_info,sizeof(VkPipelineShaderStageCreateInfo));

            vk_fragment_shader_create_info.sType  = VK_STRUCTURE_TYPE_PIPELINE_SHADER_STAGE_CREATE_INFO;
            vk_fragment_shader_create_info.stage  = VK_SHADER_STAGE_FRAGMENT_BIT;
            vk_fragment_shader_create_info.module = program->m_FragmentProgram->m_Handle;
            vk_fragment_shader_create_info.pName  = "main";

            program->m_ShaderStages[0] = vk_vertex_shader_create_info;
            program->m_ShaderStages[1] = vk_fragment_shader_create_info;
        }

        uint16_t GetVertexStreamDescriptorIndex(uint32_t binding, VkFormat format, uint32_t offset)
        {
            int32_t index = -1;

            for (uint32_t i=0; i < g_vk_context.m_VertexStreamDescriptors.Size(); i++)
            {
                VkVertexInputAttributeDescription& desc = g_vk_context.m_VertexStreamDescriptors[i];

                if (desc.binding == binding && desc.format == format && desc.offset == offset)
                {
                    index = (int32_t) i;
                    break;
                }
            }

            if (index == -1)
            {
                VkVertexInputAttributeDescription vk_desc;
                vk_desc.location = 0;
                vk_desc.binding  = binding;
                vk_desc.format   = format;
                vk_desc.offset   = offset;

                index = g_vk_context.m_VertexStreamDescriptors.Size();

                g_vk_context.m_VertexStreamDescriptors.SetCapacity(g_vk_context.m_VertexStreamDescriptors.Capacity() * 2);
                g_vk_context.m_VertexStreamDescriptors.Push(vk_desc);
            }

            return (uint16_t) index;
        }

        VertexBuffer* CreateVertexBuffer(const void* data, size_t dataSize)
        {
            VertexBuffer* new_buffer = new VertexBuffer(VK_BUFFER_USAGE_VERTEX_BUFFER_BIT);

            if (dataSize)
            {
                UploadBufferData((GeometryBuffer*) new_buffer, data, dataSize, 0);
            }

            return new_buffer;
        }

        void DeleteVertexBuffer(VertexBuffer* vxb)
        {
            assert(vxb);
            vkDestroyBuffer(g_vk_context.m_LogicalDevice, vxb->m_Handle, 0);
            vkFreeMemory(g_vk_context.m_LogicalDevice, vxb->m_GPUBuffer.m_DeviceMemory, 0);

            delete vxb;
        }

        IndexBuffer* CreateIndexBuffer(const void* data, size_t dataSize)
        {
            IndexBuffer* new_buffer = new IndexBuffer(VK_BUFFER_USAGE_INDEX_BUFFER_BIT);

            UploadBufferData((GeometryBuffer*) new_buffer, data, dataSize, 0);

            return new_buffer;
        }

        void DeleteIndexBuffer(IndexBuffer* ixb)
        {
            assert(ixb);
            vkDestroyBuffer(g_vk_context.m_LogicalDevice, ixb->m_Handle, 0);
            vkFreeMemory(g_vk_context.m_LogicalDevice, ixb->m_GPUBuffer.m_DeviceMemory, 0);

            delete ixb;
        }

        static inline void BindPipeline(Pipeline* pipeline)
        {
            vkCmdBindPipeline(g_vk_context.m_MainCommandBuffers[g_vk_context.m_CurrentFrameImageIx],
                VK_PIPELINE_BIND_POINT_GRAPHICS, pipeline->m_Handle);
        }

        static inline void BindVertexBuffer(VertexBuffer* vertexBuffer)
        {
            VkBuffer vk_vertex_buffers[]            = {vertexBuffer->m_Handle};
            VkDeviceSize vk_vertex_buffer_offsets[] = {0};

            vkCmdBindVertexBuffers(g_vk_context.m_MainCommandBuffers[g_vk_context.m_CurrentFrameImageIx]
                , 0, 1, vk_vertex_buffers, vk_vertex_buffer_offsets);
        }

        static inline void BindIndexBuffer(IndexBuffer* indexBuffer)
        {
            // TODO: Set correct index buffer type!
            vkCmdBindIndexBuffer(g_vk_context.m_MainCommandBuffers[g_vk_context.m_CurrentFrameImageIx],
                indexBuffer->m_Handle, 0, VK_INDEX_TYPE_UINT32);
        }

        static inline void DrawIndexed(uint32_t offset, uint32_t drawCount)
        {
            vkCmdDrawIndexed(g_vk_context.m_MainCommandBuffers[g_vk_context.m_CurrentFrameImageIx],
                drawCount, 1, offset, 0, 0);
        }

        static inline void Draw(uint32_t offset, uint32_t drawCount)
        {
            vkCmdDraw(g_vk_context.m_MainCommandBuffers[g_vk_context.m_CurrentFrameImageIx],
                drawCount, 1, offset, 0);
        }

        static inline void SetViewport(int32_t x, int32_t y, int32_t width, int32_t height)
        {
            g_vk_context.m_MainViewport.x      = x;
            g_vk_context.m_MainViewport.y      = y;
            g_vk_context.m_MainViewport.width  = width;
            g_vk_context.m_MainViewport.height = height;
        }

        bool OpenWindow(WindowParams* params)
        {
            if (!glfwOpenWindow(params->m_Width, params->m_Height, 8, 8, 8, 8, 32, 8, GLFW_WINDOW))
            {
                return false;
            }

            glfwWrapper::GLWFWindow wnd;

            wnd.ns.view   = glfwGetOSXNSView();
            wnd.ns.object = glfwGetOSXNSWindow();

            if (glfwWrapper::glfwCreateWindowSurface(g_vk_context.m_Instance, &wnd, 0, &g_vk_context.m_Surface) != VK_SUCCESS)
            {
                dmLogError("Unable to create window");
                return false;
            }

            if (!SetPhysicalDevice())
            {
                dmLogError("Unable to set a physical device");
                return false;
            }

            if (!SetLogicalDevice())
            {
                dmLogError("Unable to set a logical device");
                return false;
            }

            if (!CreateSwapChain(params->m_Width, params->m_Height))
            {
                dmLogError("Unable to create swap chain");
                return false;
            }

            if (!CreateSwapChainImageViews())
            {
                dmLogError("Unable to create swap chain image views");
                return false;
            }

            CreateMainRenderPass();
            CreateMainCommandPool();
            CreateMainDepthBuffer();
            CreateMainFrameBuffers();
            CreateMainCommandBuffer();
            CreateMainDescriptorPool();
            CreateMainFrameResources();

            return true;
        }

        void Clear(float r, float g, float b, float a, float d, float s)
        {
        	VkClearRect vk_clear_rect;
        	vk_clear_rect.rect.offset.x      = 0;
			vk_clear_rect.rect.offset.y      = 0;
			vk_clear_rect.rect.extent.width  = g_vk_context.m_SwapChainImageExtent.width;
			vk_clear_rect.rect.extent.height = g_vk_context.m_SwapChainImageExtent.height;
			vk_clear_rect.baseArrayLayer     = 0;
			vk_clear_rect.layerCount         = 1;

			VkClearAttachment vk_clear_attachments[2];
			VK_ZERO_MEMORY(vk_clear_attachments,sizeof(vk_clear_attachments));

			vk_clear_attachments[0].colorAttachment             = 0;
			vk_clear_attachments[0].aspectMask                  = VK_IMAGE_ASPECT_COLOR_BIT;
			vk_clear_attachments[0].clearValue.color.float32[0] = r;
			vk_clear_attachments[0].clearValue.color.float32[1] = g;
			vk_clear_attachments[0].clearValue.color.float32[2] = b;
			vk_clear_attachments[0].clearValue.color.float32[3] = a;

			vk_clear_attachments[1].colorAttachment                 = 0;
			vk_clear_attachments[1].aspectMask                      = VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
			vk_clear_attachments[1].clearValue.depthStencil.stencil = s;
			vk_clear_attachments[1].clearValue.depthStencil.depth   = d;

			// NOTE: This should clear the currently bound framebuffer and not the
			//       framebuffers used for the swap chain!
        	vkCmdClearAttachments(
        		g_vk_context.m_MainCommandBuffers[g_vk_context.m_CurrentFrameImageIx],
			    2, vk_clear_attachments, 1, &vk_clear_rect);
        }

        void BeginFrame()
        {
        	uint32_t* vk_frame_image_ptr = (uint32_t*) &g_vk_context.m_CurrentFrameImageIx;
        	uint32_t  vk_frame_image     = 0;

            vkAcquireNextImageKHR(g_vk_context.m_LogicalDevice, g_vk_context.m_SwapChain, UINT64_MAX,
                g_vk_context.m_FrameResources[g_vk_context.m_CurrentFrameInFlight].m_ImageAvailable, VK_NULL_HANDLE, vk_frame_image_ptr);

            vk_frame_image = *vk_frame_image_ptr;

            VkCommandBufferBeginInfo vk_command_buffer_begin_info;

            vk_command_buffer_begin_info.sType            = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
            vk_command_buffer_begin_info.flags            = VK_COMMAND_BUFFER_USAGE_SIMULTANEOUS_USE_BIT;
            vk_command_buffer_begin_info.pInheritanceInfo = 0;
            vk_command_buffer_begin_info.pNext            = 0;

            VK_CHECK(vkBeginCommandBuffer(g_vk_context.m_MainCommandBuffers[g_vk_context.m_CurrentFrameImageIx], &vk_command_buffer_begin_info));

            VkRenderPassBeginInfo vk_render_pass_begin_info;
		    vk_render_pass_begin_info.sType       = VK_STRUCTURE_TYPE_RENDER_PASS_BEGIN_INFO;
		    vk_render_pass_begin_info.renderPass  = g_vk_context.m_MainRenderPass.m_Handle;
		    vk_render_pass_begin_info.framebuffer = g_vk_context.m_MainFramebuffers[vk_frame_image];
		    vk_render_pass_begin_info.pNext       = 0;

		    vk_render_pass_begin_info.renderArea.offset.x = 0;
		    vk_render_pass_begin_info.renderArea.offset.y = 0;
		    vk_render_pass_begin_info.renderArea.extent   = g_vk_context.m_SwapChainImageExtent;
		    vk_render_pass_begin_info.clearValueCount     = 0;
		    vk_render_pass_begin_info.pClearValues        = 0;

		    vkCmdBeginRenderPass(g_vk_context.m_MainCommandBuffers[vk_frame_image], &vk_render_pass_begin_info, VK_SUBPASS_CONTENTS_INLINE);
        }

        void EndFrame()
        {
            FrameResource& current_frame_resource = g_vk_context.m_FrameResources[g_vk_context.m_CurrentFrameInFlight];

            vkCmdEndRenderPass(g_vk_context.m_MainCommandBuffers[g_vk_context.m_CurrentFrameImageIx]);

            VK_CHECK(vkEndCommandBuffer(g_vk_context.m_MainCommandBuffers[g_vk_context.m_CurrentFrameImageIx]))

            VkPipelineStageFlags vk_pipeline_stage_flags = VK_PIPELINE_STAGE_COLOR_ATTACHMENT_OUTPUT_BIT;

            VkSubmitInfo vk_submit_info;
            vk_submit_info.sType                = VK_STRUCTURE_TYPE_SUBMIT_INFO;
            vk_submit_info.pNext                = 0;
            vk_submit_info.waitSemaphoreCount   = 1;
            vk_submit_info.pWaitSemaphores      = &current_frame_resource.m_ImageAvailable;
            vk_submit_info.pWaitDstStageMask    = &vk_pipeline_stage_flags;
            vk_submit_info.commandBufferCount   = 1;
            vk_submit_info.pCommandBuffers      = &g_vk_context.m_MainCommandBuffers[g_vk_context.m_CurrentFrameImageIx];
            vk_submit_info.signalSemaphoreCount = 1;
            vk_submit_info.pSignalSemaphores    = &current_frame_resource.m_RenderFinished;

            VK_CHECK(vkQueueSubmit(g_vk_context.m_GraphicsQueue, 1, &vk_submit_info, VK_NULL_HANDLE));

            VkPresentInfoKHR vk_present_info;
            vk_present_info.sType              = VK_STRUCTURE_TYPE_PRESENT_INFO_KHR;
            vk_present_info.pNext              = 0;
            vk_present_info.waitSemaphoreCount = 1;
            vk_present_info.pWaitSemaphores    = &current_frame_resource.m_RenderFinished;
            vk_present_info.swapchainCount     = 1;
            vk_present_info.pSwapchains        = &g_vk_context.m_SwapChain;
            vk_present_info.pImageIndices      = (uint32_t*) &g_vk_context.m_CurrentFrameImageIx;
            vk_present_info.pResults           = 0;

            VK_CHECK(vkQueuePresentKHR(g_vk_context.m_PresentQueue, &vk_present_info));

            // NOTE: This should probably be removed when we have better frame syncing..
            VK_CHECK(vkQueueWaitIdle(g_vk_context.m_PresentQueue));

            g_vk_context.m_CurrentFrameInFlight = (g_vk_context.m_CurrentFrameInFlight + 1) % g_vk_max_frames_in_flight;
        }
    }

    uint16_t TYPE_SIZE[] =
    {
        sizeof(char), // TYPE_BYTE
        sizeof(unsigned char), // TYPE_UNSIGNED_BYTE
        sizeof(short), // TYPE_SHORT
        sizeof(unsigned short), // TYPE_UNSIGNED_SHORT
        sizeof(int), // TYPE_INT
        sizeof(unsigned int), // TYPE_UNSIGNED_INT
        sizeof(float) // TYPE_FLOAT
    };

    Context* g_Context = 0x0;

    bool Initialize()
    {
        return Vulkan::Initialize();
    }

    void Finalize()
    {
        // nop
    }

    Context::Context(const ContextParams& params)
    {
        memset(this, 0, sizeof(*this));
        m_DefaultTextureMinFilter = params.m_DefaultTextureMinFilter;
        m_DefaultTextureMagFilter = params.m_DefaultTextureMagFilter;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_LUMINANCE;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_LUMINANCE_ALPHA;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_16BPP;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGBA_16BPP;
        m_TextureFormatSupport |= 1 << TEXTURE_FORMAT_RGB_ETC1;
    }

    HContext NewContext(const ContextParams& params)
    {
        if (!g_Context)
        {
            if (glfwInit() == GL_FALSE)
            {
                dmLogError("Could not initialize glfw.");
                return 0x0;
            }

            g_Context = new Context(params);
            return g_Context;
        }
        else
        {
            return 0x0;
        }
    }

    void DeleteContext(HContext context)
    {
        assert(context);
        if (g_Context)
        {
            delete context;
            g_Context = 0x0;
        }
    }

    void OnWindowResize(int width, int height)
    {
    }

    int OnWindowClose()
    {
        if (g_Context->m_WindowCloseCallback != 0x0)
            return g_Context->m_WindowCloseCallback(g_Context->m_WindowCloseCallbackUserData);
        return 1;
    }

    static void OnWindowFocus(int focus)
    {
        assert(g_Context);
        if (g_Context->m_WindowFocusCallback != 0x0)
            g_Context->m_WindowFocusCallback(g_Context->m_WindowFocusCallbackUserData, focus);
    }

    WindowResult OpenWindow(HContext context, WindowParams* params)
    {
        assert(context);
        assert(params);
        if (context->m_WindowOpened)
            return WINDOW_RESULT_ALREADY_OPENED;

        if (!Vulkan::OpenWindow(params))
        {
            dmLogError("Unable to open Vulkan window");
        }

        int width, height;
        glfwGetWindowSize(&width, &height);
        context->m_WindowWidth = (uint32_t)width;
        context->m_WindowHeight = (uint32_t)height;
        context->m_Dpi = 0;

        glfwSetWindowTitle(params->m_Title);
        glfwSetWindowSizeCallback(OnWindowResize);
        glfwSetWindowCloseCallback(OnWindowClose);
        glfwSetWindowFocusCallback(OnWindowFocus);
        glfwSwapInterval(1);

        context->m_WindowResizeCallback         = params->m_ResizeCallback;
        context->m_WindowResizeCallbackUserData = params->m_ResizeCallbackUserData;
        context->m_WindowCloseCallback          = params->m_CloseCallback;
        context->m_WindowCloseCallbackUserData  = params->m_CloseCallbackUserData;
        context->m_WindowFocusCallback          = params->m_FocusCallback;
        context->m_WindowFocusCallbackUserData  = params->m_FocusCallbackUserData;

        context->m_Width        = params->m_Width;
        context->m_Height       = params->m_Height;
        context->m_WindowWidth  = params->m_Width;
        context->m_WindowHeight = params->m_Height;
        context->m_Dpi          = 0;
        context->m_WindowOpened = 1;

        // Draw state
        context->m_CurrentProgram      = 0x0;
        context->m_CurrentVertexBuffer = 0x0;

        if (params->m_PrintDeviceInfo)
        {
            dmLogInfo("Device: vulkan");
        }

        return WINDOW_RESULT_OK;
    }

    uint32_t GetWindowRefreshRate(HContext context)
    {
        assert(context);
        return 0;
    }

    void CloseWindow(HContext context)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            context->m_WindowOpened = 0;
            context->m_Width = 0;
            context->m_Height = 0;
            context->m_WindowWidth = 0;
            context->m_WindowHeight = 0;
        }
    }

    void BeginFrame(HContext context)
    {
        Vulkan::BeginFrame();
    }

    void IconifyWindow(HContext context)
    {
        assert(context);
    }

    void RunApplicationLoop(void* user_data, WindowStepMethod step_method, WindowIsRunning is_running)
    {
        while (0 != is_running(user_data))
        {
            step_method(user_data);
        }
    }

    uint32_t GetWindowState(HContext context, WindowState state)
    {
        switch (state)
        {
            case WINDOW_STATE_OPENED:
                return context->m_WindowOpened;
            default:
                return 0;
        }
    }

    uint32_t GetDisplayDpi(HContext context)
    {
        assert(context);
        return context->m_Dpi;
    }

    uint32_t GetWidth(HContext context)
    {
        return context->m_Width;
    }

    uint32_t GetHeight(HContext context)
    {
        return context->m_Height;
    }

    uint32_t GetWindowWidth(HContext context)
    {
        return context->m_WindowWidth;
    }

    uint32_t GetWindowHeight(HContext context)
    {
        return context->m_WindowHeight;
    }

    void SetWindowSize(HContext context, uint32_t width, uint32_t height)
    {
        assert(context);
        if (context->m_WindowOpened)
        {
            context->m_Width = width;
            context->m_Height = height;
            context->m_WindowWidth = width;
            context->m_WindowHeight = height;
            if (context->m_WindowResizeCallback)
                context->m_WindowResizeCallback(context->m_WindowResizeCallbackUserData, width, height);
        }
    }

    void GetDefaultTextureFilters(HContext context, TextureFilter& out_min_filter, TextureFilter& out_mag_filter)
    {
        out_min_filter = context->m_DefaultTextureMinFilter;
        out_mag_filter = context->m_DefaultTextureMagFilter;
    }

    void Clear(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {
    	assert(context);
    	DM_PROFILE(Graphics, "Clear");

        float r = ((float)red)/255.0f;
        float g = ((float)green)/255.0f;
        float b = ((float)blue)/255.0f;
        float a = ((float)alpha)/255.0f;

        Vulkan::Clear(r,g,b,a,depth,stencil);
    }

    void Flip(HContext context)
    {
        Vulkan::EndFrame();
    }

    void SetSwapInterval(HContext /*context*/, uint32_t /*swap_interval*/)
    {
        // NOP
    }

    #define NATIVE_HANDLE_IMPL(return_type, func_name) return_type GetNative##func_name() { return NULL; }

    NATIVE_HANDLE_IMPL(id, iOSUIWindow);
    NATIVE_HANDLE_IMPL(id, iOSUIView);
    NATIVE_HANDLE_IMPL(id, iOSEAGLContext);
    NATIVE_HANDLE_IMPL(id, OSXNSWindow);
    NATIVE_HANDLE_IMPL(id, OSXNSView);
    NATIVE_HANDLE_IMPL(id, OSXNSOpenGLContext);
    NATIVE_HANDLE_IMPL(HWND, WindowsHWND);
    NATIVE_HANDLE_IMPL(HGLRC, WindowsHGLRC);
    NATIVE_HANDLE_IMPL(EGLContext, AndroidEGLContext);
    NATIVE_HANDLE_IMPL(EGLSurface, AndroidEGLSurface);
    NATIVE_HANDLE_IMPL(JavaVM*, AndroidJavaVM);
    NATIVE_HANDLE_IMPL(jobject, AndroidActivity);
    NATIVE_HANDLE_IMPL(android_app*, AndroidApp);
    NATIVE_HANDLE_IMPL(Window, X11Window);
    NATIVE_HANDLE_IMPL(GLXContext, X11GLXContext);

    #undef NATIVE_HANDLE_IMPL

    HVertexBuffer NewVertexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        return (HVertexBuffer) Vulkan::CreateVertexBuffer(data, size);
    }

    void DeleteVertexBuffer(HVertexBuffer buffer)
    {
        Vulkan::DeleteVertexBuffer((Vulkan::VertexBuffer*) buffer);
    }

    void SetVertexBufferData(HVertexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        // Note: This is what the OpenGL renderer does since it's not permitted, but is it really the indended behaviour?
        //       The debug rendering does some kind of flushing of the vertexbuffer with a null pointer and
        //       a zero size, but on OpenGL that amounts to doing nothing..
        if (size == 0)
        {
            return;
        }

        Vulkan::UploadBufferData((Vulkan::VertexBuffer*) buffer, data, size, 0);
    }

    void SetVertexBufferSubData(HVertexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        Vulkan::UploadBufferData((Vulkan::VertexBuffer*) buffer, data, size, offset);
    }

    void* MapVertexBuffer(HVertexBuffer buffer, BufferAccess access)
    {
        assert(0 && "Not supported");
    }

    bool UnmapVertexBuffer(HVertexBuffer buffer)
    {
        assert(0 && "Not supported");
    }

    uint32_t GetMaxElementsVertices(HContext context)
    {
        return 65536;
    }

    HIndexBuffer NewIndexBuffer(HContext context, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        return (HIndexBuffer) Vulkan::CreateIndexBuffer(data, size);
    }

    void DeleteIndexBuffer(HIndexBuffer buffer)
    {
        Vulkan::DeleteIndexBuffer((Vulkan::IndexBuffer*) buffer);
    }

    void SetIndexBufferData(HIndexBuffer buffer, uint32_t size, const void* data, BufferUsage buffer_usage)
    {
        Vulkan::UploadBufferData((Vulkan::IndexBuffer*) buffer, data, size, 0);
    }

    void SetIndexBufferSubData(HIndexBuffer buffer, uint32_t offset, uint32_t size, const void* data)
    {
        Vulkan::UploadBufferData((Vulkan::IndexBuffer*) buffer, data, size, offset);
    }

    void* MapIndexBuffer(HIndexBuffer buffer, BufferAccess access)
    {
        assert(0 && "Not supported");
    }

    bool UnmapIndexBuffer(HIndexBuffer buffer)
    {
        assert(0 && "Not supported");
    }

    uint32_t GetMaxElementsIndices(HContext context)
    {
        return 65536;
    }

    static inline uint32_t GetTypeSize(Type type)
    {
        uint32_t size = 0;
        switch (type)
        {
            case TYPE_BYTE:
            case TYPE_UNSIGNED_BYTE:
                size = 1;
                break;

            case TYPE_SHORT:
            case TYPE_UNSIGNED_SHORT:
                size = 2;
                break;

            case TYPE_INT:
            case TYPE_UNSIGNED_INT:
            case TYPE_FLOAT:
                size = 4;
                break;

            default:
                assert(0);
                break;
        }
        return size;
    }

    static inline VkFormat GetVulkanFormatFromType(Type type)
    {
        VkFormat vk_format = VK_FORMAT_UNDEFINED;

        switch(type)
        {
            case(TYPE_BYTE):
            case(TYPE_SHORT):
            case(TYPE_INT):
                vk_format = VK_FORMAT_R8_SINT;
                break;
            case(TYPE_UNSIGNED_BYTE):
            case(TYPE_UNSIGNED_SHORT):
            case(TYPE_UNSIGNED_INT):
                vk_format = VK_FORMAT_R8_UINT;
                break;
            case(TYPE_FLOAT_MAT4):
            case(TYPE_FLOAT):
                vk_format = VK_FORMAT_R32_SFLOAT;
                break;
            case(TYPE_FLOAT_VEC4):
                vk_format = VK_FORMAT_R32G32B32A32_SFLOAT;
                break;
            default:break;
        }

        return vk_format;
    }

    HVertexDeclaration NewVertexDeclaration(HContext context, VertexElement* element, uint32_t count, uint32_t stride)
    {
        VertexDeclaration* vd = (VertexDeclaration*) NewVertexDeclaration(context, element, count);
        vd->m_Stride = stride;
        return vd;
    }

    HVertexDeclaration NewVertexDeclaration(HContext context, VertexElement* element, uint32_t count)
    {
        VertexDeclaration* vd = new VertexDeclaration();
        VK_ZERO_MEMORY(vd, sizeof(VertexDeclaration));

        assert(element);
        assert(count <= MAX_VERTEX_STREAM_COUNT);

        vd->m_StreamCount = count;

        for (uint32_t i = 0; i < count; ++i)
        {
            vd->m_Streams[i].m_Name         = element[i].m_Name;
            vd->m_Streams[i].m_LogicalIndex = i;
            vd->m_Streams[i].m_Size         = element[i].m_Size;
            vd->m_Streams[i].m_Type         = element[i].m_Type;
            vd->m_Streams[i].m_Offset       = vd->m_Stride;
            vd->m_Stride += element[i].m_Size * GetTypeSize(element[i].m_Type);

            vd->m_Streams[i].m_DescriptorIndex = Vulkan::GetVertexStreamDescriptorIndex(
                vd->m_Streams[i].m_LogicalIndex,
                GetVulkanFormatFromType(vd->m_Streams[i].m_Type),
                vd->m_Streams[i].m_Offset
            );
        }

        return vd;
    }

    void DeleteVertexDeclaration(HVertexDeclaration vertex_declaration)
    {
        delete vertex_declaration;
    }

    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer)
    {
        context->m_CurrentVertexBuffer = (void*) vertex_buffer;
        context->m_CurrentVertexDeclaration = (void*) vertex_declaration;
    }

    void EnableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer, HProgram program)
    {
        EnableVertexDeclaration(context, vertex_declaration, vertex_buffer);
    }

    void DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration)
    {

    }

    static inline void DrawSetup(HContext context)
    {
        Vulkan::Pipeline* pipeline = Vulkan::GetPipeline(
            (Vulkan::Program*) context->m_CurrentProgram,
            (Vulkan::VertexBuffer*) context->m_CurrentVertexBuffer,
            (HVertexDeclaration) context->m_CurrentVertexDeclaration);

        Vulkan::BindPipeline(pipeline);

        Vulkan::BindVertexBuffer((Vulkan::VertexBuffer*) context->m_CurrentVertexBuffer);

        if (context->m_CurrentIndexBuffer)
        {
            Vulkan::BindIndexBuffer((Vulkan::IndexBuffer*) context->m_CurrentIndexBuffer);
        }
    }

    void DrawElements(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count, Type type, HIndexBuffer index_buffer)
    {
        context->m_CurrentIndexBuffer = (void*) index_buffer;
        DrawSetup(context);
        Vulkan::DrawIndexed(first,count);
    }

    void Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count)
    {
        context->m_CurrentIndexBuffer = 0;
        DrawSetup(context);
        Vulkan::Draw(first,count);
    }

    struct VertexProgram
    {
        char* m_Data;
    };

    struct FragmentProgram
    {
        char* m_Data;
    };

    static void NullUniformCallback(const char* name, uint32_t name_length, dmGraphics::Type type, uintptr_t userdata);

    struct Uniform
    {
        Uniform() : m_Name(0) {};
        char* m_Name;
        uint32_t m_Index;
        Type m_Type;
    };

    struct Program
    {
        Program(VertexProgram* vp, FragmentProgram* fp)
        {
            m_Uniforms.SetCapacity(16);
            m_VP = vp;
            m_FP = fp;
            if (m_VP != 0x0)
                GLSLUniformParse(m_VP->m_Data, NullUniformCallback, (uintptr_t)this);
            if (m_FP != 0x0)
                GLSLUniformParse(m_FP->m_Data, NullUniformCallback, (uintptr_t)this);
        }

        ~Program()
        {
            for(uint32_t i = 0; i < m_Uniforms.Size(); ++i)
                delete[] m_Uniforms[i].m_Name;
        }

        VertexProgram* m_VP;
        FragmentProgram* m_FP;
        dmArray<Uniform> m_Uniforms;
    };

    static void NullUniformCallback(const char* name, uint32_t name_length, dmGraphics::Type type, uintptr_t userdata)
    {
        Program* program = (Program*) userdata;
        if(program->m_Uniforms.Full())
            program->m_Uniforms.OffsetCapacity(16);
        Uniform uniform;
        name_length++;
        uniform.m_Name = new char[name_length];
        dmStrlCpy(uniform.m_Name, name, name_length);
        uniform.m_Index = program->m_Uniforms.Size();
        uniform.m_Type = type;
        program->m_Uniforms.Push(uniform);
    }

    HProgram NewProgram(HContext context, HVertexProgram vertex_program, HFragmentProgram fragment_program)
    {
        assert(vertex_program);
        assert(fragment_program);

        Vulkan::Program* new_program = new Vulkan::Program(
            (const Vulkan::ShaderProgram*) vertex_program,
            (const Vulkan::ShaderProgram*) fragment_program);
        Vulkan::LinkProgram(new_program);

        return (HProgram) new_program;
    }

    void DeleteProgram(HContext context, HProgram program)
    {
        assert(0 && "Todo!");
    }

    HVertexProgram NewVertexProgram(HContext context, const void* program, uint32_t program_size)
    {
        assert(program);
        return (HVertexProgram) Vulkan::CreateShaderProgram((const void*) Vulkan::g_vk_vertex_shader, sizeof(Vulkan::g_vk_vertex_shader));
        // return (HVertexProgram) Vulkan::CreateShaderProgram(program,program_size);
    }

    HFragmentProgram NewFragmentProgram(HContext context, const void* program, uint32_t program_size)
    {
        assert(program);
        return (HVertexProgram) Vulkan::CreateShaderProgram((const void*) Vulkan::g_vk_fragment_shader, sizeof(Vulkan::g_vk_fragment_shader));
        // return (HVertexProgram) Vulkan::CreateShaderProgram(program,program_size);
    }

    bool ReloadVertexProgram(HVertexProgram prog, const void* program, uint32_t program_size)
    {
        assert(program);
        VertexProgram* p = (VertexProgram*)prog;
        delete [] (char*)p->m_Data;
        p->m_Data = new char[program_size];
        memcpy((char*)p->m_Data, program, program_size);
        return !g_ForceVertexReloadFail;
    }

    bool ReloadFragmentProgram(HFragmentProgram prog, const void* program, uint32_t program_size)
    {
        assert(program);
        FragmentProgram* p = (FragmentProgram*)prog;
        delete [] (char*)p->m_Data;
        p->m_Data = new char[program_size];
        memcpy((char*)p->m_Data, program, program_size);
        return !g_ForceFragmentReloadFail;
    }

    void DeleteVertexProgram(HVertexProgram program)
    {
        assert(program);
        VertexProgram* p = (VertexProgram*)program;
        delete [] (char*)p->m_Data;
        delete p;
    }

    void DeleteFragmentProgram(HFragmentProgram program)
    {
        assert(program);
        FragmentProgram* p = (FragmentProgram*)program;
        delete [] (char*)p->m_Data;
        delete p;
    }

    void EnableProgram(HContext context, HProgram program)
    {
        assert(context);
        context->m_CurrentProgram = (void*)program;
    }

    void DisableProgram(HContext context)
    {
        assert(context);
        context->m_CurrentProgram = 0x0;
    }

    bool ReloadProgram(HContext context, HProgram program, HVertexProgram vert_program, HFragmentProgram frag_program)
    {
        (void) context;
        (void) program;

        return true;
    }

    uint32_t GetUniformCount(HProgram prog)
    {
        return 0;
    }

    void GetUniformName(HProgram prog, uint32_t index, char* buffer, uint32_t buffer_size, Type* type)
    {
        *type = TYPE_FLOAT_VEC4;
        /*
        Program* program = (Program*)prog;
        assert(index < program->m_Uniforms.Size());
        Uniform& uniform = program->m_Uniforms[index];
        *buffer = '\0';
        dmStrlCat(buffer, uniform.m_Name, buffer_size);
        *type = uniform.m_Type;
        */
    }

    int32_t GetUniformLocation(HProgram prog, const char* name)
    {
        /*
        Program* program = (Program*)prog;
        uint32_t count = program->m_Uniforms.Size();
        for (uint32_t i = 0; i < count; ++i)
        {
            Uniform& uniform = program->m_Uniforms[i];
            if (dmStrCaseCmp(uniform.m_Name, name)==0)
            {
                return (int32_t)uniform.m_Index;
            }
        }
        */
        return -1;
    }

    void SetViewport(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        assert(context);
        Vulkan::SetViewport(x,y,width,height);
    }

    const Vector4& GetConstantV4Ptr(HContext context, int base_register)
    {
        assert(context);
        assert(context->m_CurrentProgram != 0x0);
        return context->m_ProgramRegisters[base_register];
    }

    void SetConstantV4(HContext context, const Vector4* data, int base_register)
    {
        assert(context);
        assert(context->m_CurrentProgram != 0x0);
        memcpy(&context->m_ProgramRegisters[base_register], data, sizeof(Vector4));
    }

    void SetConstantM4(HContext context, const Vector4* data, int base_register)
    {
        assert(context);
        assert(context->m_CurrentProgram != 0x0);
        memcpy(&context->m_ProgramRegisters[base_register], data, sizeof(Vector4) * 4);
    }

    void SetSampler(HContext context, int32_t location, int32_t unit)
    {
    }

    HRenderTarget NewRenderTarget(HContext context, uint32_t buffer_type_flags, const TextureCreationParams creation_params[MAX_BUFFER_TYPE_COUNT], const TextureParams params[MAX_BUFFER_TYPE_COUNT])
    {
        RenderTarget* rt = new RenderTarget();
        memset(rt, 0, sizeof(RenderTarget));

        void** buffers[MAX_BUFFER_TYPE_COUNT] = {&rt->m_FrameBuffer.m_ColorBuffer, &rt->m_FrameBuffer.m_DepthBuffer, &rt->m_FrameBuffer.m_StencilBuffer};
        uint32_t* buffer_sizes[MAX_BUFFER_TYPE_COUNT] = {&rt->m_FrameBuffer.m_ColorBufferSize, &rt->m_FrameBuffer.m_DepthBufferSize, &rt->m_FrameBuffer.m_StencilBufferSize};
        BufferType buffer_types[MAX_BUFFER_TYPE_COUNT] = {BUFFER_TYPE_COLOR_BIT, BUFFER_TYPE_DEPTH_BIT, BUFFER_TYPE_STENCIL_BIT};
        for (uint32_t i = 0; i < MAX_BUFFER_TYPE_COUNT; ++i)
        {
            assert(GetBufferTypeIndex(buffer_types[i]) == i);
            if (buffer_type_flags & buffer_types[i])
            {
                uint32_t buffer_size = sizeof(uint32_t) * params[i].m_Width * params[i].m_Height;
                *(buffer_sizes[i]) = buffer_size;
                rt->m_BufferTextureParams[i] = params[i];
                rt->m_BufferTextureParams[i].m_Data = 0x0;
                rt->m_BufferTextureParams[i].m_DataSize = 0;

                if(i == dmGraphics::GetBufferTypeIndex(dmGraphics::BUFFER_TYPE_COLOR_BIT))
                {
                    rt->m_BufferTextureParams[i].m_DataSize = buffer_size;
                    rt->m_ColorBufferTexture = NewTexture(context, creation_params[i]);
                    SetTexture(rt->m_ColorBufferTexture, rt->m_BufferTextureParams[i]);
                    *(buffers[i]) = rt->m_ColorBufferTexture->m_Data;
                } else {
                    *(buffers[i]) = new char[buffer_size];
                }
            }
        }

        return rt;
    }

    void DeleteRenderTarget(HRenderTarget rt)
    {
        if (rt->m_ColorBufferTexture)
            DeleteTexture(rt->m_ColorBufferTexture);
        delete [] (char*)rt->m_FrameBuffer.m_DepthBuffer;
        delete [] (char*)rt->m_FrameBuffer.m_StencilBuffer;
        delete rt;
    }

    void SetRenderTarget(HContext context, HRenderTarget rendertarget, uint32_t transient_buffer_types)
    {
        (void) transient_buffer_types;
        assert(context);
        context->m_CurrentFrameBuffer = &rendertarget->m_FrameBuffer;
    }

    HTexture GetRenderTargetTexture(HRenderTarget rendertarget, BufferType buffer_type)
    {
        if(buffer_type != BUFFER_TYPE_COLOR_BIT)
            return 0;
        return rendertarget->m_ColorBufferTexture;
    }

    void GetRenderTargetSize(HRenderTarget render_target, BufferType buffer_type, uint32_t& width, uint32_t& height)
    {
        assert(render_target);
        uint32_t i = GetBufferTypeIndex(buffer_type);
        assert(i < MAX_BUFFER_TYPE_COUNT);
        width = render_target->m_BufferTextureParams[i].m_Width;
        height = render_target->m_BufferTextureParams[i].m_Height;
    }

    void SetRenderTargetSize(HRenderTarget rt, uint32_t width, uint32_t height)
    {
        uint32_t buffer_size = sizeof(uint32_t) * width * height;

        void** buffers[MAX_BUFFER_TYPE_COUNT] = {&rt->m_FrameBuffer.m_ColorBuffer, &rt->m_FrameBuffer.m_DepthBuffer, &rt->m_FrameBuffer.m_StencilBuffer};
        uint32_t* buffer_sizes[MAX_BUFFER_TYPE_COUNT] = {&rt->m_FrameBuffer.m_ColorBufferSize, &rt->m_FrameBuffer.m_DepthBufferSize, &rt->m_FrameBuffer.m_StencilBufferSize};
        for (uint32_t i = 0; i < MAX_BUFFER_TYPE_COUNT; ++i)
        {
            if (buffers[i])
            {
                *(buffer_sizes[i]) = buffer_size;
                rt->m_BufferTextureParams[i].m_Width = width;
                rt->m_BufferTextureParams[i].m_Height = height;
                if(i == dmGraphics::GetBufferTypeIndex(dmGraphics::BUFFER_TYPE_COLOR_BIT))
                {
                    rt->m_BufferTextureParams[i].m_DataSize = buffer_size;
                    SetTexture(rt->m_ColorBufferTexture, rt->m_BufferTextureParams[i]);
                    *(buffers[i]) = rt->m_ColorBufferTexture->m_Data;
                } else {
                    delete [] (char*)*(buffers[i]);
                    *(buffers[i]) = new char[buffer_size];
                }
            }
        }
    }

    bool IsTextureFormatSupported(HContext context, TextureFormat format)
    {
        return (context->m_TextureFormatSupport & (1 << format)) != 0;
    }

    uint32_t GetMaxTextureSize(HContext context)
    {
        return 1024;
    }

    HTexture NewTexture(HContext context, const TextureCreationParams& params)
    {
        Texture* tex = new Texture();

        tex->m_Width = params.m_Width;
        tex->m_Height = params.m_Height;
        tex->m_MipMapCount = 0;
        tex->m_Data = 0;

        if (params.m_OriginalWidth == 0) {
            tex->m_OriginalWidth = params.m_Width;
            tex->m_OriginalHeight = params.m_Height;
        } else {
            tex->m_OriginalWidth = params.m_OriginalWidth;
            tex->m_OriginalHeight = params.m_OriginalHeight;
        }

        return tex;
    }

    void DeleteTexture(HTexture t)
    {
        assert(t);
        if (t->m_Data != 0x0)
            delete [] (char*)t->m_Data;
        delete t;
    }

    HandleResult GetTextureHandle(HTexture texture, void** out_handle)
    {
        *out_handle = 0x0;

        if (!texture) {
            return HANDLE_RESULT_ERROR;
        }

        *out_handle = texture->m_Data;

        return HANDLE_RESULT_OK;
    }

    void SetTextureParams(HTexture texture, TextureFilter minfilter, TextureFilter magfilter, TextureWrap uwrap, TextureWrap vwrap)
    {
        assert(texture);
    }

    void SetTexture(HTexture texture, const TextureParams& params)
    {
        assert(texture);
        assert(!params.m_SubUpdate || params.m_SubUpdate && (params.m_X + params.m_Width <= texture->m_Width));
        assert(!params.m_SubUpdate || params.m_SubUpdate && (params.m_Y + params.m_Height <= texture->m_Height));

        if (texture->m_Data != 0x0)
            delete [] (char*)texture->m_Data;
        texture->m_Format = params.m_Format;
        // Allocate even for 0x0 size so that the rendertarget dummies will work.
        texture->m_Data = new char[params.m_DataSize];
        if (params.m_Data != 0x0)
            memcpy(texture->m_Data, params.m_Data, params.m_DataSize);
        texture->m_MipMapCount = dmMath::Max(texture->m_MipMapCount, (uint16_t)(params.m_MipMap+1));
    }

    uint8_t* GetTextureData(HTexture texture) {
        return 0x0;
    }

    uint32_t GetTextureFormatBPP(TextureFormat format)
    {
        static TextureFormatToBPP g_TextureFormatToBPP;
        assert(format < TEXTURE_FORMAT_COUNT);
        return g_TextureFormatToBPP.m_FormatToBPP[format];
    }

    uint32_t GetTextureResourceSize(HTexture texture)
    {
        uint32_t size_total = 0;
        uint32_t size = (texture->m_Width * texture->m_Height * GetTextureFormatBPP(texture->m_Format)) >> 3;
        for(uint32_t i = 0; i < texture->m_MipMapCount; ++i)
        {
            size_total += size;
            size >>= 2;
        }
        return size_total + sizeof(Texture);
    }

    uint16_t GetTextureWidth(HTexture texture)
    {
        return texture->m_Width;
    }

    uint16_t GetTextureHeight(HTexture texture)
    {
        return texture->m_Height;
    }

    uint16_t GetOriginalTextureWidth(HTexture texture)
    {
        return texture->m_OriginalWidth;
    }

    uint16_t GetOriginalTextureHeight(HTexture texture)
    {
        return texture->m_OriginalHeight;
    }

    void EnableTexture(HContext context, uint32_t unit, HTexture texture)
    {
        assert(context);
        assert(unit < MAX_TEXTURE_COUNT);
        assert(texture);
        assert(texture->m_Data);
        context->m_Textures[unit] = texture;
    }

    void DisableTexture(HContext context, uint32_t unit, HTexture texture)
    {
        assert(context);
        assert(unit < MAX_TEXTURE_COUNT);
        context->m_Textures[unit] = 0;
    }

    void ReadPixels(HContext context, void* buffer, uint32_t buffer_size)
    {
        uint32_t w = dmGraphics::GetWidth(context);
        uint32_t h = dmGraphics::GetHeight(context);
        assert (buffer_size >= w * h * 4);
        memset(buffer, 0, w * h * 4);
    }

    void EnableState(HContext context, State state)
    {
        assert(context);
    }

    void DisableState(HContext context, State state)
    {
        assert(context);
    }

    void SetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
        assert(context);
    }

    void SetColorMask(HContext context, bool red, bool green, bool blue, bool alpha)
    {
        assert(context);
        context->m_RedMask = red;
        context->m_GreenMask = green;
        context->m_BlueMask = blue;
        context->m_AlphaMask = alpha;
    }

    void SetDepthMask(HContext context, bool mask)
    {
        assert(context);
        context->m_DepthMask = mask;
    }

    void SetDepthFunc(HContext context, CompareFunc func)
    {
        assert(context);
        context->m_DepthFunc = func;
    }

    void SetScissor(HContext context, int32_t x, int32_t y, int32_t width, int32_t height)
    {
        assert(context);
        context->m_ScissorRect[0] = (int32_t) x;
        context->m_ScissorRect[1] = (int32_t) y;
        context->m_ScissorRect[2] = (int32_t) x+width;
        context->m_ScissorRect[3] = (int32_t) y+height;
    }

    void SetStencilMask(HContext context, uint32_t mask)
    {
        assert(context);
        context->m_StencilMask = mask;
    }

    void SetStencilFunc(HContext context, CompareFunc func, uint32_t ref, uint32_t mask)
    {
        assert(context);
        context->m_StencilFunc = func;
        context->m_StencilFuncRef = ref;
        context->m_StencilFuncMask = mask;
    }

    void SetStencilOp(HContext context, StencilOp sfail, StencilOp dpfail, StencilOp dppass)
    {
        assert(context);
        context->m_StencilOpSFail = sfail;
        context->m_StencilOpDPFail = dpfail;
        context->m_StencilOpDPPass = dppass;
    }

    void SetCullFace(HContext context, FaceType face_type)
    {
        assert(context);
    }

    void SetPolygonOffset(HContext context, float factor, float units)
    {
        assert(context);
    }

    bool AcquireSharedContext()
    {
        return false;
    }

    void UnacquireContext()
    {

    }

    void SetTextureAsync(HTexture texture, const TextureParams& params)
    {
        SetTexture(texture, params);
    }

    uint32_t GetTextureStatusFlags(HTexture texture)
    {
        return TEXTURE_STATUS_OK;
    }

    void SetForceFragmentReloadFail(bool should_fail)
    {
        g_ForceFragmentReloadFail = should_fail;
    }

    void SetForceVertexReloadFail(bool should_fail)
    {
        g_ForceVertexReloadFail = should_fail;
    }

}

