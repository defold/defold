// Copyright 2020-2025 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DM_GAMESYS_RES_COMPUTE_H
#define DM_GAMESYS_RES_COMPUTE_H

#include <render/render.h>

#include <dmsdk/resource/resource.h>

namespace dmGameSystem
{
    struct TextureResource;
    struct ComputeResource
    {
        dmRender::HComputeProgram m_Program;
        TextureResource*          m_Textures[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        dmhash_t                  m_SamplerNames[dmRender::RenderObject::MAX_TEXTURE_COUNT];
        uint32_t                  m_NumTextures;
    };

    dmResource::Result ResComputeCreate(const dmResource::ResourceCreateParams* params);
    dmResource::Result ResComputeDestroy(const dmResource::ResourceDestroyParams* params);
    dmResource::Result ResComputeRecreate(const dmResource::ResourceRecreateParams* params);
    dmResource::Result ResComputePreload(const dmResource::ResourcePreloadParams* params);
}

#endif // DM_GAMESYS_RES_COMPUTE_H
