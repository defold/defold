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

#ifndef DM_GAMESYS_SCRIPT_BOX2D_H
#define DM_GAMESYS_SCRIPT_BOX2D_H

#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/vmath.h>
#include <Box2D/Common/b2Math.h>

namespace dmGameObject
{
    typedef struct CollectionHandle* HCollection;
}

namespace dmGameSystem
{
    float GetPhysicsScale();
    float GetInvPhysicsScale();

    b2Vec2           CheckVec2(struct lua_State* L, int index, float scale);
    dmVMath::Vector3 FromB2(const b2Vec2& p, float inv_scale);

    void    PushWorld(struct lua_State* L, class b2World* world);

    void            PushBody(struct lua_State* L, class b2Body* body, dmGameObject::HCollection collection, dmhash_t gameobject_id);
    class b2Body*   CheckBody(lua_State* L, int index);

    void    ScriptBox2DInitializeBody(struct lua_State* L);
}

#endif // DM_GAMESYS_SCRIPT_BOX2D_H
