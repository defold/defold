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

#ifndef DM_FONT_TEXT_CURVE_HPP
#define DM_FONT_TEXT_CURVE_HPP

#include <dlib/vmath.h>
#include <stdint.h>

// Immutable, reference-counted cubic Bezier chain in local text space.
typedef struct FontTextCurve* HFontTextCurve;

// The sampled chain and its distance-index sentinel use 32-bit array indices.
static const uint32_t FONT_TEXT_CURVE_SEGMENT_STEPS = 64;
static const uint32_t FONT_TEXT_CURVE_MAX_KNOT_COUNT = UINT32_MAX / FONT_TEXT_CURVE_SEGMENT_STEPS - 1;

// Handles are offsets from the knot. Roll is measured around the transported
// tangent; scale is a finite, positive uniform glyph/baseline scale.
struct FontTextCurveKnot
{
    dmVMath::Vector3 m_Position;
    dmVMath::Vector3 m_InHandle;
    dmVMath::Vector3 m_OutHandle;
    float            m_RollDegrees;
    float            m_Scale;
};

/**
 * Copy and bake a cubic Bezier chain using 64 uniform steps per segment.
 * Distances and frames are approximations; there is no fixed geometric error bound.
 * Initial up is projected perpendicular to the tangent and transported along the
 * curve. Its length does not set scale.
 * @param knots Ordered knots; not retained
 * @param count Knot count, from 2 through FONT_TEXT_CURVE_MAX_KNOT_COUNT
 * @param closed Connect the last knot back to the first
 * @param initial_up Finite, nonzero initial up direction
 * @return One owned reference, or null for invalid or zero-length input
 */
HFontTextCurve FontTextCurveCreate(const FontTextCurveKnot* knots, uint32_t count, bool closed, const dmVMath::Vector3& initial_up);

/** Increment the reference count. A null handle is accepted. */
void FontTextCurveRetain(HFontTextCurve curve);
/** Release one reference. A null handle is accepted. */
void FontTextCurveRelease(HFontTextCurve curve);

/** Return the sampled baseline length in local units. */
float FontTextCurveGetLength(HFontTextCurve curve);

/**
 * Query the sampled baseline bounds and a conservative scale bound for culling.
 * @param curve Baked curve, borrowed for this call
 * @param minimum Output minimum coordinates
 * @param maximum Output maximum coordinates
 * @param max_scale Output upper bound on authored uniform scales
 */
void FontTextCurveGetBounds(HFontTextCurve curve, dmVMath::Vector3* minimum, dmVMath::Vector3* maximum, float* max_scale);

/**
 * Map a glyph cluster onto the curve. Lines share the frame and keep their layout
 * y offset from the first baseline. Open endpoints extrapolate; closed curves repeat.
 * @param curve Baked curve, borrowed for this call
 * @param distance Cluster position along the baseline, in local units
 * @param first_baseline Layout y coordinate of the first baseline
 * @return Local-space cluster transform, including the authored uniform scale
 */
dmVMath::Matrix4 FontTextCurveGetTransform(HFontTextCurve curve, float distance, float first_baseline);

#endif // DM_FONT_TEXT_CURVE_HPP
