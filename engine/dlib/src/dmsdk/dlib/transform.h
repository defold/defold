// Copyright 2020-2024 The Defold Foundation
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

#ifndef DMSDK_TRANSFORM_H
#define DMSDK_TRANSFORM_H

#include <assert.h>
#include <dmsdk/dlib/vmath.h>

/*# Transform API documentation
 * [file:<dmsdk/dlib/transform.h>]
 *
 * Api for transforms with rotation, scale and translation
 *
 * @document
 * @name Transform
 * @namespace dmTransform
 */

namespace dmTransform
{
    /*#
     * Transform with non-uniform (3-component) scale.
     * Transform applied as:
     * T(p) = translate(rotate(scale(p))) = p'
     * The scale is non-rotated to avoid shearing in the transform.
     * Two transforms are applied as:
     * T1(T2(p)) = t1(r1(t2(r2(s1(s2(p)))))) = p'
     * This means that the transform is not associative:
     * T1(T2(p)) != (T1*T2)(P)
     * @struct
     * @name Transform
     */
    class Transform
    {
        dmVMath::Quat    m_Rotation;
        dmVMath::Vector3 m_Translation;
        dmVMath::Vector3 m_Scale;
    public:

        /*#
         * Constructor. Leaves the struct in an uninitialized state
         * @name Transform
         */
        Transform() {}

        /*# constructor
         * @name Transform
         * @param translation [type: dmVMath::Vector3]
         * @param rotation [type: dmVMath::Quat]
         * @param scale [type: dmVMath::Vector3]
         */
        Transform(dmVMath::Vector3 translation, dmVMath::Quat rotation, dmVMath::Vector3 scale)
        : m_Rotation(rotation)
        , m_Translation(translation)
        , m_Scale(scale)
        {

        }

        /*# constructor
         * @name Transform
         * @param translation [type: dmVMath::Vector3]
         * @param rotation [type: dmVMath::Quat]
         * @param scale [type: dmVMath::Vector3]
         */
        Transform(dmVMath::Vector3 translation, dmVMath::Quat rotation, float uniformScale)
        : m_Rotation(rotation)
        , m_Translation(translation)
        , m_Scale(uniformScale)
        {

        }

        /*# initialize to identity transform
         * @name SetIdentity
         */
        inline void SetIdentity()
        {
            m_Rotation = dmVMath::Quat::identity();
            m_Translation = dmVMath::Vector3(0.0f, 0.0f, 0.0f);
            m_Scale = dmVMath::Vector3(1.0f, 1.0f, 1.0f);
        }

        /*# get translation
         * @name GetTranslation
         * @return translation [type: dmVMath::Vector3]
         */
        inline dmVMath::Vector3 GetTranslation() const
        {
            return m_Translation;
        }

        /*# set translation
         * @name SetTranslation
         * @param translation [type: dmVMath::Vector3]
         */
        inline void SetTranslation(dmVMath::Vector3 translation)
        {
            m_Translation = translation;
        }

        inline float* GetPositionPtr() const
        {
            return (float*)&m_Translation;
        }

        /*# get scale
         * @name GetScale
         * @return scale [type: dmVMath::Vector3]
         */
        inline dmVMath::Vector3 GetScale() const
        {
            return m_Scale;
        }

        /*# set scale
         * @name SetScale
         * @return scale [type: dmVMath::Vector3]
         */
        inline void SetScale(dmVMath::Vector3 scale)
        {
            m_Scale = scale;
        }

        inline float* GetScalePtr() const
        {
            return (float*)&m_Scale;
        }

        /*#
         * Compute a 'uniform' scale for this transform. In the event that the
         * scale applied to this transform is not uniform then the value is arbitrary:
         * we make a selection that will not introduce any floating point rounding errors.
         * @name GetUniformScale
         * @return scale [type: float] the uniform scale associated with this transform.
         */
        inline float GetUniformScale() const
        {
            return minElem(m_Scale);
        }

        /*# set uniform scale
         * @name SetUniformScale
         * @param scale [type: float]
         */
        inline void SetUniformScale(float scale)
        {
            m_Scale = dmVMath::Vector3(scale);
        }

        /*# get rotatiom
         * @name GetRotation
         * @return rotation [type: dmVMath::Quat]
         */
        inline dmVMath::Quat GetRotation() const
        {
            return m_Rotation;
        }

        /*# set rotatiom
         * @name SetRotation
         * @param rotation [type: dmVMath::Quat]
         */
        inline void SetRotation(dmVMath::Quat rotation)
        {
            m_Rotation = rotation;
        }

