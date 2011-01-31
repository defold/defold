#ifndef __GRAPHICS_DEVICE_OPENGL__
#define __GRAPHICS_DEVICE_OPENGL__

#include <vectormath/cpp/vectormath_aos.h>

namespace dmGraphics
{
    struct Context
    {
        Context();

        WindowResizeCallback    m_WindowResizeCallback;
        void*                   m_WindowResizeCallbackUserData;
        uint32_t                m_WindowWidth;
        uint32_t                m_WindowHeight;
        uint32_t                m_WindowOpened : 1;
    };

    struct Texture
    {
        GLuint      m_Texture;
        uint16_t    m_Width;
        uint16_t    m_Height;
    };

    struct VertexDeclaration
    {
        struct Stream
        {
            uint32_t    m_Index;
            uint32_t    m_Size;
            uint32_t    m_Usage;
            Type        m_Type;
            uint32_t    m_UsageIndex;
            uint32_t    m_Offset;
        };

        Stream      m_Streams[8];
        uint32_t    m_StreamCount;
        uint32_t    m_Stride;

    };
    struct VertexBuffer
    {
        GLuint      m_VboId;
    };

    struct IndexBuffer
    {
        GLuint      m_VboId;
    };

    struct RenderTarget
    {
        TextureParams   m_BufferTextureParams[MAX_BUFFER_TYPE_COUNT];
        HTexture        m_BufferTextures[MAX_BUFFER_TYPE_COUNT];
        GLuint          m_Id;
    };

}
#endif // __GRAPHICS_DEVICE_OPENGL__

