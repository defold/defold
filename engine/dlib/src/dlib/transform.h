#ifndef DM_TRANSFORM_H
#define DM_TRANSFORM_H

#include <assert.h>
#include <math.h>
#include <vectormath/cpp/vectormath_aos.h>

namespace dmTransform
{
    using namespace Vectormath::Aos;

    /**
     * Transform with uniform scale.
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

    inline TransformS1 Mul(const TransformS1& lhs, const TransformS1& rhs)
    {
        TransformS1 res;
        res.SetRotation(lhs.GetRotation() * rhs.GetRotation());
        res.SetTranslation(rotate(lhs.GetRotation(), lhs.GetScale() * rhs.GetTranslation()) + lhs.GetTranslation());
        res.SetScale(lhs.GetScale() * rhs.GetScale());
        return res;
    }

    inline TransformS1 Inv(const TransformS1& t)
    {
        assert(t.GetScale() != 0.0f && "Transform can not be inverted (0 scale).");
        TransformS1 res;
        res.SetRotation(conj(t.GetRotation()));
        res.SetScale(1.0f / t.GetScale());
        res.SetTranslation(rotate(res.GetRotation(), -t.GetTranslation()) * res.GetScale());
        return res;
    }
}

#endif // DM_TRANSFORM_H