        inline float* GetRotationPtr() const
        {
            return (float*)&m_Rotation;
        }
    };

    /*#
     * Apply the transform on a point (includes the transform translation).
     * @name Apply
     * @param t [type: dmTransform::Transform&] Transform
     * @param p [type: dmVMath::Point3&] Point
     * @return point [type: dmVMath::Point3] Transformed point
     */
    inline dmVMath::Point3 Apply(const Transform& t, const dmVMath::Point3 p)
    {
        return dmVMath::Point3(rotate(t.GetRotation(), mulPerElem(dmVMath::Vector3(p), t.GetScale())) + t.GetTranslation());
    }

    /*#
     * Apply the transform on a point, but without scaling the Z-component of the point (includes the transform translation).
     * @name ApplyNoScaleZ
     * @param t [type: dmTransform::Transform&] Transform
     * @param p [type: dmVMath::Point3&] Point
     * @return point [type: dmVMath::Point3] Transformed point
     */
    inline dmVMath::Point3 ApplyNoScaleZ(const Transform& t, const dmVMath::Point3 p)
    {
        dmVMath::Vector3 s = t.GetScale();
        s.setZ(1.0f);
        return dmVMath::Point3(rotate(t.GetRotation(), mulPerElem(dmVMath::Vector3(p), s)) + t.GetTranslation());
    }

    /*#
     * Apply the transform on a vector (excludes the transform translation).
     * @name Apply
     * @param t [type: dmTransform::Transform&] Transform
     * @param v [type: dmVMath::Vector3&] Vector
     * @return point [type: dmVMath::Vector3] Transformed vector
     */
    inline dmVMath::Vector3 Apply(const Transform& t, const dmVMath::Vector3 v)
    {
        return dmVMath::Vector3(rotate(t.GetRotation(), mulPerElem(v, t.GetScale())));
    }

    /*#
     * Apply the transform on a vector, but without scaling the Z-component of the vector (excludes the transform translation).
     * @name ApplyNoScaleZ
     * @param t [type: dmTransform::Transform&] Transform
     * @param v [type: dmVMath::Vector3&] Vector
     * @return point [type: dmVMath::Vector3] Transformed vector
     */
    inline dmVMath::Vector3 ApplyNoScaleZ(const Transform& t, const dmVMath::Vector3 v)
    {
        dmVMath::Vector3 s = t.GetScale();
        s.setZ(1.0f);
        return dmVMath::Vector3(rotate(t.GetRotation(), mulPerElem(v, s)));
    }

    /*#
     * Transforms the right-hand transform by the left-hand transform
     * @name Mul
     * @param lhs [type: const dmTransform::Transform&]
     * @param rhs [type: const dmTransform::Transform&]
     * @return result [type: dmTransform::Transform] Transformed transform
     */
    inline Transform Mul(const Transform& lhs, const Transform& rhs)
    {
        Transform res;
        res.SetRotation(lhs.GetRotation() * rhs.GetRotation());
        res.SetTranslation(dmVMath::Vector3(Apply(lhs, dmVMath::Point3(rhs.GetTranslation()))));
        res.SetScale(mulPerElem(lhs.GetScale(), rhs.GetScale()));
        return res;
    }

    /*#
     * Transforms the right-hand transform by the left-hand transform, without scaling the Z-component of the transition of the transformed transform
     * @name MulNoScaleZ
     * @param lhs [type: const dmTransform::Transform&]
     * @param rhs [type: const dmTransform::Transform&]
     * @return result [type: dmTransform::Transform] Transformed transform
     */
    inline Transform MulNoScaleZ(const Transform& lhs, const Transform& rhs)
    {
        Transform res;
        res.SetRotation(lhs.GetRotation() * rhs.GetRotation());
        res.SetTranslation(dmVMath::Vector3(ApplyNoScaleZ(lhs, dmVMath::Point3(rhs.GetTranslation()))));
        res.SetScale(mulPerElem(lhs.GetScale(), rhs.GetScale()));
        return res;
    }

