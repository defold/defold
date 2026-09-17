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

#ifndef DM_BOX2D_PHYSICS_H
#define DM_BOX2D_PHYSICS_H

#include <box2d/box2d.h>

#include <dlib/array.h>
#include <dlib/hashtable.h>
#include <dmsdk/dlib/vmath.h>

#include "box2d_debug_draw.h"
#include "../physics.h"
#include "../physics_private.h"

// These values are from the original Box2D implementation, I think the newer version has different constants.
// i.e B2_LINEAR_SLOP

/// A small length used as a collision and constraint tolerance. Usually it is
/// chosen to be numerically significant, but visually insignificant.
#define b2_linearSlop           0.005f
/// The radius of the polygon/edge shape skin. This should not be modified. Making
/// this smaller means polygons will have an insufficient buffer for continuous collision.
/// Making it larger may create artifacts for vertex collision.
#define b2_polygonRadius        (2.0f * b2_linearSlop)

namespace dmPhysics
{
    static const uint32_t B2GRIDSHAPE_EMPTY_CELL = 0xffffffff;

    enum ShapeType
    {
        SHAPE_TYPE_CIRCLE,
        SHAPE_TYPE_POLYGON,
        SHAPE_TYPE_GRID,
    };

    struct ShapeData;
    struct ShapeData
    {
        ShapeData*  m_Resource;
        b2ShapeId   m_ShapeId;
        b2Vec2      m_CreationPosition;
        ShapeType   m_Type;
        float       m_CreationScale;
        float       m_LastScale;
    };

    struct CircleShapeData
    {
        ShapeData m_ShapeDataBase;
        b2Circle  m_Circle;
    };

    struct PolygonShapeData
    {
        ShapeData m_ShapeDataBase;
        b2Vec2    m_VerticesOriginal[B2_MAX_POLYGON_VERTICES];
        b2Vec2    m_CentroidOriginal;
        b2Polygon m_Polygon;
    };

    struct HullSet;
    struct GridShapeData
    {
        struct Cell
        {
            // Index to hull in hull-set
            uint32_t m_Index;
        };

        struct Filter
        {
            uint16_t m_Group;
            uint16_t m_Mask;
        };

        ShapeData  m_ShapeDataBase;
        b2ShapeDef m_ShapeDef;
        HullSet*   m_HullSet;
        Cell*      m_Cells;
        HullFlags* m_CellFlags;
        b2ShapeId* m_CellPolygonShapes;
        Filter*    m_CellFilters;
        b2Vec2     m_Position;
        float      m_Radius;
        float      m_CellWidth;
        float      m_CellHeight;
        uint32_t   m_RowCount;
        uint32_t   m_ColumnCount;
    };

    struct Body
    {
        b2BodyId    m_BodyId;
        // Grids represent each layer as a separate body
        ShapeData** m_Shapes;
        uint8_t     m_ShapeCount;
    };

    struct ContactPair
    {
        uint64_t      m_ShapeIdA;
        uint64_t      m_ShapeIdB;
        b2ContactData m_Data;
    };

    struct World2D
    {
        World2D(HContext2D context, const NewWorldParams& params);

        OverlapCache                m_TriggerOverlaps;
        HContext2D                  m_Context;
        b2WorldId                   m_WorldId;
        dmArray<RayCastRequest>     m_RayCastRequests;
        DebugDraw2D                 m_DebugDraw;
        GetWorldTransformCallback   m_GetWorldTransformCallback;
        SetWorldTransformCallback   m_SetWorldTransformCallback;

        dmArray<Body*>              m_Bodies;
        dmArray<ContactPair>        m_ContactBuffer;

        // TODO: I think we can merge these into a single buffer of bytes
        dmArray<b2ShapeId>          m_GetShapeScratchBuffer;
        dmArray<b2ContactData>      m_GetContactsScratchBuffer;
        dmArray<b2ShapeId>          m_GetSensorOverlapsScratchBuffer;

        uint8_t                     m_AllowDynamicTransforms:1;
        uint8_t                     :7;
    };

    struct Context2D
    {
        Context2D();

        dmArray<World2D*>           m_Worlds;
        DebugCallbacks              m_DebugCallbacks;
        b2Vec2                      m_Gravity;
        dmMessage::HSocket          m_Socket;
        float                       m_Scale;
        float                       m_InvScale;
        float                       m_ContactImpulseLimit;
        float                       m_TriggerEnterLimit;
        float                       m_VelocityThreshold;
        int                         m_RayCastLimit;
        int                         m_TriggerOverlapCapacity;
        uint8_t                     m_AllowDynamicTransforms:1;
        uint8_t                     :7;
    };

    inline void ToB2(const dmVMath::Point3& p0, b2Vec2& p1, float scale)
    {
        p1.x = p0.getX() * scale;
        p1.y = p0.getY() * scale;
    }

    inline void ToB2(const dmVMath::Vector3& p0, b2Vec2& p1, float scale)
    {
        p1.x = p0.getX() * scale;
        p1.y = p0.getY() * scale;
    }

    inline void FromB2(const b2Vec2& p0, dmVMath::Vector3& p1, float inv_scale)
    {
        p1.setX(p0.x * inv_scale);
        p1.setY(p0.y * inv_scale);
        p1.setZ(0.0f);
    }

    inline void FromB2(const b2Vec2& p0, dmVMath::Point3& p1, float inv_scale)
    {
        p1.setX(p0.x * inv_scale);
        p1.setY(p0.y * inv_scale);
        p1.setZ(0.0f);
    }

    inline b2Vec2 TransformScaleB2(const b2Transform& t, float scale, const b2Vec2& p)
    {
        b2Vec2 pp = p;
        pp *= scale;
        return b2TransformPoint(t, pp);
    }

    inline b2Vec2 FromTransformScaleB2(const b2Transform& t, float inv_scale, const b2Vec2& p)
    {
        b2Vec2 pp = p;
        pp *= inv_scale;
        return b2InvTransformPoint(t, pp);
    }
}

#endif // DM_PHYSICS_2D_H
