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

#include <math.h>
#include <stdlib.h>
#include <string.h>

#include <dmsdk/dlib/array.h>

#include "font.h"
#include "font_outline.h"

// Font backends decode their native glyph representation into this shared path
// format. Curves are then split at their y extrema. Keeping that normalization
// in the outline makes it reusable by rasterizers and scanline algorithms, and
// ensures each curve intersects a horizontal scanline at most once.

// Do not split at roots extremely close to an endpoint. Such a split creates a
// nearly zero-length curve without changing its useful monotonic intervals.
static const float MONOTONIC_ROOT_EPSILON = 1.0e-6f;

static FontOutlinePoint LerpPoint(FontOutlinePoint a, FontOutlinePoint b, float t)
{
    // Linear interpolation is the primitive used by de Casteljau subdivision.
    FontOutlinePoint result = { a.m_X + (b.m_X - a.m_X) * t,
                                a.m_Y + (b.m_Y - a.m_Y) * t };
    return result;
}

static void PushCommand(dmArray<FontOutlineCommand>& commands, FontOutlineCommandType type,
                        FontOutlinePoint p0 = {}, FontOutlinePoint p1 = {}, FontOutlinePoint p2 = {})
{
    // FontOutlineCommand always has room for the largest operation (a cubic),
    // so unused points remain zero-initialized for moves, lines, and quadratics.
    if (commands.Full())
        commands.OffsetCapacity(32);
    FontOutlineCommand command = {};
    command.m_Type = type;
    command.m_Points[0] = p0;
    command.m_Points[1] = p1;
    command.m_Points[2] = p2;
    commands.Push(command);
}

static void PushYMonotonicQuadratic(dmArray<FontOutlineCommand>& commands, FontOutlinePoint from,
                                    FontOutlinePoint control, FontOutlinePoint to)
{
    // A quadratic has one possible y extremum where dy/dt == 0. Split there
    // when the root lies strictly inside the curve; otherwise it is already
    // y-monotonic.
    float denominator = from.m_Y - 2.0f * control.m_Y + to.m_Y;
    float t = denominator == 0.0f ? -1.0f : (from.m_Y - control.m_Y) / denominator;
    if (t <= MONOTONIC_ROOT_EPSILON || t >= 1.0f - MONOTONIC_ROOT_EPSILON)
    {
        PushCommand(commands, FONT_OUTLINE_QUADRATIC_TO, control, to);
        return;
    }

    FontOutlinePoint a = LerpPoint(from, control, t);
    FontOutlinePoint b = LerpPoint(control, to, t);
    FontOutlinePoint middle = LerpPoint(a, b, t);
    PushCommand(commands, FONT_OUTLINE_QUADRATIC_TO, a, middle);
    PushCommand(commands, FONT_OUTLINE_QUADRATIC_TO, b, to);
}

static void SplitCubic(FontOutlinePoint p0, FontOutlinePoint p1, FontOutlinePoint p2,
                       FontOutlinePoint p3, float t, FontOutlinePoint left[4], FontOutlinePoint right[4])
{
    // de Casteljau subdivision produces two cubic Beziers whose concatenation
    // is exactly the original curve.
    FontOutlinePoint a = LerpPoint(p0, p1, t);
    FontOutlinePoint b = LerpPoint(p1, p2, t);
    FontOutlinePoint c = LerpPoint(p2, p3, t);
    FontOutlinePoint d = LerpPoint(a, b, t);
    FontOutlinePoint e = LerpPoint(b, c, t);
    FontOutlinePoint middle = LerpPoint(d, e, t);
    left[0] = p0;
    left[1] = a;
    left[2] = d;
    left[3] = middle;
    right[0] = middle;
    right[1] = e;
    right[2] = c;
    right[3] = p3;
}

