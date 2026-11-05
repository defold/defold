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

#include "text_curve.hpp"
#include <dlib/array.h>
#include <dlib/math.h>
#include <float.h>
#include <math.h>

using namespace dmVMath;

struct CurveSample
{
    Vector3 m_Position;
    Vector3 m_Tangent;
    Vector3 m_Up;
    float   m_Distance;
    float   m_Roll;
    float   m_Scale;
};

struct FontTextCurve
{
    Vector3              m_Minimum;
    Vector3              m_Maximum;
    dmArray<CurveSample> m_Samples;
    dmArray<uint32_t>    m_DistanceIndex;
    double               m_DistanceToIndex;
    float                m_MaxScale;
    uint32_t             m_References;
    bool                 m_Closed;
};

static bool IsFinite(const Vector3& v)
{
    return isfinite(v.getX()) && isfinite(v.getY()) && isfinite(v.getZ());
}

static Vector3 NormalizeDirection(const Vector3& direction, const Vector3& fallback)
{
    const float length_sq = LengthSqr(direction);
    if (length_sq > 1.0e-12f)
    {
        return direction / sqrtf(length_sq);
    }
    return fallback;
}

static Vector3 PerpendicularUp(const Vector3& up, const Vector3& tangent)
{
    const Vector3 fallback = fabsf(tangent.getY()) < 0.9f ? Vector3(0, 1, 0) : Vector3(0, 0, 1);
    const Vector3 projected_up = up - tangent * Dot(up, tangent);
    const Vector3 projected_fallback = fallback - tangent * Dot(fallback, tangent);
    const Vector3 fallback_up = NormalizeDirection(projected_fallback, Vector3(1, 0, 0));
    return NormalizeDirection(projected_up, fallback_up);
}

// Rodrigues' rotation formula for a unit axis.
static Vector3 RotateAroundAxis(const Vector3& vector, const Vector3& axis, float angle)
{
    const float cosine = cosf(angle);
    const float sine = sinf(angle);
    return vector * cosine + Cross(axis, vector) * sine + axis * Dot(axis, vector) * (1.0f - cosine);
}

static Vector3 TransportUp(const Vector3& up, const Vector3& from, const Vector3& to)
{
    const Vector3 axis = Cross(from, to);
    const float   sine = sqrtf(LengthSqr(axis));
    const float   cosine = dmMath::Clamp(Dot(from, to), -1.0f, 1.0f);

    Vector3 transported_up = up;
    if (sine > 1.0e-6f)
    {
        transported_up = RotateAroundAxis(up, axis / sine, atan2f(sine, cosine));
    }
    return PerpendicularUp(transported_up, to);
}

// Sample positions and tangents at uniform parameter intervals on each segment.
static void SampleCurveSegments(FontTextCurve* curve, const FontTextCurveKnot* knots, uint32_t count)
{
    const uint32_t steps = FONT_TEXT_CURVE_SEGMENT_STEPS;
    const uint32_t segments = curve->m_Closed ? count : count - 1;
    curve->m_Samples.SetCapacity(segments * steps + 1);
    curve->m_Samples.SetSize(segments * steps + 1);

    for (uint32_t segment = 0; segment < segments; ++segment)
    {
        const FontTextCurveKnot& a = knots[segment];
        const FontTextCurveKnot& b = knots[(segment + 1) % count];
        const Vector3            p0 = a.m_Position;
        const Vector3            p1 = p0 + a.m_OutHandle;
        const Vector3            p3 = b.m_Position;
        const Vector3            p2 = p3 + b.m_InHandle;
        for (uint32_t j = segment == 0 ? 0 : 1; j <= steps; ++j)
        {
            const float  t = (float)j / steps;
            const float  u = 1.0f - t;
            CurveSample& sample = curve->m_Samples[segment * steps + j];
            sample.m_Position = p0 * (u * u * u) + p1 * (3 * u * u * t) + p2 * (3 * u * t * t) + p3 * (t * t * t);
            sample.m_Tangent = (p1 - p0) * (3 * u * u) + (p2 - p1) * (6 * u * t) + (p3 - p2) * (3 * t * t);
            for (uint32_t axis = 0; axis < 3; ++axis)
            {
                curve->m_Minimum.setElem(axis, dmMath::Min(curve->m_Minimum.getElem(axis), sample.m_Position.getElem(axis)));
                curve->m_Maximum.setElem(axis, dmMath::Max(curve->m_Maximum.getElem(axis), sample.m_Position.getElem(axis)));
            }
        }
    }
}