    /*#
     * Invert a transform
     * @name Inv
     * @param t [type: const dmTransform::Transform&]
     * @return result [type: dmTransform::Transform] inverted transform
     */
    inline Transform Inv(const Transform& t)
    {
        const dmVMath::Vector3& s = t.GetScale();
        (void)s;
        assert(s.getX() != 0.0f && s.getY() != 0.0f && s.getZ() != 0.0f && "Transform can not be inverted (0 scale-component).");
        Transform res;
        res.SetRotation(conj(t.GetRotation()));
        res.SetScale(recipPerElem(t.GetScale()));
        res.SetTranslation(mulPerElem(rotate(res.GetRotation(), -t.GetTranslation()), res.GetScale()));
        return res;
    }

    /*#
     * Convert a transform into a 4-dim matrix
     * @name ToMatrix4
     * @param t Transform to convert
     * @return Matrix representing the same transform
     */
    inline dmVMath::Matrix4 ToMatrix4(const Transform& t)
    {
        dmVMath::Matrix4 res(t.GetRotation(), t.GetTranslation());
        res = appendScale(res, t.GetScale());
        return res;
    }

    /*#
     * Extract the absolute values of the scale component from a matrix.
     * @name ExtractScale
     * @param mtx Source matrix
     * @return Vector3 with scale values for x,y,z
     */
    inline dmVMath::Vector3 ExtractScale(const dmVMath::Matrix4& mtx)
    {
        dmVMath::Vector4 col0(mtx.getCol(0));
        dmVMath::Vector4 col1(mtx.getCol(1));
        dmVMath::Vector4 col2(mtx.getCol(2));
        return dmVMath::Vector3(length(col0), length(col1), length(col2));
    }

    /*#
     * Eliminate the scaling components in a matrix
     * @name ResetScale
     * @param mtx Matrix to operate on
     * @return Vector containing the scaling by component
     */
    inline dmVMath::Vector3 ResetScale(dmVMath::Matrix4 *mtx)
    {
        dmVMath::Vector3 scale = ExtractScale(*mtx);
        if (scale.getX() == 0 || scale.getY() == 0 || scale.getZ() == 0)
            return dmVMath::Vector3(1, 1, 1);

        mtx->setCol(0, mtx->getCol(0) * (1.0f / scale.getX()));
        mtx->setCol(1, mtx->getCol(1) * (1.0f / scale.getY()));
        mtx->setCol(2, mtx->getCol(2) * (1.0f / scale.getZ()));
        return scale;
    }

    /*#
     * Convert a matrix into a transform
     * @name ToTransform
     * @param mtx Matrix4 to convert
     * @return Transform representing the same transform
     */
    inline Transform ToTransform(const dmVMath::Matrix4& mtx)
    {
        dmVMath::Matrix4 tmp(mtx);
        dmVMath::Vector4 col3(mtx.getCol(3));
        dmVMath::Vector3 scale = ResetScale(&tmp);
        return dmTransform::Transform(dmVMath::Vector3(col3.getX(), col3.getY(), col3.getZ()), dmVMath::Quat(tmp.getUpper3x3()), scale);
    }

    /*#
     * Eliminate the z scaling components in a matrix
     * @name NormalizeZScale
     * @param mtx Matrix to operate on
     */
    inline void NormalizeZScale(dmVMath::Matrix4 *mtx)
    {
        dmVMath::Vector4 col3(mtx->getCol(2));
        float zMagSqr = lengthSqr(col3);
        if (zMagSqr > 0.0f)
        {
             mtx->setCol(2, col3 * (1.0f / sqrtf(zMagSqr)));
        }
    }

    /*#
     * Eliminate the z scaling components in a matrix
     * @name NormalizeZScale
     * @param source [type: const dmVMath::Matrix&] Source matrix
     * @param target [type: dmVMath::Matrix*] Target matrix
     */
    inline void NormalizeZScale(dmVMath::Matrix4 const &source, dmVMath::Matrix4 *target)
    {
        *target = source;
        NormalizeZScale(target);
    }

    /*#
     * Multiply two matrices without z-scaling the translation in m2
     * @name MulNoScaleZ
     * @param m1 [type: const dmVMath::Matrix&] First matrix
     * @param m2 [type: const dmVMath::Matrix&] Second matrix
     * @return result [type: dmVMath::Matrix] The resulting transform
     */
    inline dmVMath::Matrix4 MulNoScaleZ(dmVMath::Matrix4 const &m1, dmVMath::Matrix4 const &m2)
    {
        // tmp is the non-z scaling version of m1
        dmVMath::Matrix4 tmp, res;
        NormalizeZScale(m1, &tmp);
        res = m1 * m2;
        res.setCol(3, tmp * m2.getCol(3));
        return res;
    }
}

#endif // DMSDK_TRANSFORM_H
