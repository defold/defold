#ifndef GRAPHICS_DEVICE_NULL
#define GRAPHICS_DEVICE_NULL

namespace dmGraphics
{
    struct Texture
    {
        void* m_Data;
        TextureFormat m_Format;
        uint32_t m_Width;
        uint32_t m_Height;
    };

    struct VertexStream
    {
        const void* m_Source;
        void* m_Buffer;
        uint16_t m_Size;
        uint16_t m_Stride;
    };

    const static uint32_t MAX_VERTEX_STREAM_COUNT = 8;
    const static uint32_t MAX_REGISTER_COUNT = 16;

    struct RenderBuffer
    {
        void*   m_ColorBuffer;
        void*   m_DepthBuffer;
        void*   m_AccumBuffer;
        void*   m_StencilBuffer;
    };

    struct Device
    {
        VertexStream                m_VertexStreams[MAX_VERTEX_STREAM_COUNT];
        Vectormath::Aos::Vector4    m_VertexProgramRegisters[MAX_REGISTER_COUNT];
        Vectormath::Aos::Vector4    m_FragmentProgramRegisters[MAX_REGISTER_COUNT];
        RenderBuffer                m_FrameBuffer;
        RenderBuffer*               m_RenderBuffer;
        void*                       m_VertexProgram;
        void*                       m_FragmentProgram;
        uint32_t                    m_DisplayWidth;
        uint32_t                    m_DisplayHeight;
        uint32_t                    m_Opened : 1;
    };

    struct Context
    {
        Vectormath::Aos::Matrix4 m_ViewMatrix;
    };

    struct VertexDeclaration
    {
        VertexElement m_Elements[MAX_VERTEX_STREAM_COUNT];
    };

    struct VertexBuffer
    {
        char* m_Buffer;
        char* m_Copy;
        uint32_t m_Size;
    };

    struct IndexBuffer
    {
        char* m_Buffer;
        char* m_Copy;
        uint32_t m_Size;
    };

    struct RenderTarget
    {
        RenderBuffer    m_RenderBuffer;
        Texture*        m_Texture;
    };

}

#endif // __GRAPHICS_DEVICE_NULL__