// Accumulate chord distances and transport up along the sampled tangents.
static void BuildCurveFrames(FontTextCurve* curve, const Vector3& initial_up)
{
    const uint32_t sample_count = curve->m_Samples.Size();
    for (uint32_t i = 0; i < sample_count; ++i)
    {
        const uint32_t     previous_index = i == 0 ? 0 : i - 1;
        const uint32_t     next_index = dmMath::Min(i + 1, sample_count - 1);
        const CurveSample& previous = curve->m_Samples[previous_index];
        CurveSample&       sample = curve->m_Samples[i];

        const Vector3 chord = curve->m_Samples[next_index].m_Position - previous.m_Position;
        const Vector3 fallback = i == 0 ? Vector3(1, 0, 0) : previous.m_Tangent;
        const Vector3 tangent = NormalizeDirection(chord, fallback);
        sample.m_Tangent = NormalizeDirection(sample.m_Tangent, tangent);

        if (i == 0)
        {
            sample.m_Distance = 0.0f;
            sample.m_Up = PerpendicularUp(initial_up, sample.m_Tangent);
        }
        else
        {
            sample.m_Distance = previous.m_Distance + Length(sample.m_Position - previous.m_Position);
            sample.m_Up = TransportUp(previous.m_Up, previous.m_Tangent, sample.m_Tangent);
        }
    }
}

// Spread the closing-frame correction over the loop to avoid a twist at its seam.
static void CloseCurveFrame(FontTextCurve* curve)
{
    const CurveSample& first = curve->m_Samples[0];
    const CurveSample& last = curve->m_Samples.Back();
    const Vector3      up = TransportUp(last.m_Up, last.m_Tangent, first.m_Tangent);
    const float        correction = atan2f(Dot(Cross(up, first.m_Up), first.m_Tangent), Dot(up, first.m_Up));
    const float        length = last.m_Distance;

    for (uint32_t i = 1; i < curve->m_Samples.Size(); ++i)
    {
        CurveSample& sample = curve->m_Samples[i];
        const float  angle = correction * sample.m_Distance / length;
        sample.m_Up = RotateAroundAxis(sample.m_Up, sample.m_Tangent, angle);
    }
}

static void InterpolateKnotProperties(FontTextCurve* curve, const FontTextCurveKnot* knots, uint32_t count)
{
    const uint32_t steps = FONT_TEXT_CURVE_SEGMENT_STEPS;
    const uint32_t segments = curve->m_Closed ? count : count - 1;
    // Roll and scale progress with distance within each segment.
    for (uint32_t segment = 0; segment < segments; ++segment)
    {
        const FontTextCurveKnot& a = knots[segment];
        const FontTextCurveKnot& b = knots[(segment + 1) % count];
        const CurveSample&       first = curve->m_Samples[segment * steps];
        const CurveSample&       last = curve->m_Samples[(segment + 1) * steps];
        const float              segment_length = last.m_Distance - first.m_Distance;
        curve->m_MaxScale = dmMath::Max(curve->m_MaxScale, dmMath::Max(a.m_Scale, b.m_Scale));

        for (uint32_t j = 0; j <= steps; ++j)
        {
            CurveSample& sample = curve->m_Samples[segment * steps + j];
            const float  t = segment_length > 0 ? (sample.m_Distance - first.m_Distance) / segment_length : (float)j / steps;
            sample.m_Roll = dmMath::DegToRad(a.m_RollDegrees + (b.m_RollDegrees - a.m_RollDegrees) * t);
            sample.m_Scale = a.m_Scale + (b.m_Scale - a.m_Scale) * t;
        }
    }
}

static void BuildDistanceIndex(FontTextCurve* curve)
{
    // Uniform distance buckets bracket the original samples without resampling.
    const uint32_t sample_count = curve->m_Samples.Size();
    curve->m_DistanceToIndex = (sample_count - 1) / (double)curve->m_Samples.Back().m_Distance;
    curve->m_DistanceIndex.SetCapacity(sample_count + 1);
    curve->m_DistanceIndex.SetSize(sample_count + 1);

    uint32_t bucket = 0;
    for (uint32_t i = 0; i < sample_count; ++i)
    {
        const uint32_t last_bucket = (uint32_t)(curve->m_Samples[i].m_Distance * curve->m_DistanceToIndex);
        for (; bucket <= last_bucket; ++bucket)
        {
            curve->m_DistanceIndex[bucket] = i;
        }
    }
    for (; bucket <= sample_count; ++bucket)
    {
        curve->m_DistanceIndex[bucket] = sample_count - 1;
    }
}

