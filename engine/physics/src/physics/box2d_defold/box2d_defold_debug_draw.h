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

#ifndef DM_BOX2d_DEFOLD_DEBUG_DRAW_H
#define DM_BOX2d_DEFOLD_DEBUG_DRAW_H

#include "Box2D/Box2D.h"

#include "../physics.h"

namespace dmPhysics
{
    class DebugDraw2D : public b2Draw
    {
    public:
        DebugDraw2D(DebugCallbacks* callbacks);

        /// Draw a closed polygon provided in CCW order.
        virtual void DrawPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color);

        /// Draw a solid closed polygon provided in CCW order.
        virtual void DrawSolidPolygon(const b2Vec2* vertices, int32 vertexCount, const b2Color& color);

        /// Draw a circle.
        virtual void DrawCircle(const b2Vec2& center, float32 radius, const b2Color& color);

        /// Draw a solid circle.
        virtual void DrawSolidCircle(const b2Vec2& center, float32 radius, const b2Vec2& axis, const b2Color& color);

        /// Draw a line segment.
        virtual void DrawSegment(const b2Vec2& p1, const b2Vec2& p2, const b2Color& color);

        /// Draw a transform. Choose your own length scale.
        /// @param xf a transform.
        virtual void DrawTransform(const b2Transform& xf);

        /// Draw an arrow. Choose your own length scale.
        /// @param p position.
        /// @param d direction.
        virtual void DrawArrow(const b2Vec2& p, const b2Vec2& d, const b2Color& color);

    private:
        DebugCallbacks* m_Callbacks;
    };
}

#endif // DM_BOX2d_DEFOLD_DEBUG_DRAW_H
