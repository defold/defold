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

#include "slug.hpp"
#include <math.h>
#include <string.h>

namespace dmSlug
{
    static void ExtendBounds(Point point, float* x0, float* y0, float* x1, float* y1)
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

    static void ExtendBounds(OutlineBounds* bounds, Point point)
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

    static void ExtendQuadraticBounds(Point from, Point control, Point to, float* x0, float* y0, float* x1, float* y1)
    {
        // Endpoints are handled by GetOutlineBounds. Add interior extrema by
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

    static void ExtendCubicAxisBounds(float p0, float p1, float p2, float p3, Point from, Point control_1, Point control_2, Point to, float* x0, float* y0, float* x1, float* y1)
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
                Point point = {
                    EvaluateCubic(from.m_X, control_1.m_X, control_2.m_X, to.m_X, t),
                    EvaluateCubic(from.m_Y, control_1.m_Y, control_2.m_Y, to.m_Y, t)
                };
                ExtendBounds(point, x0, y0, x1, y1);
            }
        }
    }

    void OutlineBoundsMoveTo(OutlineBounds* bounds, Point to)
    {
        ExtendBounds(bounds, to);
        bounds->m_Current = to;
    }

    void OutlineBoundsLineTo(OutlineBounds* bounds, Point to)
    {
        ExtendBounds(bounds, to);
        bounds->m_Current = to;
    }

    void OutlineBoundsQuadraticTo(OutlineBounds* bounds, Point control, Point to)
    {
        ExtendBounds(bounds, to);
        ExtendQuadraticBounds(bounds->m_Current, control, to, &bounds->m_X0, &bounds->m_Y0, &bounds->m_X1, &bounds->m_Y1);
        bounds->m_Current = to;
    }

    void OutlineBoundsCubicTo(OutlineBounds* bounds, Point control_1, Point control_2, Point to)
    {
        ExtendBounds(bounds, to);
        ExtendCubicAxisBounds(bounds->m_Current.m_X, control_1.m_X, control_2.m_X, to.m_X, bounds->m_Current, control_1, control_2, to, &bounds->m_X0, &bounds->m_Y0, &bounds->m_X1, &bounds->m_Y1);
        ExtendCubicAxisBounds(bounds->m_Current.m_Y, control_1.m_Y, control_2.m_Y, to.m_Y, bounds->m_Current, control_1, control_2, to, &bounds->m_X0, &bounds->m_Y0, &bounds->m_X1, &bounds->m_Y1);
        bounds->m_Current = to;
    }

    void OutlineBoundsMerge(OutlineBounds* bounds, const OutlineBounds* source, float dx, float dy)
    {
        if (!source->m_HasBounds)
            return;

        ExtendBounds(bounds, { source->m_X0 + dx, source->m_Y0 + dy });
        ExtendBounds(bounds, { source->m_X1 + dx, source->m_Y1 + dy });
    }

    bool OutlineBoundsGet(const OutlineBounds* bounds, float* x0, float* y0, float* x1, float* y1)
    {
        if (!bounds->m_HasBounds)
            return false;

        *x0 = bounds->m_X0;
        *y0 = bounds->m_Y0;
        *x1 = bounds->m_X1;
        *y1 = bounds->m_Y1;
        return true;
    }

    bool GetOutlineBounds(const Outline* outline, float* x0, float* y0, float* x1, float* y1)
    {
        // Include every segment endpoint, then extend by the exact interior extrema
        // of curved segments. Control points are not necessarily on the curve and
        // therefore must not be used directly as exact bounds.
        OutlineBounds bounds = {};
        for (uint32_t i = 0; i < outline->m_CommandCount; ++i)
        {
            const OutlineCommand* command = &outline->m_Commands[i];
            switch (command->m_Type)
            {
                case OUTLINE_MOVE_TO:
                    OutlineBoundsMoveTo(&bounds, command->m_Points[0]);
                    break;
                case OUTLINE_LINE_TO:
                    OutlineBoundsLineTo(&bounds, command->m_Points[0]);
                    break;
                case OUTLINE_QUADRATIC_TO:
                    OutlineBoundsQuadraticTo(&bounds, command->m_Points[0], command->m_Points[1]);
                    break;
                case OUTLINE_CUBIC_TO:
                    OutlineBoundsCubicTo(&bounds, command->m_Points[0], command->m_Points[1], command->m_Points[2]);
                    break;
                case OUTLINE_CLOSE:
                    break;
            }
        }
        return OutlineBoundsGet(&bounds, x0, y0, x1, y1);
    }

    struct OutlineBuildContext
    {
        dmArray<CurveCommand> m_Commands;
        bool                  m_Unsupported;

        OutlineBuildContext()
            : m_Unsupported(false)
        {
        }
    };

    static void EnsureOutlineCapacity(OutlineBuildContext* ctx)
    {
        if (ctx->m_Commands.Full())
        {
            ctx->m_Commands.OffsetCapacity(16);
        }
    }

    static void PushOutlineCommand(OutlineBuildContext* ctx, const CurveCommand& command)
    {
        EnsureOutlineCapacity(ctx);
        ctx->m_Commands.Push(command);
    }

    static void OutlineMoveTo(OutlineBuildContext* ctx, float scale, float to_x, float to_y)
    {
        CurveCommand command;
        memset(&command, 0, sizeof(command));
        command.m_Type = CURVE_MOVE_TO;
        command.m_Points[0].m_X = to_x * scale;
        command.m_Points[0].m_Y = to_y * scale;
        PushOutlineCommand(ctx, command);
    }

    static void OutlineLineTo(OutlineBuildContext* ctx, float scale, float to_x, float to_y)
    {
        CurveCommand command;
        memset(&command, 0, sizeof(command));
        command.m_Type = CURVE_LINE_TO;
        command.m_Points[0].m_X = to_x * scale;
        command.m_Points[0].m_Y = to_y * scale;
        PushOutlineCommand(ctx, command);
    }

    static void OutlineQuadraticTo(OutlineBuildContext* ctx, float scale, float control_x, float control_y, float to_x, float to_y)
    {
        CurveCommand command;
        memset(&command, 0, sizeof(command));
        command.m_Type = CURVE_QUADRATIC_TO;
        command.m_Points[0].m_X = control_x * scale;
        command.m_Points[0].m_Y = control_y * scale;
        command.m_Points[1].m_X = to_x * scale;
        command.m_Points[1].m_Y = to_y * scale;
        PushOutlineCommand(ctx, command);
    }

    static void OutlineClosePath(OutlineBuildContext* ctx)
    {
        CurveCommand command;
        memset(&command, 0, sizeof(command));
        command.m_Type = CURVE_CLOSE;
        PushOutlineCommand(ctx, command);
    }

    static Point OutlineMidpoint(const Point& a, const Point& b)
    {
        Point point = { (a.m_X + b.m_X) * 0.5f, (a.m_Y + b.m_Y) * 0.5f };
        return point;
    }

    static bool OutlineCubicTo(OutlineBuildContext* ctx, float scale, float tolerance_squared, const Point& p0, const Point& p1, const Point& p2, const Point& p3, uint32_t depth)
    {
        // Degree reduction followed by degree elevation bounds the error by the
        // largest control-point difference. Subdivide until that bound is small
        // in input units, keeping the approximation independent of display size.
        const float error_x = (3.0f * (p1.m_X - p2.m_X) - p0.m_X + p3.m_X) / 6.0f;
        const float error_y = (3.0f * (p1.m_Y - p2.m_Y) - p0.m_Y + p3.m_Y) / 6.0f;
        if (error_x * error_x + error_y * error_y <= tolerance_squared)
        {
            const float control_x = (3.0f * (p1.m_X + p2.m_X) - p0.m_X - p3.m_X) * 0.25f;
            const float control_y = (3.0f * (p1.m_Y + p2.m_Y) - p0.m_Y - p3.m_Y) * 0.25f;
            OutlineQuadraticTo(ctx, scale, control_x, control_y, p3.m_X, p3.m_Y);
            return true;
        }
        if (depth == 16)
            return false;

        const Point p01 = OutlineMidpoint(p0, p1);
        const Point p12 = OutlineMidpoint(p1, p2);
        const Point p23 = OutlineMidpoint(p2, p3);
        const Point p012 = OutlineMidpoint(p01, p12);
        const Point p123 = OutlineMidpoint(p12, p23);
        const Point middle = OutlineMidpoint(p012, p123);
        return OutlineCubicTo(ctx, scale, tolerance_squared, p0, p01, p012, middle, depth + 1) &&
        OutlineCubicTo(ctx, scale, tolerance_squared, middle, p123, p23, p3, depth + 1);
    }

    bool OutlineToQuadratics(const Outline* outline, float scale, float tolerance, dmArray<CurveCommand>* commands)
    {
        OutlineBuildContext ctx;
        ctx.m_Commands.SetCapacity(32);
        ctx.m_Commands.SetSize(0);
        Point current = {};
        Point start = {};

        for (uint32_t i = 0; i < outline->m_CommandCount; ++i)
        {
            const OutlineCommand& command = outline->m_Commands[i];
            switch (command.m_Type)
            {
                case OUTLINE_MOVE_TO:
                {
                    OutlineMoveTo(&ctx, scale, command.m_Points[0].m_X, command.m_Points[0].m_Y);
                    current = start = command.m_Points[0];
                    break;
                }
                case OUTLINE_LINE_TO:
                {
                    OutlineLineTo(&ctx, scale, command.m_Points[0].m_X, command.m_Points[0].m_Y);
                    current = command.m_Points[0];
                    break;
                }
                case OUTLINE_QUADRATIC_TO:
                {
                    OutlineQuadraticTo(&ctx, scale, command.m_Points[0].m_X, command.m_Points[0].m_Y, command.m_Points[1].m_X, command.m_Points[1].m_Y);
                    current = command.m_Points[1];
                    break;
                }
                case OUTLINE_CUBIC_TO:
                {
                    if (!OutlineCubicTo(&ctx, scale, tolerance * tolerance, current, command.m_Points[0], command.m_Points[1], command.m_Points[2], 0))
                        ctx.m_Unsupported = true;
                    current = command.m_Points[2];
                    break;
                }
                case OUTLINE_CLOSE:
                {
                    OutlineClosePath(&ctx);
                    current = start;
                    break;
                }
                default:
                {
                    break;
                }
            }
        }

        if (ctx.m_Unsupported)
            return false;
        commands->Swap(ctx.m_Commands);
        return true;
    }
} // namespace dmSlug