HFontTextCurve FontTextCurveCreate(const FontTextCurveKnot* knots, uint32_t count, bool closed, const Vector3& initial_up)
{
    if (!knots || count < 2 || count > FONT_TEXT_CURVE_MAX_KNOT_COUNT || !IsFinite(initial_up))
    {
        return 0;
    }
    const float up_component = Vectormath::Aos::maxElem(AbsPerElem(initial_up));
    if (up_component == 0.0f)
    {
        return 0;
    }
    const Vector3 up = initial_up / up_component;
    for (uint32_t i = 0; i < count; ++i)
    {
        if (!IsFinite(knots[i].m_Position) || !IsFinite(knots[i].m_InHandle) || !IsFinite(knots[i].m_OutHandle) ||
            !isfinite(knots[i].m_RollDegrees) || !isfinite(knots[i].m_Scale) || knots[i].m_Scale <= 0.0f)
        {
            return 0;
        }
    }

    FontTextCurve* curve = new FontTextCurve;
    curve->m_References = 1;
    curve->m_Closed = closed;
    curve->m_MaxScale = 1.0f;
    curve->m_Minimum = Vector3(FLT_MAX);
    curve->m_Maximum = Vector3(-FLT_MAX);

    SampleCurveSegments(curve, knots, count);
    BuildCurveFrames(curve, up);
    if (!isfinite(curve->m_Samples.Back().m_Distance) || curve->m_Samples.Back().m_Distance < 1.0e-6f)
    {
        delete curve;
        return 0;
    }
    if (closed)
    {
        CloseCurveFrame(curve);
    }

    InterpolateKnotProperties(curve, knots, count);
    BuildDistanceIndex(curve);
    return curve;
}

void FontTextCurveRetain(HFontTextCurve curve)
{
    if (curve)
    {
        ++curve->m_References;
    }
}

void FontTextCurveRelease(HFontTextCurve curve)
{
    if (curve && --curve->m_References == 0)
    {
        delete curve;
    }
}

float FontTextCurveGetLength(HFontTextCurve curve)
{
    return curve->m_Samples.Back().m_Distance;
}

void FontTextCurveGetBounds(HFontTextCurve curve, Vector3* minimum, Vector3* maximum, float* max_scale)
{
    *minimum = curve->m_Minimum;
    *maximum = curve->m_Maximum;
    *max_scale = curve->m_MaxScale;
}

static CurveSample SampleCurve(HFontTextCurve curve, float distance)
{
    const float length = FontTextCurveGetLength(curve);
    if (curve->m_Closed)
    {
        distance = fmodf(distance, length);
        if (distance < 0)
        {
            distance += length;
        }
    }
    // Endpoints and nonfinite distances must not enter the distance-index lookup.
    if (!(distance > 0 && distance < length))
    {
        const uint32_t sample_index = distance <= 0 ? 0 : curve->m_Samples.Size() - 1;
        CurveSample    sample = curve->m_Samples[sample_index];
        sample.m_Position += sample.m_Tangent * (distance - sample.m_Distance);
        sample.m_Up = RotateAroundAxis(sample.m_Up, sample.m_Tangent, sample.m_Roll);
        return sample;
    }

    const uint32_t bucket = (uint32_t)(distance * curve->m_DistanceToIndex);
    uint32_t       low = curve->m_DistanceIndex[bucket];
    uint32_t       high = curve->m_DistanceIndex[bucket + 1];
    if (low > 0)
    {
        --low;
    }
    while (high - low > 1)
    {
        const uint32_t mid = (low + high) / 2;
        if (curve->m_Samples[mid].m_Distance < distance)
        {
            low = mid;
        }
        else
        {
            high = mid;
        }
    }

    const CurveSample& a = curve->m_Samples[low];
    const CurveSample& b = curve->m_Samples[high];
    const float        t = (distance - a.m_Distance) / (b.m_Distance - a.m_Distance);
    CurveSample        sample;
    sample.m_Position = Lerp(t, a.m_Position, b.m_Position);
    sample.m_Tangent = NormalizeDirection(Lerp(t, a.m_Tangent, b.m_Tangent), a.m_Tangent);
    sample.m_Up = PerpendicularUp(Lerp(t, a.m_Up, b.m_Up), sample.m_Tangent);
    sample.m_Up = RotateAroundAxis(sample.m_Up, sample.m_Tangent, a.m_Roll + (b.m_Roll - a.m_Roll) * t);
    sample.m_Scale = a.m_Scale + (b.m_Scale - a.m_Scale) * t;
    return sample;
}

Matrix4 FontTextCurveGetTransform(HFontTextCurve curve, float distance, float first_baseline)
{
    const CurveSample sample = SampleCurve(curve, distance);
    const Vector3     tangent = sample.m_Tangent * sample.m_Scale;
    const Vector3     up = sample.m_Up * sample.m_Scale;
    return Matrix4(
        Vector4(tangent, 0),
        Vector4(up, 0),
        Vector4(Cross(sample.m_Tangent, sample.m_Up) * sample.m_Scale, 0),
        Vector4(sample.m_Position - tangent * distance - up * first_baseline, 1));
}
