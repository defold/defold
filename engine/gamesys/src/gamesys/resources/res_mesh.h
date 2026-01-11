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

#ifndef DM_GAMESYS_RES_MESH_H
#define DM_GAMESYS_RES_MESH_H

#include <dmsdk/resource/resource.h>
#include <dmsdk/gamesys/resources/res_buffer.h>
#include <dmsdk/gamesys/resources/res_mesh.h>
#include <dmsdk/graphics/graphics.h>

namespace dmGameSystem
{
    dmResource::Result ResMeshPreload(const dmResource::ResourcePreloadParams* params);

    dmResource::Result ResMeshCreate(const dmResource::ResourceCreateParams* params);

    dmResource::Result ResMeshDestroy(const dmResource::ResourceDestroyParams* params);

    dmResource::Result ResMeshRecreate(const dmResource::ResourceRecreateParams* params);

    bool BuildVertexDeclaration(BufferResource* buffer_resource, dmGraphics::HVertexDeclaration* out_vert_decl);
}

#endif // DM_GAMESYS_RES_MESH_H