static void PushYMonotonicCubic(dmArray<FontOutlineCommand>& commands, FontOutlinePoint p0,
                                FontOutlinePoint p1, FontOutlinePoint p2, FontOutlinePoint p3)
{
    // The derivative of a cubic Bezier is quadratic. Its zero, one, or two
    // roots are the only positions where the direction along y can change.
    float a = 3.0f * (-p0.m_Y + 3.0f * p1.m_Y - 3.0f * p2.m_Y + p3.m_Y);
    float b = 6.0f * (p0.m_Y - 2.0f * p1.m_Y + p2.m_Y);
    float c = 3.0f * (p1.m_Y - p0.m_Y);
    float roots[2];
    uint32_t root_count = 0;
    if (fabsf(a) < 1.0e-7f)
    {
        if (fabsf(b) >= 1.0e-7f)
            roots[root_count++] = -c / b;
    }
    else
    {
        float discriminant = b * b - 4.0f * a * c;
        if (discriminant >= 0.0f)
        {
            float root = sqrtf(discriminant);
            roots[root_count++] = (-b - root) / (2.0f * a);
            if (root != 0.0f)
                roots[root_count++] = (-b + root) / (2.0f * a);
        }
    }
    if (root_count == 2 && roots[0] > roots[1])
    {
        float temporary = roots[0];
        roots[0] = roots[1];
        roots[1] = temporary;
    }

    FontOutlinePoint remaining[4] = { p0, p1, p2, p3 };
    float previous = 0.0f;
    for (uint32_t i = 0; i < root_count; ++i)
    {
        float root = roots[i];
        if (root <= previous + MONOTONIC_ROOT_EPSILON || root >= 1.0f - MONOTONIC_ROOT_EPSILON)
            continue;

        FontOutlinePoint left[4];
        FontOutlinePoint right[4];
        // 'remaining' starts at the previous root, so remap the root from the
        // original curve's parameter range to the remaining curve's range.
        SplitCubic(remaining[0], remaining[1], remaining[2], remaining[3],
                   (root - previous) / (1.0f - previous), left, right);
        PushCommand(commands, FONT_OUTLINE_CUBIC_TO, left[1], left[2], left[3]);
        memcpy(remaining, right, sizeof(remaining));
        previous = root;
    }
    PushCommand(commands, FONT_OUTLINE_CUBIC_TO, remaining[1], remaining[2], remaining[3]);
}

FontResult FontOutlineMakeYMonotonic(FontOutline* outline)
{
    // Build a replacement command stream rather than modifying the source in
    // place: splitting adds commands and later commands still depend on the
    // original current point.
    dmArray<FontOutlineCommand> commands;
    FontOutlinePoint current = {};
    for (uint32_t i = 0; i < outline->m_CommandCount; ++i)
    {
        const FontOutlineCommand& command = outline->m_Commands[i];
        switch (command.m_Type)
        {
        case FONT_OUTLINE_MOVE_TO:
        case FONT_OUTLINE_LINE_TO:
            PushCommand(commands, command.m_Type, command.m_Points[0]);
            current = command.m_Points[0];
            break;
        case FONT_OUTLINE_QUADRATIC_TO:
            PushYMonotonicQuadratic(commands, current, command.m_Points[0], command.m_Points[1]);
            current = command.m_Points[1];
            break;
        case FONT_OUTLINE_CUBIC_TO:
            PushYMonotonicCubic(commands, current, command.m_Points[0], command.m_Points[1], command.m_Points[2]);
            current = command.m_Points[2];
            break;
        case FONT_OUTLINE_CLOSE:
            PushCommand(commands, FONT_OUTLINE_CLOSE);
            break;
        }
    }

    FontOutlineCommand* normalized = 0;
    if (!commands.Empty())
    {
        // dmArray owns temporary storage only. Copy into the malloc-owned form
        // required by FontOutline before replacing the original allocation.
        normalized = (FontOutlineCommand*)malloc(sizeof(FontOutlineCommand) * commands.Size());
        if (!normalized)
            return FONT_RESULT_ERROR;

        memcpy(normalized, commands.Begin(), sizeof(FontOutlineCommand) * commands.Size());
    }

    // The original outline remains untouched if allocation fails above.
    free(outline->m_Commands);
    outline->m_Commands = normalized;
    outline->m_CommandCount = commands.Size();
    return FONT_RESULT_OK;
}

