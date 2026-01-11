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

#ifndef PHYSICS_DEBUG_DRAW_H
#define PHYSICS_DEBUG_DRAW_H

#define _USE_MATH_DEFINES
#include <math.h>

#include <LinearMath/btVector3.h>
#include <LinearMath/btIDebugDraw.h>

#include "physics.h"

namespace dmPhysics
{
    class DebugDraw3D : public btIDebugDraw
    {
    public:
        DebugDraw3D(DebugCallbacks* callbacks);
        virtual ~DebugDraw3D();

        virtual void drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &color);
        virtual void drawContactPoint(const btVector3 &PointOnB, const btVector3 &normalOnB, btScalar distance, int lifeTime, const btVector3 &color);
        virtual void reportErrorWarning(const char *warningString);
        virtual void draw3dText(const btVector3 &location, const char *textString);
        virtual void setDebugMode(int debugMode);
        virtual int getDebugMode() const;

    private:
        DebugCallbacks* m_Callbacks;
        int m_DebugMode;
    };
}

#endif
