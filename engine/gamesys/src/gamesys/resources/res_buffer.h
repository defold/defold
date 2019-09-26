#ifndef DM_GAMESYS_RES_BUFFER_H
#define DM_GAMESYS_RES_BUFFER_H

#include <stdint.h>

#include <resource/resource.h>
#include <dlib/buffer.h>
#include "buffer_ddf.h"

namespace dmGameSystem
{
    struct BufferResource
    {
        dmBufferDDF::BufferDesc* m_BufferDDF;
        dmBuffer::HBuffer        m_Buffer;
        uint64_t                 m_ElementCount;
        uint32_t                 m_Version;
        // dmBuffer
        // RigSceneResource*       m_RigScene;
        // dmRender::HMaterial     m_Material;
        // dmGraphics::HVertexBuffer m_VertexBuffer;
        // dmGraphics::HIndexBuffer m_IndexBuffer;
        // dmGraphics::HTexture    m_Textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        // dmhash_t                m_TexturePaths[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        // dmGraphics::Type        m_IndexBufferElementType;
        // uint32_t                m_ElementCount;
    };

    dmResource::Result ResBufferPreload(const dmResource::ResourcePreloadParams& params);

    dmResource::Result ResBufferCreate(const dmResource::ResourceCreateParams& params);

    dmResource::Result ResBufferDestroy(const dmResource::ResourceDestroyParams& params);

    dmResource::Result ResBufferRecreate(const dmResource::ResourceRecreateParams& params);
}

#endif // DM_GAMESYS_RES_BUFFER_H
