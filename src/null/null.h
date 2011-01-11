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

    struct FrameBuffer
    {
        void*   m_ColorBuffer;
        void*   m_DepthBuffer;
        void*   m_StencilBuffer;
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
        FrameBuffer m_FrameBuffer;
        HTexture    m_BufferTextures[MAX_BUFFER_TYPE_COUNT];
    };

    struct Context
    {
        VertexStream                m_VertexStreams[MAX_VERTEX_STREAM_COUNT];
        Vectormath::Aos::Vector4    m_VertexProgramRegisters[MAX_REGISTER_COUNT];
        Vectormath::Aos::Vector4    m_FragmentProgramRegisters[MAX_REGISTER_COUNT];
        FrameBuffer                 m_MainFrameBuffer;
        FrameBuffer*                m_CurrentFrameBuffer;
        void*                       m_VertexProgram;
        void*                       m_FragmentProgram;
        uint32_t                    m_Width;
        uint32_t                    m_Height;
        uint32_t                    m_StencilMask;
        uint32_t                    m_Opened : 1;
        uint32_t                    m_RedMask : 1;
        uint32_t                    m_GreenMask : 1;
        uint32_t                    m_BlueMask : 1;
        uint32_t                    m_AlphaMask : 1;
        uint32_t                    m_DepthMask : 1;
    };
}

#endif // __GRAPHICS_DEVICE_NULL__
