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

#ifndef DM_GAMESYS_COMP_COLLISION_OBJECT_BULLET3D_H
#define DM_GAMESYS_COMP_COLLISION_OBJECT_BULLET3D_H

#include <gameobject/component.h>
#include <dmsdk/dlib/vmath.h>

// for scripting
#include <stdint.h>
#include <physics/physics.h>

#include <gamesys/physics_ddf.h>

namespace dmGameSystem
{
    dmGameObject::CreateResult   CompCollisionObjectBullet3DNewWorld(const dmGameObject::ComponentNewWorldParams& params);
    dmGameObject::CreateResult   CompCollisionObjectBullet3DDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);
    dmGameObject::CreateResult   CompCollisionObjectBullet3DCreate(const dmGameObject::ComponentCreateParams& params);
    dmGameObject::CreateResult   CompCollisionObjectBullet3DDestroy(const dmGameObject::ComponentDestroyParams& params);
    dmGameObject::CreateResult   CompCollisionObjectBullet3DFinal(const dmGameObject::ComponentFinalParams& params);
    dmGameObject::CreateResult   CompCollisionObjectBullet3DAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);
    dmGameObject::UpdateResult   CompCollisionObjectBullet3DUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);
    dmGameObject::UpdateResult   CompCollisionObjectBullet3DFixedUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);
    dmGameObject::UpdateResult   CompCollisionObjectBullet3DPostUpdate(const dmGameObject::ComponentsPostUpdateParams& params);
    dmGameObject::UpdateResult   CompCollisionObjectBullet3DOnMessage(const dmGameObject::ComponentOnMessageParams& params);
    void                         CompCollisionObjectBullet3DOnReload(const dmGameObject::ComponentOnReloadParams& params);
    dmGameObject::PropertyResult CompCollisionObjectBullet3DGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);
    dmGameObject::PropertyResult CompCollisionObjectBullet3DSetProperty(const dmGameObject::ComponentSetPropertyParams& params);
}

#endif // DM_GAMESYS_COMP_COLLISION_OBJECT_BULLET3D_H
