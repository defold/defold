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

#ifndef DM_GAMESYS_SCRIPT_BOX2D_V2_H
#define DM_GAMESYS_SCRIPT_BOX2D_V2_H

#include <Box2D/Collision/Shapes/b2ChainShape.h>
#include <Box2D/Collision/Shapes/b2CircleShape.h>
#include <Box2D/Collision/Shapes/b2EdgeShape.h>
#include <Box2D/Collision/Shapes/b2PolygonShape.h>

#include <dlib/array.h>

#include "../script_box2d.h"

class b2Body;
class b2Fixture;
struct b2FixtureDef;

namespace dmGameSystem
{
    struct FixtureShapeDef
    {
        b2CircleShape m_Circle;
        b2EdgeShape m_Edge;
        b2PolygonShape m_Polygon;
        b2ChainShape m_Chain;
        dmArray<b2Vec2> m_Vertices;
    };

    b2Body*    CheckBody(struct lua_State* L, int index);
    b2Fixture* GetFixtureByIndex(b2Body* body, int fixture_index);
    void       PushBodyFromReference(struct lua_State* L, b2Body* body, int reference_index);
    void       TrackOwnedFixtureShape(b2Fixture* fixture, FixtureShapeDef* shape_def);
    void       ReleaseOwnedFixtureShape(b2Fixture* fixture);
    const b2Shape* CheckShapeDef(struct lua_State* L, int index, FixtureShapeDef* out_shape);
    void       CheckFixtureDef(struct lua_State* L, int index, FixtureShapeDef* out_shape, b2FixtureDef* out_fixture_def);
    void       ScriptBox2DInitializeBody(struct lua_State* L);
    void       ScriptBox2DInvalidateBody(void* body);
    void       ScriptBox2DFinalizeBody();
    void       ScriptBox2DInitializeFixture(struct lua_State* L);
    void       ScriptBox2DInitializeShape(struct lua_State* L);
}

#endif // DM_GAMESYS_SCRIPT_BOX2D_V2_H
