#ifndef GRAPHICS_DEVICE_VULKAN
#define GRAPHICS_DEVICE_VULKAN

namespace dmGraphics
{
    struct Texture
    {
    	uint16_t      m_MipMapCount;
        uint32_t      m_Width;
        uint32_t      m_Height;
        uint32_t      m_OriginalWidth;
        uint32_t      m_OriginalHeight;
        TextureType   m_Type;
        void*         m_Texture;
        TextureParams m_Params;
    };

    const static uint32_t MAX_VERTEX_STREAM_COUNT = 8;
    const static uint32_t MAX_REGISTER_COUNT      = 16;
    const static uint32_t MAX_TEXTURE_COUNT       = 32;

    struct VertexDeclaration
    {
        struct Stream
        {
            const char* m_Name;
            uint16_t    m_DescriptorIndex;
            uint16_t    m_Size;
            uint16_t    m_Offset;
            Type        m_Type;
            // bool        m_Normalize; // Not sure how to deal in VK
        };

        uint64_t    m_Hash;
        Stream      m_Streams[MAX_VERTEX_STREAM_COUNT];
        uint16_t    m_StreamCount;
        uint16_t    m_Stride;
    };

    struct RenderTarget
    {
        TextureParams m_BufferTextureParams[MAX_BUFFER_TYPE_COUNT];
        void*         m_ColorTexture;
        void*         m_DepthStencilTexture;
        uint16_t      m_RenderTargetObject;
    };

    struct Viewport
    {
        uint16_t m_X;
        uint16_t m_Y;
        uint16_t m_W;
        uint16_t m_H;
        bool     m_HasChanged;
    };

    struct Context
    {
        Context(const ContextParams& params);

        uint8_t m_ViewportChanged;
        uint8_t m_CullFaceChanged;

        Vectormath::Aos::Vector4    m_ProgramRegisters[MAX_REGISTER_COUNT];
        Viewport                    m_Viewport;

        void*                       m_CurrentProgram;
        void*                       m_CurrentVertexBuffer;
        void*                       m_CurrentIndexBuffer;
        void*                       m_CurrentVertexDeclaration;
        WindowResizeCallback        m_WindowResizeCallback;
        void*                       m_WindowResizeCallbackUserData;
        WindowCloseCallback         m_WindowCloseCallback;
        void*                       m_WindowCloseCallbackUserData;
        WindowFocusCallback         m_WindowFocusCallback;
        void*                       m_WindowFocusCallbackUserData;
        Type                        m_CurrentIndexBufferType;
        TextureFilter               m_DefaultTextureMinFilter;
        TextureFilter               m_DefaultTextureMagFilter;
        uint32_t                    m_Width;
        uint32_t                    m_Height;
        uint32_t                    m_WindowWidth;
        uint32_t                    m_WindowHeight;
        uint32_t                    m_Dpi;
        int32_t                     m_ScissorRect[4];
        uint32_t                    m_TextureFormatSupport;
        uint32_t                    m_WindowOpened : 1;
        // Only use for testing
        uint32_t                    m_RequestWindowClose : 1;
    };
}

#endif // __GRAPHICS_DEVICE_VULKAN__
