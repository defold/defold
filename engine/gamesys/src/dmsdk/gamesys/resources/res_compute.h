// Copyright 2020-2026 The Defold Foundation
// Licensed under the Defold License version 1.0
#ifndef DMSDK_GAMESYS_RES_COMPUTE_H
#define DMSDK_GAMESYS_RES_COMPUTE_H

#include <dmsdk/render/render.h>

namespace dmGameSystem
{
    struct TextureResource;
    /*# Runtime representation of a compiled .compute resource. The resource
     * factory owns this object and all referenced textures. */
    struct ComputeResource
    {
        dmRender::HComputeProgram m_Program;
        TextureResource*          m_Textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        dmhash_t                  m_TextureResourcePaths[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        dmhash_t                  m_SamplerNames[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        uint32_t                  m_NumTextures;
    };
}

#endif
