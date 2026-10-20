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

// Copies and bakes the knots. Returns one owned reference, or null for invalid
// or zero-length input. Closed curves connect the last knot back to the first.
// Initial up provides direction only, is projected perpendicular to the first
// tangent, and is transported along the curve. Its length does not set scale.
HFontTextCurve FontTextCurveCreate(const FontTextCurveKnot* knots, uint32_t count, bool closed, const dmVMath::Vector3& initial_up);
// Retain/release an owned reference. Both accept null.
void FontTextCurveRetain(HFontTextCurve curve);
void FontTextCurveRelease(HFontTextCurve curve);
// Distance in local units along the baked curve.
float FontTextCurveGetLength(HFontTextCurve curve);
// Bounds of the baseline and maximum authored scale, for conservative culling.
void FontTextCurveGetBounds(HFontTextCurve curve, dmVMath::Vector3* minimum, dmVMath::Vector3* maximum, float* max_scale);
// Map a glyph cluster at the given baseline distance. Every line uses this
// same frame; its layout y coordinate provides the offset from the first line.
// Open curves extrapolate their endpoint tangents; closed curves repeat.
dmVMath::Matrix4 FontTextCurveGetTransform(HFontTextCurve curve, float distance, float first_baseline);

#endif // DM_FONT_TEXT_CURVE_HPP
