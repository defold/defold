// Copyright 2020-2026 The Defold Foundation
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

#ifndef DM_GAMESYS_RES_RENDER_TARGET_H
#define DM_GAMESYS_RES_RENDER_TARGET_H

#include "res_texture.h"
#include <graphics/graphics.h>
#include <dmsdk/resource/resource.h>

namespace dmGameSystem
{
    struct RenderTargetResource
    {
        TextureResource*          m_ColorAttachmentResources[dmGraphics::MAX_BUFFER_COLOR_ATTACHMENTS];
        TextureResource*          m_DepthAttachmentResource;
        dmhash_t                  m_ColorAttachmentPaths[dmGraphics::MAX_BUFFER_COLOR_ATTACHMENTS];
        dmhash_t                  m_DepthAttachmentPath;
        dmGraphics::HRenderTarget m_RenderTarget;
    };

    dmResource::Result ResRenderTargetPreload(const dmResource::ResourcePreloadParams* params);
    dmResource::Result ResRenderTargetCreate(const dmResource::ResourceCreateParams* params);
    dmResource::Result ResRenderTargetDestroy(const dmResource::ResourceDestroyParams* params);
    dmResource::Result ResRenderTargetRecreate(const dmResource::ResourceRecreateParams* params);
}

#endif // DM_GAMESYS_RES_RENDER_TARGET_H
