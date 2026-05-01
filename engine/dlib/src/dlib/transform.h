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

#ifndef DM_TRANSFORM_H
#define DM_TRANSFORM_H

#include <assert.h>
#include <dmsdk/dlib/transform.h>
#include <dmsdk/dlib/vmath.h>

namespace dmTransform
{
    /**
     * Transform with uniform (1-component) scale.
     * Transform applied as:
     * T(p) = translate(rotate(scale(p))) = p'
     */
    class TransformS1
    {
        // tx, ty, tz, scale
        dmVMath::Vector4 m_TranslationScale;
        dmVMath::Quat    m_Rotation;
    public:

        TransformS1() {}

        TransformS1(dmVMath::Vector3 translation, dmVMath::Quat rotation, float scale)
        : m_TranslationScale(translation, scale)
        , m_Rotation(rotation)
        {

        }

        inline void SetIdentity()
        {
            m_TranslationScale = dmVMath::Vector4(0.0f, 0.0f, 0.0f, 1.0f);
            m_Rotation = dmVMath::Quat::identity();
        }

        inline dmVMath::Vector3 GetTranslation() const
        {
            return m_TranslationScale.getXYZ();
        }

        inline void SetTranslation(dmVMath::Vector3 translation)
        {
            m_TranslationScale.setXYZ(translation);
        }

        inline float GetScale() const
        {
            return m_TranslationScale.getW();
        }

        inline void SetScale(float scale)
        {
            m_TranslationScale.setW(scale);
        }

        inline dmVMath::Quat GetRotation() const
        {
            return m_Rotation;
        }

        inline void SetRotation(dmVMath::Quat rotation)
        {
            m_Rotation = rotation;
        }
    };

    /**
     * Apply the transform on a point (includes the transform translation).
     * @param t Transform
     * @param p Point
     * @return Transformed point
     */
    inline dmVMath::Point3 Apply(const TransformS1& t, const dmVMath::Point3 p)
    {
        return dmVMath::Point3(rotate(t.GetRotation(), dmVMath::Vector3(p) * t.GetScale()) + t.GetTranslation());
    }

    /**
     * Apply the transform on a vector (excludes the transform translation).
     * @param t Transform
     * @param v Vector
     * @return Transformed vector
     */
    inline dmVMath::Vector3 Apply(const TransformS1& t, const dmVMath::Vector3 v)
    {
        return dmVMath::Vector3(rotate(t.GetRotation(), v * t.GetScale()));
    }

    /**
     * Transforms the right-hand transform by the left-hand transform
     * @param lhs Transforming transform
     * @param rhs Transformed transform
     * @return Transformed transform
     */
    inline TransformS1 Mul(const TransformS1& lhs, const TransformS1& rhs)
    {
        TransformS1 res;
        res.SetRotation(lhs.GetRotation() * rhs.GetRotation());
        res.SetTranslation(dmVMath::Vector3(Apply(lhs, dmVMath::Point3(rhs.GetTranslation()))));
        res.SetScale(lhs.GetScale() * rhs.GetScale());
        return res;
    }

    /**
     * Invert a transform
     * @param t Transform to invert
     * @return Inverted transform
     */
    inline TransformS1 Inv(const TransformS1& t)
    {
        assert(t.GetScale() != 0.0f && "Transform can not be inverted (0 scale).");
        TransformS1 res;
        res.SetRotation(conj(t.GetRotation()));
        res.SetScale(1.0f / t.GetScale());
        res.SetTranslation(rotate(res.GetRotation(), -t.GetTranslation()) * res.GetScale());
        return res;
    }

    /**
     * Convert a transform into a 4-dim matrix
     * @param t Transform to convert
     * @return Matrix representing the same transform
     */
    inline dmVMath::Matrix4 ToMatrix4(const TransformS1& t)
    {
        dmVMath::Matrix4 res(t.GetRotation(), t.GetTranslation());
        res = appendScale(res, dmVMath::Vector3(t.GetScale()));
        return res;
    }
}

#endif // DM_TRANSFORM_H
