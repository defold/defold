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

#ifndef DM_GAMESYS_COMP_COLLISION_OBJECT_BOX2D_H
#define DM_GAMESYS_COMP_COLLISION_OBJECT_BOX2D_H

#include <gameobject/component.h>
#include <dmsdk/dlib/vmath.h>

// for scripting
#include <stdint.h>
#include <physics/physics.h>

#include <gamesys/physics_ddf.h>

class b2World;
class b2Body;

namespace dmGameSystem
{
    dmGameObject::CreateResult   CompCollisionObjectBox2DNewWorld(const dmGameObject::ComponentNewWorldParams& params);
    dmGameObject::CreateResult   CompCollisionObjectBox2DDeleteWorld(const dmGameObject::ComponentDeleteWorldParams& params);
    dmGameObject::CreateResult   CompCollisionObjectBox2DCreate(const dmGameObject::ComponentCreateParams& params);
    dmGameObject::CreateResult   CompCollisionObjectBox2DDestroy(const dmGameObject::ComponentDestroyParams& params);
    dmGameObject::CreateResult   CompCollisionObjectBox2DFinal(const dmGameObject::ComponentFinalParams& params);
    dmGameObject::CreateResult   CompCollisionObjectBox2DAddToUpdate(const dmGameObject::ComponentAddToUpdateParams& params);
    dmGameObject::UpdateResult   CompCollisionObjectBox2DUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);
    dmGameObject::UpdateResult   CompCollisionObjectBox2DFixedUpdate(const dmGameObject::ComponentsUpdateParams& params, dmGameObject::ComponentsUpdateResult& update_result);
    dmGameObject::UpdateResult   CompCollisionObjectBox2DPostUpdate(const dmGameObject::ComponentsPostUpdateParams& params);
    dmGameObject::UpdateResult   CompCollisionObjectBox2DOnMessage(const dmGameObject::ComponentOnMessageParams& params);
    void                         CompCollisionObjectBox2DOnReload(const dmGameObject::ComponentOnReloadParams& params);
    dmGameObject::PropertyResult CompCollisionObjectBox2DGetProperty(const dmGameObject::ComponentGetPropertyParams& params, dmGameObject::PropertyDesc& out_value);
    dmGameObject::PropertyResult CompCollisionObjectBox2DSetProperty(const dmGameObject::ComponentSetPropertyParams& params);

    // For script_box2d.cpp
    void* CompCollisionObjectGetBox2DWorld(dmGameObject::HComponentWorld _world);
    void* CompCollisionObjectGetBox2DBody(dmGameObject::HComponent _component);
}

#endif // DM_GAMESYS_COMP_COLLISION_OBJECT_BOX2D_H