void FontFreeGlyphOutline(FontOutline* outline)
{
    free(outline->m_Commands);
    memset(outline, 0, sizeof(*outline));
}

static void ExtendBounds(FontOutlinePoint point, float* x0, float* y0, float* x1, float* y1)
{
    // Extend an initialized axis-aligned bounding box by one point.
    if (point.m_X < *x0)
        *x0 = point.m_X;
    if (point.m_Y < *y0)
        *y0 = point.m_Y;
    if (point.m_X > *x1)
        *x1 = point.m_X;
    if (point.m_Y > *y1)
        *y1 = point.m_Y;
}

static void ExtendBounds(FontOutlineBounds* bounds, FontOutlinePoint point)
{
    if (!bounds->m_HasBounds)
    {
        bounds->m_X0 = bounds->m_X1 = point.m_X;
        bounds->m_Y0 = bounds->m_Y1 = point.m_Y;
        bounds->m_HasBounds = true;
        return;
    }

    ExtendBounds(point, &bounds->m_X0, &bounds->m_Y0, &bounds->m_X1, &bounds->m_Y1);
}

static float EvaluateQuadratic(float p0, float p1, float p2, float t)
{
    float u = 1.0f - t;
    return u * u * p0 + 2.0f * u * t * p1 + t * t * p2;
}

static float EvaluateCubic(float p0, float p1, float p2, float p3, float t)
{
    float u = 1.0f - t;
    return u * u * u * p0 + 3.0f * u * u * t * p1 + 3.0f * u * t * t * p2 + t * t * t * p3;
}

static void ExtendQuadraticBounds(FontOutlinePoint from, FontOutlinePoint control, FontOutlinePoint to, float* x0, float* y0, float* x1, float* y1)
{
    // Endpoints are handled by FontGetOutlineBounds. Add interior extrema by
    // solving dx/dt == 0 and dy/dt == 0 independently.
    float denominator = from.m_X - 2.0f * control.m_X + to.m_X;
    if (denominator != 0.0f)
    {
        float t = (from.m_X - control.m_X) / denominator;
        if (t > 0.0f && t < 1.0f)
            ExtendBounds({ EvaluateQuadratic(from.m_X, control.m_X, to.m_X, t), EvaluateQuadratic(from.m_Y, control.m_Y, to.m_Y, t) }, x0, y0, x1, y1);
    }
    denominator = from.m_Y - 2.0f * control.m_Y + to.m_Y;
    if (denominator != 0.0f)
    {
        float t = (from.m_Y - control.m_Y) / denominator;
        if (t > 0.0f && t < 1.0f)
            ExtendBounds({ EvaluateQuadratic(from.m_X, control.m_X, to.m_X, t), EvaluateQuadratic(from.m_Y, control.m_Y, to.m_Y, t) }, x0, y0, x1, y1);
    }
}

static void ExtendCubicAxisBounds(float p0, float p1, float p2, float p3, FontOutlinePoint from, FontOutlinePoint control_1, FontOutlinePoint control_2, FontOutlinePoint to, float* x0, float* y0, float* x1, float* y1)
{
    // Solve the quadratic derivative for one axis. Evaluate both coordinates
    // at each interior root so the shared point-based bounds helper can be used.
    float    a = -p0 + 3.0f * p1 - 3.0f * p2 + p3;
    float    b = 2.0f * (p0 - 2.0f * p1 + p2);
    float    c = p1 - p0;
    float    roots[2];
    uint32_t root_count = 0;
    if (fabsf(a) < 1.0e-8f)
    {
        if (b != 0.0f)
            roots[root_count++] = -c / b;
    }
    else
    {
        float discriminant = b * b - 4.0f * a * c;
        if (discriminant >= 0.0f)
        {
            float root = sqrtf(discriminant);
            roots[root_count++] = (-b + root) / (2.0f * a);
            roots[root_count++] = (-b - root) / (2.0f * a);
        }
    }
    for (uint32_t i = 0; i < root_count; ++i)
    {
        float t = roots[i];
        if (t > 0.0f && t < 1.0f)
        {
            FontOutlinePoint point = {
                EvaluateCubic(from.m_X, control_1.m_X, control_2.m_X, to.m_X, t),
                EvaluateCubic(from.m_Y, control_1.m_Y, control_2.m_Y, to.m_Y, t)
            };
            ExtendBounds(point, x0, y0, x1, y1);
        }
    }
}

