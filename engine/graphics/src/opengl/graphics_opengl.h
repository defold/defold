#ifndef __GRAPHICS_DEVICE_OPENGL__
#define __GRAPHICS_DEVICE_OPENGL__

#include <dlib/math.h>
#include <dlib/mutex.h>
#include <vectormath/cpp/vectormath_aos.h>

namespace dmGraphics
{
    struct Context
    {
        Context(const ContextParams& params);

        WindowResizeCallback    m_WindowResizeCallback;
        void*                   m_WindowResizeCallbackUserData;
        WindowCloseCallback     m_WindowCloseCallback;
        void*                   m_WindowCloseCallbackUserData;
        WindowFocusCallback     m_WindowFocusCallback;
        void*                   m_WindowFocusCallbackUserData;
        uint32_t                m_Width;
        uint32_t                m_Height;
        uint32_t                m_WindowWidth;
        uint32_t                m_WindowHeight;
        uint32_t                m_Dpi;
        uint32_t                m_MaxTextureSize;
        TextureFilter           m_DefaultTextureMinFilter;
        TextureFilter           m_DefaultTextureMagFilter;
        // Counter to keep track of various modifications. Used for cache flush etc
        // Version zero is never used
        uint32_t                m_ModificationVersion;
        uint32_t                m_TextureFormatSupport;
        uint32_t                m_IndexBufferFormatSupport;
        uint32_t                m_DepthBufferBits;
        uint32_t                m_PackedDepthStencil : 1;
        uint32_t                m_WindowOpened : 1;
        uint32_t                m_VerifyGraphicsCalls : 1;

        // Async queue data and synchronization objects
        dmMutex::Mutex          m_AsyncMutex;
    };

    static inline void IncreaseModificationVersion(Context* context)
    {
        ++context->m_ModificationVersion;
        context->m_ModificationVersion = dmMath::Max(0U, context->m_ModificationVersion);
    }

    struct Texture
    {
        TextureType m_Type;
        GLuint      m_Texture;
        uint16_t    m_Width;
        uint16_t    m_Height;
        uint16_t    m_OriginalWidth;
        uint16_t    m_OriginalHeight;

        // data state per mip-map (mipX = bitX). 0=ok, 1=pending
        volatile uint16_t    m_DataState;

        TextureParams m_Params;
    };

    struct VertexDeclaration
    {
        struct Stream
        {
            const char* m_Name;
            uint16_t    m_LogicalIndex;
            int16_t     m_PhysicalIndex;
            uint16_t    m_Size;
            uint16_t    m_Offset;
            Type        m_Type;
            bool        m_Normalize;
        };

        Stream      m_Streams[8];
        uint16_t    m_StreamCount;
        uint16_t    m_Stride;
        HProgram    m_BoundForProgram;
        uint32_t    m_ModificationVersion;

    };
    // TODO: Why this one here!? Not used?
    struct VertexBuffer
    {
        GLuint      m_VboId;
    };

    // TODO: Why this one here!? Not used?
    struct IndexBuffer
    {
        GLuint      m_VboId;
    };

    struct RenderTarget
    {
        TextureParams   m_BufferTextureParams[MAX_BUFFER_TYPE_COUNT];
        HTexture        m_ColorBufferTexture;
        GLuint          m_DepthBuffer;
        GLuint          m_StencilBuffer;
        GLuint          m_DepthStencilBuffer;
        GLuint          m_Id;
        uint32_t        m_BufferTypeFlags;
        uint32_t        m_DepthBufferBits;
    };

}
#endif // __GRAPHICS_DEVICE_OPENGL__

