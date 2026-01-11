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

#include "debug_draw_3d.h"

#include <dlib/log.h>

#include "physics_3d.h"

namespace dmPhysics
{
    DebugDraw3D::DebugDraw3D(DebugCallbacks* callbacks)
    : m_Callbacks(callbacks)
    , m_DebugMode(0)
    {
    }

    DebugDraw3D::~DebugDraw3D()
    {

    }

    void DebugDraw3D::drawLine(const btVector3 &from, const btVector3 &to, const btVector3 &color)
    {
        if (m_Callbacks->m_DrawLines != 0x0)
        {
            float inv_scale = m_Callbacks->m_InvScale;
            dmVMath::Point3 points[2];
            FromBt(from, points[0], inv_scale);
            FromBt(to, points[1], inv_scale);
            (*m_Callbacks->m_DrawLines)(points, 2, dmVMath::Vector4(color.getX(), color.getY(), color.getZ(), m_Callbacks->m_Alpha), m_Callbacks->m_UserData);
        }
    }

    void DebugDraw3D::drawContactPoint(const btVector3 &pointOnB, const btVector3 &normalOnB, btScalar distance, int lifeTime, const btVector3 &color)
    {
        dmVMath::Point3 p;
        FromBt(pointOnB, p, m_Callbacks->m_InvScale);
        dmVMath::Vector3 n;
        FromBt(normalOnB, n, 1.0f); // Don't scale normals
        assert(lengthSqr(n) > 0.0f);
        dmVMath::Vector3 t1;
        if (fabs(n.getX()) < fabs(n.getY()))
        {
            t1 = dmVMath::Vector3::xAxis();
        }
        else
        {
            t1 = dmVMath::Vector3::yAxis();
        }
        dmVMath::Vector3 t2 = cross(n, t1);
        assert(lengthSqr(t2) > 0.0f);
        t2 = normalize(t2);
        t1 = cross(t2, n);
        // lifetime measures number of frames the contact point has existed (max 127)
        float alpha = m_Callbacks->m_Alpha * (1.0f - (lifeTime/255.0f));
        dmVMath::Vector4 c(color.getX(), color.getY(), color.getZ(), alpha);
        dmVMath::Point3 points[10] =
        {
            p, p + n - t1,
            p, p + n + t1,
            p, p + n - t2,
            p, p + n + t2,
            p, p + distance * n
        };
        (*m_Callbacks->m_DrawLines)(points, 10, c, m_Callbacks->m_UserData);
    }

    void DebugDraw3D::reportErrorWarning(const char *warningString)
    {
        dmLogWarning("%s", warningString);
    }

    void DebugDraw3D::draw3dText(const btVector3 &location, const char *textString)
    {
        dmVMath::Point3 pos;
        FromBt(location, pos, m_Callbacks->m_InvScale);
        dmLogInfo("[%.2f, %.2f, %.2f]: %s\n", pos.getX(), pos.getY(), pos.getZ(), textString);
    }

    void DebugDraw3D::setDebugMode(int debugMode)
    {
        m_DebugMode = debugMode;
    }

    int DebugDraw3D::getDebugMode() const
    {
        return m_DebugMode;
    }
}
