#include <string.h>
#include <assert.h>
#include <vectormath/cpp/vectormath_aos.h>

#include "../graphics_device.h"
#include "null_device.h"

using namespace Vectormath::Aos;

namespace dmGraphics
{

    Device gdevice;
    Context gcontext;

    HContext GetContext()
    {
        return (HContext)&gcontext;
    }

    HDevice NewDevice(int* argc, char** argv, CreateDeviceParams *params )
    {
        assert(params);
        gdevice.m_DisplayWidth = params->m_DisplayWidth;
        gdevice.m_DisplayHeight = params->m_DisplayHeight;
        gdevice.m_Opened = 1;
        return (HDevice)&gdevice;
    }

    void DeleteDevice(HDevice device)
    {
        gdevice.m_Opened = 0;
    }

    void Clear(HContext context, uint32_t flags, uint8_t red, uint8_t green, uint8_t blue, uint8_t alpha, float depth, uint32_t stencil)
    {
        assert(context);
    }

    void Flip()
    {
    }

    HVertexBuffer NewVertexbuffer(uint32_t element_size, uint32_t element_count, BufferType buffer_type, MemoryType memory_type, uint32_t buffer_count, const void* data)
    {
        return new VertexBuffer;
    }

    void DeleteVertexBuffer(HVertexBuffer buffer)
    {
        delete buffer;
    }

    HIndexBuffer NewIndexBuffer(uint32_t element_count, BufferType buffer_type, MemoryType memory_type, const void* data)
    {
        return new IndexBuffer;
    }
    void DeleteIndexBuffer(HIndexBuffer buffer)
    {
        delete buffer;
    }

    HVertexDeclaration NewVertexDeclaration(VertexElement* element, uint32_t count)
    {
        return new VertexDeclaration;
    }

    void DeleteVertexDeclaration(HVertexDeclaration vertex_declaration)
    {
        delete vertex_declaration;
    }

    void SetVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration, HVertexBuffer vertex_buffer)
    {
        assert(context);
    }

    void DisableVertexDeclaration(HContext context, HVertexDeclaration vertex_declaration)
    {
        assert(context);
    }


    void SetVertexStream(HContext context, uint16_t stream, uint16_t size, Type type, uint16_t stride, const void* vertex_buffer)
    {
        assert(context);
        assert(vertex_buffer);
    }

    void DisableVertexStream(HContext context, uint16_t stream)
    {
        assert(context);
    }

    void DrawElements(HContext context, PrimitiveType prim_type, uint32_t count, Type type, const void* index_buffer)
    {
        assert(context);
        assert(index_buffer);
    }

    void DrawRangeElements(HContext context, PrimitiveType prim_type, uint32_t start, uint32_t count, Type type, HIndexBuffer index_buffer)
    {
        assert(context);
    }

    void Draw(HContext context, PrimitiveType prim_type, uint32_t first, uint32_t count)
    {
        assert(context);
    }

    HVertexProgram NewVertexProgram(const void* program, uint32_t program_size)
    {
        assert(program);
        uint32_t* p = new uint32_t;
        return (uint32_t)p;
    }

    HFragmentProgram NewFragmentProgram(const void* program, uint32_t program_size)
    {
        assert(program);
        uint32_t* p = new uint32_t;
        return (uint32_t)p;
    }

    void DeleteVertexProgram(HVertexProgram program)
    {
        assert(program);
        delete (uint32_t*)program;
    }

    void DeleteFragmentProgram(HFragmentProgram program)
    {
        assert(program);
        delete (uint32_t*)program;
    }


    void SetVertexProgram(HContext context, HVertexProgram program)
    {
        assert(context);
    }

    void SetFragmentProgram(HContext context, HFragmentProgram program)
    {
        assert(context);
    }

    void SetViewport(HContext context, int width, int height)
    {
        assert(context);
    }

    void SetFragmentConstant(HContext context, const Vector4* data, int base_register)
    {
        assert(context);
    }

    void SetVertexConstantBlock(HContext context, const Vector4* data, int base_register, int num_vectors)
    {
        assert(context);
    }

    void SetFragmentConstantBlock(HContext context, const Vector4* data, int base_register, int num_vectors)
    {
        assert(context);
    }

    void SetTexture(HContext context, HTexture t)
    {
        assert(context);
        assert(t);
    }

    HTexture NewTexture(uint32_t width, uint32_t height, TextureFormat texture_format)
    {
        Texture* tex = new Texture;
        return (HTexture) tex;
    }

    void SetTextureData(HTexture texture,
                           uint16_t mip_map,
                           uint16_t width, uint16_t height, uint16_t border,
                           TextureFormat texture_format, const void* data, uint32_t data_size)
    {
    }

    void DeleteTexture(HTexture t)
    {
        assert(t);
        delete t;
    }

    void EnableState(HContext context, RenderState state)
    {
        assert(context);
    }

    void DisableState(HContext context, RenderState state)
    {
        assert(context);
    }

    void SetBlendFunc(HContext context, BlendFactor source_factor, BlendFactor destinaton_factor)
    {
        assert(context);
    }

    void SetDepthMask(HContext context, bool mask)
    {
        assert(context);
    }

    void SetCullFace(HContext context, FaceType face_type)
    {
        assert(context);
    }

    uint32_t GetWindowParam(WindowParam param)
    {
        switch (param)
        {
            case WINDOW_PARAM_OPENED:
                return gdevice.m_Opened;
            default:
                return 0;
        }
    }

    uint32_t GetWindowWidth()
    {
        return gdevice.m_DisplayWidth;
    }

    uint32_t GetWindowHeight()
    {
        return gdevice.m_DisplayHeight;
    }
}
