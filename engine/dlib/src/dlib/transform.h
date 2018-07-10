#ifndef DM_TRANSFORM_H
#define DM_TRANSFORM_H

#include <assert.h>
#include <vectormath/cpp/vectormath_aos.h>
#include "math.h"

namespace dmTransform
{
    using namespace Vectormath::Aos;

    /**
     * Transform with uniform (1-component) scale.
     * Transform applied as:
     * T(p) = translate(rotate(scale(p))) = p'
     */
    class TransformS1
    {
        // tx, ty, tz, scale
        Vector4 m_TranslationScale;
        Quat    m_Rotation;
    public:

        TransformS1() {}

        TransformS1(Vector3 translation, Quat rotation, float scale)
        : m_TranslationScale(translation, scale)
        , m_Rotation(rotation)
        {

        }

        inline void SetIdentity()
        {
            m_TranslationScale = Vector4(0.0f, 0.0f, 0.0f, 1.0f);
            m_Rotation = Quat::identity();
        }

        inline Vector3 GetTranslation() const
        {
            return m_TranslationScale.getXYZ();
        }

        inline void SetTranslation(Vector3 translation)
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

        inline Quat GetRotation() const
        {
            return m_Rotation;
        }

        inline void SetRotation(Quat rotation)
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
    inline Point3 Apply(const TransformS1& t, const Point3 p)
    {
        return Point3(rotate(t.GetRotation(), Vector3(p) * t.GetScale()) + t.GetTranslation());
    }

    /**
     * Apply the transform on a point, but without scaling the Z-component of the point (includes the transform translation).
     * @param t Transform
     * @param p Point
     * @return Transformed point
     */
    inline Point3 ApplyNoScaleZ(const TransformS1& t, const Point3 p)
    {
        return Point3(rotate(t.GetRotation(), mulPerElem(Vector3(t.GetScale(), t.GetScale(), 1.0f), Vector3(p))) + t.GetTranslation());
    }

    /**
     * Apply the transform on a vector (excludes the transform translation).
     * @param t Transform
     * @param v Vector
     * @return Transformed vector
     */
    inline Vector3 Apply(const TransformS1& t, const Vector3 v)
    {
        return Vector3(rotate(t.GetRotation(), v * t.GetScale()));
    }

    /**
     * Apply the transform on a vector, but without scaling the Z-component of the vector (excludes the transform translation).
     * @param t Transform
     * @param v Vector
     * @return Transformed vector
     */
    inline Vector3 ApplyNoScaleZ(const TransformS1& t, const Vector3 v)
    {
        return Vector3(rotate(t.GetRotation(), mulPerElem(Vector3(t.GetScale(), t.GetScale(), 1.0f), v)));
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
        res.SetTranslation(Vector3(Apply(lhs, Point3(rhs.GetTranslation()))));
        res.SetScale(lhs.GetScale() * rhs.GetScale());
        return res;
    }