void FontOutlineBoundsMoveTo(FontOutlineBounds* bounds, FontOutlinePoint to)
{
    ExtendBounds(bounds, to);
    bounds->m_Current = to;
}

void FontOutlineBoundsLineTo(FontOutlineBounds* bounds, FontOutlinePoint to)
{
    ExtendBounds(bounds, to);
    bounds->m_Current = to;
}

void FontOutlineBoundsQuadraticTo(FontOutlineBounds* bounds, FontOutlinePoint control, FontOutlinePoint to)
{
    ExtendBounds(bounds, to);
    ExtendQuadraticBounds(bounds->m_Current, control, to, &bounds->m_X0, &bounds->m_Y0, &bounds->m_X1, &bounds->m_Y1);
    bounds->m_Current = to;
}

void FontOutlineBoundsCubicTo(FontOutlineBounds* bounds, FontOutlinePoint control_1, FontOutlinePoint control_2, FontOutlinePoint to)
{
    ExtendBounds(bounds, to);
    ExtendCubicAxisBounds(bounds->m_Current.m_X, control_1.m_X, control_2.m_X, to.m_X,
                          bounds->m_Current, control_1, control_2, to,
                          &bounds->m_X0, &bounds->m_Y0, &bounds->m_X1, &bounds->m_Y1);
    ExtendCubicAxisBounds(bounds->m_Current.m_Y, control_1.m_Y, control_2.m_Y, to.m_Y,
                          bounds->m_Current, control_1, control_2, to,
                          &bounds->m_X0, &bounds->m_Y0, &bounds->m_X1, &bounds->m_Y1);
    bounds->m_Current = to;
}

void FontOutlineBoundsMerge(FontOutlineBounds* bounds, const FontOutlineBounds* source, float dx, float dy)
{
    if (!source->m_HasBounds)
        return;

    ExtendBounds(bounds, { source->m_X0 + dx, source->m_Y0 + dy });
    ExtendBounds(bounds, { source->m_X1 + dx, source->m_Y1 + dy });
}

bool FontOutlineBoundsGet(const FontOutlineBounds* bounds, float* x0, float* y0, float* x1, float* y1)
{
    if (!bounds->m_HasBounds)
        return false;

    *x0 = bounds->m_X0;
    *y0 = bounds->m_Y0;
    *x1 = bounds->m_X1;
    *y1 = bounds->m_Y1;
    return true;
}

bool FontGetOutlineBounds(const FontOutline* outline, float* x0, float* y0, float* x1, float* y1)
{
    // Include every segment endpoint, then extend by the exact interior extrema
    // of curved segments. Control points are not necessarily on the curve and
    // therefore must not be used directly as exact bounds.
    FontOutlineBounds bounds = {};
    for (uint32_t i = 0; i < outline->m_CommandCount; ++i)
    {
        const FontOutlineCommand* command = &outline->m_Commands[i];
        switch (command->m_Type)
        {
            case FONT_OUTLINE_MOVE_TO:
                FontOutlineBoundsMoveTo(&bounds, command->m_Points[0]);
                break;
            case FONT_OUTLINE_LINE_TO:
                FontOutlineBoundsLineTo(&bounds, command->m_Points[0]);
                break;
            case FONT_OUTLINE_QUADRATIC_TO:
                FontOutlineBoundsQuadraticTo(&bounds, command->m_Points[0], command->m_Points[1]);
                break;
            case FONT_OUTLINE_CUBIC_TO:
                FontOutlineBoundsCubicTo(&bounds, command->m_Points[0], command->m_Points[1], command->m_Points[2]);
                break;
            case FONT_OUTLINE_CLOSE:
                break;
        }
    }
    return FontOutlineBoundsGet(&bounds, x0, y0, x1, y1);
}
