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

#ifndef DM_GAMESYS_SCRIPT_BOX2D_V3_H
#define DM_GAMESYS_SCRIPT_BOX2D_V3_H

#include <box2d/box2d.h>

#include "../script_box2d.h"

namespace dmGameSystem
{
    enum
    {
        SHAPE_TYPE_BOX  = b2_polygonShape,
        SHAPE_TYPE_EDGE = b2_segmentShape,
    };

    struct B2DShapeDef
    {
        enum Type
        {
            TYPE_CIRCLE,
            TYPE_CAPSULE,
            TYPE_SEGMENT,
            TYPE_POLYGON,
        };

        Type m_Type;
        union
        {
            b2Circle  m_Circle;
            b2Capsule m_Capsule;
            b2Segment m_Segment;
            b2Polygon m_Polygon;
        };
    };

    static inline int NormalizeShapeTypeForLua(b2ShapeType type)
    {
        switch (type)
        {
            case b2_segmentShape:
            case b2_chainSegmentShape:
                return SHAPE_TYPE_EDGE;
            default:
                return type;
        }
    }

    b2BodyId*   CheckBody(struct lua_State* L, int index);
    b2ShapeId   GetShapeByIndex(b2BodyId body, int shape_index);
    B2DShapeDef CheckShapeDef(struct lua_State* L, int index);
    void        CheckShapeCreateDef(struct lua_State* L, int index, B2DShapeDef* out_shape_def, b2ShapeDef* out_shape_create_def);
    void        PushShape(struct lua_State* L, b2ShapeId shape_id);
    void        ScriptBox2DInitializeBody(struct lua_State* L);
    void        ScriptBox2DFinalizeBody();
    void        ScriptBox2DInitializeShape(struct lua_State* L);
}

#endif // DM_GAMESYS_SCRIPT_BOX2D_V3_H