    /**
     * Transforms the right-hand transform by the left-hand transform, without scaling the Z-component of the transition of the transformed transform
     * @param lhs Transforming transform
     * @param rhs Transformed transform
     * @return Transformed transform
     */
    inline TransformS1 MulNoScaleZ(const TransformS1& lhs, const TransformS1& rhs)
    {
        TransformS1 res;
        res.SetRotation(lhs.GetRotation() * rhs.GetRotation());
        res.SetTranslation(Vector3(ApplyNoScaleZ(lhs, Point3(rhs.GetTranslation()))));
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
    inline Matrix4 ToMatrix4(const TransformS1& t)
    {
        Matrix4 res(t.GetRotation(), t.GetTranslation());
        res = appendScale(res, Vector3(t.GetScale()));
        return res;
    }

    /**
     * Transform with non-uniform (3-component) scale.
     * Transform applied as:
     * T(p) = translate(rotate(scale(p))) = p'
     * The scale is non-rotated to avoid shearing in the transform.
     * Two transforms are applied as:
     * T1(T2(p)) = t1(r1(t2(r2(s1(s2(p)))))) = p'
     * This means that the transform is not associative:
     * T1(T2(p)) != (T1*T2)(P)
     */
    class Transform
    {
        Quat    m_Rotation;
        Vector3 m_Translation;
        Vector3 m_Scale;
    public:

        Transform() {}

        Transform(Vector3 translation, Quat rotation, Vector3 scale)
        : m_Rotation(rotation)
        , m_Translation(translation)
        , m_Scale(scale)
        {

        }

        Transform(Vector3 translation, Quat rotation, float uniformScale)
        : m_Rotation(rotation)
        , m_Translation(translation)
        , m_Scale(uniformScale)
        {

        }

        inline void SetIdentity()
        {
            m_Rotation = Quat::identity();
            m_Translation = Vector3(0.0f, 0.0f, 0.0f);
            m_Scale = Vector3(1.0f, 1.0f, 1.0f);
        }

        inline Vector3 GetTranslation() const
        {
            return m_Translation;
        }

        inline void SetTranslation(Vector3 translation)
        {
            m_Translation = translation;
        }

        inline float* GetPositionPtr() const
        {
            return (float*)&m_Translation;
        }

        inline Vector3 GetScale() const
        {
            return m_Scale;
        }

        inline void SetScale(Vector3 scale)
        {
            m_Scale = scale;
        }

        inline float* GetScalePtr() const
        {
            return (float*)&m_Scale;
        }

        /**
         * Compute a 'uniform' scale for this transform. In the event that the
         * scale applied to this transform is not uniform then the value is arbitrary:
         * we make a selection that will not introduce any floating point rounding errors.
         * @return the uniform scale associated with this transform.
         */
        inline float GetUniformScale() const
        {
            return minElem(m_Scale);
        }

        inline void SetUniformScale(float scale)
        {
            m_Scale = Vector3(scale);
        }

        inline Quat GetRotation() const
        {
            return m_Rotation;
        }

        inline void SetRotation(Quat rotation)
        {
            m_Rotation = rotation;
        }

        inline float* GetRotationPtr() const
        {
            return (float*)&m_Rotation;
        }
    };

    /**
     * Apply the transform on a point (includes the transform translation).
     * @param t Transform
     * @param p Point
     * @return Transformed point
     */
    inline Point3 Apply(const Transform& t, const Point3 p)
    {
        return Point3(rotate(t.GetRotation(), mulPerElem(Vector3(p), t.GetScale())) + t.GetTranslation());
    }

    /**
     * Apply the transform on a point, but without scaling the Z-component of the point (includes the transform translation).
     * @param t Transform
     * @param p Point
     * @return Transformed point
     */
    inline Point3 ApplyNoScaleZ(const Transform& t, const Point3 p)
    {
        Vector3 s = t.GetScale();
        s.setZ(1.0f);
        return Point3(rotate(t.GetRotation(), mulPerElem(Vector3(p), s)) + t.GetTranslation());
    }

    /**
     * Apply the transform on a vector (excludes the transform translation).
     * @param t Transform
     * @param v Vector
     * @return Transformed vector
     */
    inline Vector3 Apply(const Transform& t, const Vector3 v)
    {
        return Vector3(rotate(t.GetRotation(), mulPerElem(v, t.GetScale())));
    }

    /**
     * Apply the transform on a vector, but without scaling the Z-component of the vector (excludes the transform translation).
     * @param t Transform
     * @param v Vector
     * @return Transformed vector
     */
    inline Vector3 ApplyNoScaleZ(const Transform& t, const Vector3 v)
    {
        Vector3 s = t.GetScale();
        s.setZ(1.0f);
        return Vector3(rotate(t.GetRotation(), mulPerElem(v, s)));
    }

    /**
     * Transforms the right-hand transform by the left-hand transform
     * @param lhs Transforming transform
     * @param rhs Transformed transform
     * @return Transformed transform
     */
    inline Transform Mul(const Transform& lhs, const Transform& rhs)
    {
        Transform res;
        res.SetRotation(lhs.GetRotation() * rhs.GetRotation());
        res.SetTranslation(Vector3(Apply(lhs, Point3(rhs.GetTranslation()))));
        res.SetScale(mulPerElem(lhs.GetScale(), rhs.GetScale()));
        return res;
    }

    /**
     * Transforms the right-hand transform by the left-hand transform, without scaling the Z-component of the transition of the transformed transform
     * @param lhs Transforming transform
     * @param rhs Transformed transform
     * @return Transformed transform
     */
    inline Transform MulNoScaleZ(const Transform& lhs, const Transform& rhs)
    {
        Transform res;
        res.SetRotation(lhs.GetRotation() * rhs.GetRotation());
        res.SetTranslation(Vector3(ApplyNoScaleZ(lhs, Point3(rhs.GetTranslation()))));
        res.SetScale(mulPerElem(lhs.GetScale(), rhs.GetScale()));
        return res;
    }

    /**
     * Invert a transform
     * @param t Transform to invert
     * @return Inverted transform
     */
    inline Transform Inv(const Transform& t)
    {
        const Vector3& s = t.GetScale();
        (void)s;
        assert(s.getX() != 0.0f && s.getY() != 0.0f && s.getZ() != 0.0f && "Transform can not be inverted (0 scale-component).");
        Transform res;
        res.SetRotation(conj(t.GetRotation()));
        res.SetScale(recipPerElem(t.GetScale()));
        res.SetTranslation(mulPerElem(rotate(res.GetRotation(), -t.GetTranslation()), res.GetScale()));
        return res;
    }

    /**
     * Convert a transform into a 4-dim matrix
     * @param t Transform to convert
     * @return Matrix representing the same transform
     */
    inline Matrix4 ToMatrix4(const Transform& t)
    {
        Matrix4 res(t.GetRotation(), t.GetTranslation());
        res = appendScale(res, t.GetScale());
        return res;
    }

    /**
     * Extract the absolute values of the scale component from a matrix.
     * @param mtx Source matrix
     * @return Vector3 with scale values for x,y,z
     */
    inline Vector3 ExtractScale(const Matrix4& mtx)
    {
        Vector4 col0(mtx.getCol(0));
        Vector4 col1(mtx.getCol(1));
        Vector4 col2(mtx.getCol(2));
        return Vector3(length(col0), length(col1), length(col2));
    }

    /**
     * Eliminate the scaling components in a matrix
     * @param mtx Matrix to operate on
     * @return Vector containing the scaling by component
     */
    inline Vector3 ResetScale(Matrix4 *mtx)
    {
        Vector3 scale = ExtractScale(*mtx);
        if (scale.getX() == 0 || scale.getY() == 0 || scale.getZ() == 0)
            return Vector3(1, 1, 1);

        mtx->setCol(0, mtx->getCol(0) * (1.0f / scale.getX()));
        mtx->setCol(1, mtx->getCol(1) * (1.0f / scale.getY()));
        mtx->setCol(2, mtx->getCol(2) * (1.0f / scale.getZ()));
        return scale;
    }

    /**
     * Convert a matrix into a transform
     * @param mtx Matrix4 to convert
     * @return Transform representing the same transform
     */
    inline Transform ToTransform(const Matrix4& mtx)
    {
        Matrix4 tmp(mtx);
        Vector4 col3(mtx.getCol(3));
        Vector3 scale = ResetScale(&tmp);
        return dmTransform::Transform(Vector3(col3.getX(), col3.getY(), col3.getZ()), Quat(tmp.getUpper3x3()), scale);
    }

    /**
     * Eliminate the z scaling components in a matrix
     * @param mtx Matrix to operate on
     */
    inline void NormalizeZScale(Matrix4 *mtx)
    {
        Vector4 col3(mtx->getCol(2));
        float zMagSqr = lengthSqr(col3);
        if (zMagSqr > 0.0f)
        {
             mtx->setCol(2, col3 * (1.0f / sqrtf(zMagSqr)));
        }
    }

    /**
     * Eliminate the z scaling components in a matrix
     * @param source Source matrix
     * @param target Target matrix
     */
    inline void NormalizeZScale(Matrix4 const &source, Matrix4 *target)
    {
        *target = source;
        NormalizeZScale(target);
    }

    /**
     * Multiply two matrices without z-scaling the translation in m2
     * @param m1 First matrix
     * @param m2 Second matrix
     * @return The resulting transform
     */
    inline Matrix4 MulNoScaleZ(Matrix4 const &m1, Matrix4 const &m2)
    {
        // tmp is the non-z scaling version of m1
        Matrix4 tmp, res;
        NormalizeZScale(m1, &tmp);
        res = m1 * m2;
        res.setCol(3, tmp * m2.getCol(3));
        return res;
    }
}

#endif // DM_TRANSFORM_H
