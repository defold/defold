#include "graphics.h"

extern "C"
{
#include <sol/reflect.h>
#include <sol/runtime.h>
}

#include <dlib/log.h>

namespace dmGraphics
{
    WindowParams::WindowParams()
    : m_ResizeCallback(0x0)
    , m_ResizeCallbackUserData(0x0)
    , m_CloseCallback(0x0)
    , m_CloseCallbackUserData(0x0)
    , m_Width(640)
    , m_Height(480)
    , m_Samples(1)
    , m_Title("Dynamo App")
    , m_Fullscreen(false)
    , m_PrintDeviceInfo(false)
    {

    }

    ContextParams::ContextParams()
    : m_DefaultTextureMinFilter(TEXTURE_FILTER_LINEAR_MIPMAP_NEAREST)
    , m_DefaultTextureMagFilter(TEXTURE_FILTER_LINEAR)
    {

    }

    extern "C"
    {
        dmGraphics::HVertexDeclaration SolNewVertexDeclaration(dmGraphics::HContext context, dmGraphics::VertexElement* element, uint32_t count)
        {
            // this object is fully managed by sol, and the delete call only calls runtime_unpin... so do it here, and sol code will not have
            // to do any manual deletion.
            dmGraphics::HVertexDeclaration decl = (dmGraphics::HVertexDeclaration) dmGraphics::NewVertexDeclaration(context, element, count);
            runtime_unpin((void*)decl);
            return decl;
        }

        void SolEnableVertexDeclarationCVV(dmGraphics::HContext context, dmGraphics::HVertexDeclaration vertex_declaration, dmGraphics::HVertexBuffer vertex_buffer)
        {
            return dmGraphics::EnableVertexDeclaration(context, vertex_declaration, vertex_buffer);
        }

        void SolEnableVertexDeclarationCVVP(dmGraphics::HContext context, dmGraphics::HVertexDeclaration vertex_declaration, dmGraphics::HVertexBuffer vertex_buffer, dmGraphics::HProgram program)
        {
            return dmGraphics::EnableVertexDeclaration(context, vertex_declaration, vertex_buffer, program);
        }

        dmGraphics::HVertexBuffer SolNewVertexBuffer(dmGraphics::HContext context, uint32_t size, const void* data, dmGraphics::BufferUsage buffer_usage)
        {
            return dmGraphics::NewVertexBuffer(context, size, data, buffer_usage);
        }

        void SolDeleteVertexBuffer(dmGraphics::HVertexBuffer buffer)
        {
            dmGraphics::DeleteVertexBuffer(buffer);
        }

        void SolSetVertexBufferData(dmGraphics::HVertexBuffer buffer, ::Any any, uint32_t elements, dmGraphics::BufferUsage buffer_usage)
        {
            ::Type* t = reflect_get_any_type(any);
            void* v = (void*) reflect_get_any_value(any);
            if (v == 0 && elements != 0)
            {
                dmLogError("SolSetVertexBufferData: Wants to set data but none provided.");
                return;
            }

            if (!t->referenced_type || !t->referenced_type->array_type)
            {
                dmLogError("SolSetVertexBufferData: Reference to array not provided");
            }

            ArrayType* arr = t->referenced_type->array_type;

            uint32_t size = 0;
            if (elements != 0)
            {
                uint32_t array_len = runtime_array_length(v);

                if (array_len < elements)
                {
                    dmLogError("SolSetVertexBufferData: Invalid number of elements provided");
                    return;
                }

                size = arr->stride * elements;
            }

            dmGraphics::SetVertexBufferData(buffer, size, v, buffer_usage);
        }
    }
}

