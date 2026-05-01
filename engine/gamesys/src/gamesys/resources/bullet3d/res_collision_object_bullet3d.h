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

#ifndef DM_GAMESYS_RES_COLLISION_OBJECT_BULLET3D_H
#define DM_GAMESYS_RES_COLLISION_OBJECT_BULLET3D_H

#include <dmsdk/resource/resource.hpp>
#include <dmsdk/gamesys/resources/res_collision_object.h>

namespace dmGameSystem
{
    dmResource::Result ResCollisionObjectBullet3DCreate(const dmResource::ResourceCreateParams* params);

    dmResource::Result ResCollisionObjectBullet3DDestroy(const dmResource::ResourceDestroyParams* params);

    dmResource::Result ResCollisionObjectBullet3DRecreate(const dmResource::ResourceRecreateParams* params);
}
#endif
