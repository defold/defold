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
#include <float.h>
#include <stdlib.h>
#include <string.h>

#include <dmsdk/dlib/array.h>

#include "font_sdf.h"

// Distance and winding are calculated directly from the y-monotonic Bezier
// commands retained in FontOutline. Curves are not flattened, so their distance
// remains independent of a tessellation tolerance. Work is organized by row and
// then by segment: bounds reject rows and pixels before the relatively expensive
// curve-distance root search, while adjacent surviving pixels can be evaluated
// as a pair. The output uses the same top-down orientation and glyph offsets as
// the runtime cache.

// Once Bernstein subdivision has isolated a root, seven bisections followed by
// one safeguarded Newton step provide sufficient subpixel accuracy for an
// 8-bit SDF without spending more time on precision that is rounded away.
static const uint32_t ROOT_BISECTION_ITERATIONS = 7;

enum FontSDFSegmentType
{
    FONT_SDF_SEGMENT_LINE,
    FONT_SDF_SEGMENT_QUADRATIC,
    FONT_SDF_SEGMENT_CUBIC,
};

enum FontSDFBoundaryType
{
    // The entire segment separates filled and empty space.
    FONT_SDF_BOUNDARY_VISIBLE,
    // The entire segment is covered by the non-zero fill of other contours.
    FONT_SDF_BOUNDARY_BURIED,
    // Another contour can cross or overlap the segment, so visibility must be
    // checked at the closest point selected by the distance calculation.
    FONT_SDF_BOUNDARY_VARIABLE,
};

struct FontSDFSegment
{
    // Points use bitmap coordinates. A line uses P0-P1, a quadratic P0-P2,
    // and a cubic P0-P3.
    FontOutlinePoint   m_P0;
    FontOutlinePoint   m_P1;
    FontOutlinePoint   m_P2;
    FontOutlinePoint   m_P3;
    // Power-basis coefficients used by EvaluateSegmentCoordinate:
    // quadratic: P(t) = A*t^2 + B*t + P0
    // cubic:     P(t) = A*t^3 + B*t^2 + C*t + P0
    // Precomputing these avoids rebuilding the curve for every pixel.
    FontOutlinePoint   m_A;
    FontOutlinePoint   m_B;
    FontOutlinePoint   m_C;
    // Bounds contain all control points and therefore the entire Bezier. They
    // can be wider than the curve's exact bounds, which is safe for rejection.
    float              m_MinX;
    float              m_MinY;
    float              m_MaxX;
    float              m_MaxY;
    FontSDFSegmentType  m_Type;
    FontSDFBoundaryType m_BoundaryType;
    uint32_t            m_BoundaryTransitionOffset;
    uint32_t            m_BoundaryTransitionCount;
    uint32_t            m_Contour;
    // Winding contribution at a scanline crossing in y-down bitmap space:
    // +1 for increasing y, -1 for decreasing y, and 0 for horizontal segments.
    int32_t            m_Direction;
    bool               m_RequiresTwoSidedWinding;
    bool               m_BoundaryStartsVisible;
};

// Intersection between one outline segment and the current horizontal
// scanline. Crossings are sorted by x and consumed while moving across the row.
// m_Direction updates the non-zero winding count after passing the crossing.
struct FontSDFCrossing
{
    float   m_X;
    int32_t m_Direction;
};

static FontOutlinePoint ScalePoint(FontOutlinePoint point, float scale)
{
    // Font outlines are y-up, while glyph bitmaps are y-down.
    FontOutlinePoint result = { point.m_X * scale, -point.m_Y * scale };
    return result;
}

static void ExtendSegmentBounds(FontSDFSegment* segment, FontOutlinePoint point)
{
    // Extending by the control polygon is cheaper than finding exact Bezier
    // extrema and still gives a valid lower bound for distance rejection.
    if (point.m_X < segment->m_MinX) segment->m_MinX = point.m_X;
    if (point.m_Y < segment->m_MinY) segment->m_MinY = point.m_Y;
    if (point.m_X > segment->m_MaxX) segment->m_MaxX = point.m_X;
    if (point.m_Y > segment->m_MaxY) segment->m_MaxY = point.m_Y;
}

static FontOutlinePoint GetSegmentEnd(const FontSDFSegment& segment)
{
    if (segment.m_Type == FONT_SDF_SEGMENT_LINE)
        return segment.m_P1;
    if (segment.m_Type == FONT_SDF_SEGMENT_QUADRATIC)
        return segment.m_P2;

    return segment.m_P3;
}

static void PushSegment(dmArray<FontSDFSegment>& segments, FontSDFSegmentType type,
                        FontOutlinePoint p0, FontOutlinePoint p1,
                        FontOutlinePoint p2, FontOutlinePoint p3, uint32_t contour)
{
    // Copy one outline command into the representation used by the hot raster
    // loop, including coefficients, conservative bounds, and winding direction.
    FontOutlinePoint end = type == FONT_SDF_SEGMENT_LINE ? p1 : type == FONT_SDF_SEGMENT_QUADRATIC ? p2 : p3;
    if (type == FONT_SDF_SEGMENT_LINE && p0.m_X == end.m_X && p0.m_Y == end.m_Y)
        return;

    FontSDFSegment segment = {};
    segment.m_P0 = p0;
    segment.m_P1 = p1;
    segment.m_P2 = p2;
    segment.m_P3 = p3;
    segment.m_MinX = segment.m_MaxX = p0.m_X;
    segment.m_MinY = segment.m_MaxY = p0.m_Y;
    segment.m_Type = type;
    segment.m_BoundaryType = FONT_SDF_BOUNDARY_VISIBLE;
    segment.m_Contour = contour;
    segment.m_Direction = p0.m_Y < end.m_Y ? 1 : p0.m_Y > end.m_Y ? -1 : 0;
    segment.m_RequiresTwoSidedWinding = segment.m_Direction == 0;
    if (type == FONT_SDF_SEGMENT_QUADRATIC)
    {
        segment.m_A = { p0.m_X - 2.0f * p1.m_X + p2.m_X, p0.m_Y - 2.0f * p1.m_Y + p2.m_Y };
        segment.m_B = { 2.0f * (p1.m_X - p0.m_X), 2.0f * (p1.m_Y - p0.m_Y) };
    }
    else if (type == FONT_SDF_SEGMENT_CUBIC)
    {
        segment.m_A = { -p0.m_X + 3.0f * p1.m_X - 3.0f * p2.m_X + p3.m_X,
                        -p0.m_Y + 3.0f * p1.m_Y - 3.0f * p2.m_Y + p3.m_Y };
        segment.m_B = { 3.0f * p0.m_X - 6.0f * p1.m_X + 3.0f * p2.m_X,
                        3.0f * p0.m_Y - 6.0f * p1.m_Y + 3.0f * p2.m_Y };
        segment.m_C = { 3.0f * (p1.m_X - p0.m_X), 3.0f * (p1.m_Y - p0.m_Y) };
    }
    ExtendSegmentBounds(&segment, p1);
    if (type != FONT_SDF_SEGMENT_LINE)
        ExtendSegmentBounds(&segment, p2);
    if (type == FONT_SDF_SEGMENT_CUBIC)
        ExtendSegmentBounds(&segment, p3);
    segments.Push(segment);
}

static void BuildSegments(const FontOutline* outline, float scale, dmArray<FontSDFSegment>& segments)
{
    // MOVE emits no segment and every other command emits at most one, making
    // command count a tight upper bound that avoids growth reallocations.
    if (outline->m_CommandCount != 0)
        segments.SetCapacity(outline->m_CommandCount);
    FontOutlinePoint current = {};
    FontOutlinePoint first = {};
    bool contour_open = false;
    uint32_t contour = UINT32_MAX;
    for (uint32_t i = 0; i < outline->m_CommandCount; ++i)
    {
        const FontOutlineCommand* command = &outline->m_Commands[i];
        switch (command->m_Type)
        {
        case FONT_OUTLINE_MOVE_TO:
            ++contour;
            current = ScalePoint(command->m_Points[0], scale);
            first = current;
            contour_open = true;
            break;
        case FONT_OUTLINE_LINE_TO:
            {
                FontOutlinePoint to = ScalePoint(command->m_Points[0], scale);
                PushSegment(segments, FONT_SDF_SEGMENT_LINE, current, to, {}, {}, contour);
                current = to;
            }
            break;
        case FONT_OUTLINE_QUADRATIC_TO:
            {
                FontOutlinePoint control = ScalePoint(command->m_Points[0], scale);
                FontOutlinePoint to = ScalePoint(command->m_Points[1], scale);
                PushSegment(segments, FONT_SDF_SEGMENT_QUADRATIC, current, control, to, {}, contour);
                current = to;
            }
            break;
        case FONT_OUTLINE_CUBIC_TO:
            {
                FontOutlinePoint control_1 = ScalePoint(command->m_Points[0], scale);
                FontOutlinePoint control_2 = ScalePoint(command->m_Points[1], scale);
                FontOutlinePoint to = ScalePoint(command->m_Points[2], scale);
                PushSegment(segments, FONT_SDF_SEGMENT_CUBIC, current, control_1, control_2, to, contour);
                current = to;
            }
            break;
        case FONT_OUTLINE_CLOSE:
            if (contour_open)
                PushSegment(segments, FONT_SDF_SEGMENT_LINE, current, first, {}, {}, contour);
            contour_open = false;
            break;
        }
    }
}

static double Dot(double ax, double ay, double bx, double by)
{
    return ax * bx + ay * by;
}

static float PointAABBDistanceSquared(FontOutlinePoint point, const FontSDFSegment& segment)
{
    // A lower bound on the distance to the segment. If this is no better than
    // the current result, the exact line or curve distance cannot improve it.
    float dx = 0.0f;
    float dy = 0.0f;
    if (point.m_X < segment.m_MinX)
        dx = segment.m_MinX - point.m_X;
    else if (point.m_X > segment.m_MaxX)
        dx = point.m_X - segment.m_MaxX;
    if (point.m_Y < segment.m_MinY)
        dy = segment.m_MinY - point.m_Y;
    else if (point.m_Y > segment.m_MaxY)
        dy = point.m_Y - segment.m_MaxY;
    return dx * dx + dy * dy;
}

static float PointLineDistanceSquared(FontOutlinePoint point, const FontSDFSegment& segment, double* parameter = 0)
{
    // Project onto the infinite line, then clamp to the finite segment.
    double dx = segment.m_P1.m_X - segment.m_P0.m_X;
    double dy = segment.m_P1.m_Y - segment.m_P0.m_Y;
    double t = ((point.m_X - segment.m_P0.m_X) * dx + (point.m_Y - segment.m_P0.m_Y) * dy) /
               (dx * dx + dy * dy);
    if (t < 0.0) t = 0.0;
    if (t > 1.0) t = 1.0;
    if (parameter)
        *parameter = t;
    double distance_x = point.m_X - (segment.m_P0.m_X + t * dx);
    double distance_y = point.m_Y - (segment.m_P0.m_Y + t * dy);
    return (float)(distance_x * distance_x + distance_y * distance_y);
}

static double EvaluatePolynomial(const double* coefficients, uint32_t degree, double t)
{
    // Coefficients are stored in ascending power order. Horner evaluation
    // starts at the highest power and works back towards the constant term.
    double value = coefficients[degree];
    while (degree > 0)
        value = value * t + coefficients[--degree];
    return value;
}

static double EvaluatePolynomialDerivative(const double* coefficients, uint32_t degree, double t)
{
    double value = degree * coefficients[degree];
    while (degree > 1)
    {
        --degree;
        value = value * t + degree * coefficients[degree];
    }
    return value;
}

static void AddRoot(double root, double* roots, uint32_t* root_count)
{
    // Endpoints are tested explicitly by PointCurveDistanceSquaredFromRoots.
    // Keeping only unique interior roots avoids evaluating them twice.
    if (root <= 0.0 || root >= 1.0)
        return;

    for (uint32_t i = 0; i < *root_count; ++i)
    {
        if (fabs(roots[i] - root) < 1.0e-7)
            return;
    }

    roots[(*root_count)++] = root;
}

static uint32_t CountSignChanges(const double* coefficients, uint32_t degree)
{
    // The number of sign changes in Bernstein coefficients bounds the number
    // of roots in their parameter interval (Descartes' rule of signs).
    double scale = 0.0;
    for (uint32_t i = 0; i <= degree; ++i)
    {
        double magnitude = fabs(coefficients[i]);
        if (magnitude > scale)
            scale = magnitude;
    }
    double epsilon = scale * 1.0e-12;
    int previous_sign = 0;
    uint32_t changes = 0;
    for (uint32_t i = 0; i <= degree; ++i)
    {
        if (fabs(coefficients[i]) <= epsilon)
            continue;

        int sign = coefficients[i] < 0.0 ? -1 : 1;
        changes += previous_sign != 0 && sign != previous_sign;
        previous_sign = sign;
    }
    return changes;
}

static void SplitBernstein(const double* coefficients, uint32_t degree, double* left, double* right)
{
    // de Casteljau subdivision at t=0.5 produces Bernstein coefficients for
    // the left and right halves without converting back to power basis.
    double work[6];
    memcpy(work, coefficients, sizeof(double) * (degree + 1));
    left[0] = work[0];
    right[degree] = work[degree];
    for (uint32_t level = 1; level <= degree; ++level)
    {
        for (uint32_t i = 0; i <= degree - level; ++i)
            work[i] = (work[i] + work[i + 1]) * 0.5;
        left[level] = work[0];
        right[degree - level] = work[degree - level];
    }
}

static void FindBernsteinRoots(const double* coefficients, const double* polynomial, uint32_t degree,
                               double t0, double t1, uint32_t depth, double* roots, uint32_t* root_count)
{
    // Descartes' rule of signs in Bernstein form bounds the roots in an
    // interval. Subdivide ambiguous intervals and refine isolated roots with a
    // fixed bisection budget followed by one safeguarded Newton step.
    uint32_t changes = CountSignChanges(coefficients, degree);
    if (changes == 0)
        return;

    if (changes == 1)
    {
        double low_value = EvaluatePolynomial(polynomial, degree, t0);
        double high_value = EvaluatePolynomial(polynomial, degree, t1);
        if ((low_value < 0.0) == (high_value < 0.0))
            return;

        for (uint32_t iteration = 0; iteration < ROOT_BISECTION_ITERATIONS; ++iteration)
        {
            double middle = (t0 + t1) * 0.5;
            double middle_value = EvaluatePolynomial(polynomial, degree, middle);
            if ((low_value < 0.0) == (middle_value < 0.0))
            {
                t0 = middle;
                low_value = middle_value;
            }
            else
            {
                t1 = middle;
            }
        }
        double root = (t0 + t1) * 0.5;
        double derivative = EvaluatePolynomialDerivative(polynomial, degree, root);
        if (derivative != 0.0)
        {
            double refined_root = root - EvaluatePolynomial(polynomial, degree, root) / derivative;
            if (refined_root > t0 && refined_root < t1)
                root = refined_root;
        }
        AddRoot(root, roots, root_count);
        return;
    }

    if (depth == 20)
        return;

    double left[6];
    double right[6];
    SplitBernstein(coefficients, degree, left, right);
    double middle = (t0 + t1) * 0.5;
    FindBernsteinRoots(left, polynomial, degree, t0, middle, depth + 1, roots, root_count);
    FindBernsteinRoots(right, polynomial, degree, middle, t1, depth + 1, roots, root_count);
}

static void ConvertPolynomialToBernstein(const double* polynomial, uint32_t degree, double* bernstein)
{
    // Convert the only two degrees used here directly from power basis to
    // Bernstein basis. This is a hot path, so avoid the general binomial sum.
    if (degree == 3)
    {
        bernstein[0] = polynomial[0];
        bernstein[1] = polynomial[0] + polynomial[1] / 3.0;
        bernstein[2] = polynomial[0] + 2.0 * polynomial[1] / 3.0 + polynomial[2] / 3.0;
        bernstein[3] = polynomial[0] + polynomial[1] + polynomial[2] + polynomial[3];
    }
    else
    {
        bernstein[0] = polynomial[0];
        bernstein[1] = polynomial[0] + polynomial[1] / 5.0;
        bernstein[2] = polynomial[0] + 2.0 * polynomial[1] / 5.0 + polynomial[2] / 10.0;
        bernstein[3] = polynomial[0] + 3.0 * polynomial[1] / 5.0 + 3.0 * polynomial[2] / 10.0 + polynomial[3] / 10.0;
        bernstein[4] = polynomial[0] + 4.0 * polynomial[1] / 5.0 + 3.0 * polynomial[2] / 5.0 + 2.0 * polynomial[3] / 5.0 + polynomial[4] / 5.0;
        bernstein[5] = polynomial[0] + polynomial[1] + polynomial[2] + polynomial[3] + polynomial[4] + polynomial[5];
    }
}

static uint32_t FindPolynomialRoots(const double* polynomial, uint32_t degree, double* roots)
{
    // Only roots inside the Bezier parameter interval (0, 1) are returned.
    double bernstein[6];
    ConvertPolynomialToBernstein(polynomial, degree, bernstein);
    uint32_t root_count = 0;
    FindBernsteinRoots(bernstein, polynomial, degree, 0.0, 1.0, 0, roots, &root_count);
    return root_count;
}

#if defined(__clang__)
// Clang lowers this architecture-neutral pair to NEON or wasm SIMD when the
// target supports it, and to scalar operations otherwise.
typedef double Double2 __attribute__((ext_vector_type(2)));

static Double2 EvaluatePolynomialPair(const double polynomial[2][6], uint32_t degree, Double2 t)
{
    Double2 value = { polynomial[0][degree], polynomial[1][degree] };
    while (degree > 0)
    {
        --degree;
        Double2 coefficient = { polynomial[0][degree], polynomial[1][degree] };
        value = value * t + coefficient;
    }
    return value;
}

static Double2 EvaluatePolynomialDerivativePair(const double polynomial[2][6], uint32_t degree, Double2 t)
{
    Double2 value = { degree * polynomial[0][degree], degree * polynomial[1][degree] };
    while (degree > 1)
    {
        --degree;
        Double2 coefficient = { degree * polynomial[0][degree], degree * polynomial[1][degree] };
        value = value * t + coefficient;
    }
    return value;
}

static bool FindPolynomialRootPair(const double polynomial[2][6], uint32_t degree, double roots[2])
{
    // The common one-root case is evaluated in two lanes. More complicated
    // polynomials fall back to the complete scalar root isolation algorithm.
    double bernstein[2][6];
    ConvertPolynomialToBernstein(polynomial[0], degree, bernstein[0]);
    ConvertPolynomialToBernstein(polynomial[1], degree, bernstein[1]);
    if (CountSignChanges(bernstein[0], degree) != 1 || CountSignChanges(bernstein[1], degree) != 1)
        return false;

    Double2 low = { 0.0, 0.0 };
    Double2 high = { 1.0, 1.0 };
    Double2 zero = { 0.0, 0.0 };
    Double2 low_value = EvaluatePolynomialPair(polynomial, degree, low);
    Double2 high_value = EvaluatePolynomialPair(polynomial, degree, high);
    if ((low_value[0] < 0.0) == (high_value[0] < 0.0) ||
        (low_value[1] < 0.0) == (high_value[1] < 0.0))
        return false;

    for (uint32_t iteration = 0; iteration < ROOT_BISECTION_ITERATIONS; ++iteration)
    {
        Double2 middle = (low + high) * 0.5;
        Double2 middle_value = EvaluatePolynomialPair(polynomial, degree, middle);
        auto same_sign = (low_value < zero) == (middle_value < zero);
        low = same_sign ? middle : low;
        high = same_sign ? high : middle;
        low_value = same_sign ? middle_value : low_value;
    }
    Double2 root = (low + high) * 0.5;
    Double2 derivative = EvaluatePolynomialDerivativePair(polynomial, degree, root);
    Double2 refined = root - EvaluatePolynomialPair(polynomial, degree, root) / derivative;
    auto use_refined = (derivative != zero) & (refined > low) & (refined < high);
    root = use_refined ? refined : root;
    roots[0] = root[0];
    roots[1] = root[1];
    return true;
}
#endif

static double EvaluateSegmentCoordinate(const FontSDFSegment& segment, double t, bool x_coordinate)
{
    // Evaluate one coordinate from the precomputed power-basis coefficients.
    double p0 = x_coordinate ? segment.m_P0.m_X : segment.m_P0.m_Y;
    double p1 = x_coordinate ? segment.m_P1.m_X : segment.m_P1.m_Y;
    if (segment.m_Type == FONT_SDF_SEGMENT_LINE)
        return p0 + (p1 - p0) * t;

    if (segment.m_Type == FONT_SDF_SEGMENT_QUADRATIC)
    {
        double a = x_coordinate ? segment.m_A.m_X : segment.m_A.m_Y;
        double b = x_coordinate ? segment.m_B.m_X : segment.m_B.m_Y;
        return (a * t + b) * t + p0;
    }

    double a = x_coordinate ? segment.m_A.m_X : segment.m_A.m_Y;
    double b = x_coordinate ? segment.m_B.m_X : segment.m_B.m_Y;
    double c = x_coordinate ? segment.m_C.m_X : segment.m_C.m_Y;
    return ((a * t + b) * t + c) * t + p0;
}

static void BuildDistancePolynomial(FontOutlinePoint point, const FontSDFSegment& segment, double* coefficients)
{
    // Interior extrema of squared distance satisfy
    // dot(curve(t) - point, curve'(t)) == 0. This is cubic for a quadratic
    // Bezier and quintic for a cubic Bezier.
    memset(coefficients, 0, sizeof(double) * 6);
    if (segment.m_Type == FONT_SDF_SEGMENT_QUADRATIC)
    {
        double ax = segment.m_A.m_X;
        double ay = segment.m_A.m_Y;
        double bx = segment.m_B.m_X;
        double by = segment.m_B.m_Y;
        double qx = segment.m_P0.m_X - point.m_X;
        double qy = segment.m_P0.m_Y - point.m_Y;
        coefficients[0] = Dot(bx, by, qx, qy);
        coefficients[1] = Dot(bx, by, bx, by) + 2.0 * Dot(ax, ay, qx, qy);
        coefficients[2] = 3.0 * Dot(ax, ay, bx, by);
        coefficients[3] = 2.0 * Dot(ax, ay, ax, ay);
    }
    else
    {
        double ax = segment.m_A.m_X;
        double ay = segment.m_A.m_Y;
        double bx = segment.m_B.m_X;
        double by = segment.m_B.m_Y;
        double cx = segment.m_C.m_X;
        double cy = segment.m_C.m_Y;
        double qx = segment.m_P0.m_X - point.m_X;
        double qy = segment.m_P0.m_Y - point.m_Y;
        coefficients[0] = Dot(cx, cy, qx, qy);
        coefficients[1] = Dot(cx, cy, cx, cy) + 2.0 * Dot(bx, by, qx, qy);
        coefficients[2] = 3.0 * (Dot(bx, by, cx, cy) + Dot(ax, ay, qx, qy));
        coefficients[3] = 4.0 * Dot(ax, ay, cx, cy) + 2.0 * Dot(bx, by, bx, by);
        coefficients[4] = 5.0 * Dot(ax, ay, bx, by);
        coefficients[5] = 3.0 * Dot(ax, ay, ax, ay);
    }
}

static float PointCurveDistanceSquaredFromRoots(FontOutlinePoint point, const FontSDFSegment& segment,
                                                const double* roots, uint32_t root_count)
{
    // The closest point is either an endpoint or one of the stationary points
    // supplied by the distance polynomial.
    FontOutlinePoint end = segment.m_Type == FONT_SDF_SEGMENT_QUADRATIC ? segment.m_P2 : segment.m_P3;
    double distance_x = point.m_X - segment.m_P0.m_X;
    double distance_y = point.m_Y - segment.m_P0.m_Y;
    double best = distance_x * distance_x + distance_y * distance_y;
    distance_x = point.m_X - end.m_X;
    distance_y = point.m_Y - end.m_Y;
    double end_distance = distance_x * distance_x + distance_y * distance_y;
    if (end_distance < best)
        best = end_distance;

    for (uint32_t i = 0; i < root_count; ++i)
    {
        double curve_x = EvaluateSegmentCoordinate(segment, roots[i], true);
        double curve_y = EvaluateSegmentCoordinate(segment, roots[i], false);
        distance_x = point.m_X - curve_x;
        distance_y = point.m_Y - curve_y;
        double distance = distance_x * distance_x + distance_y * distance_y;
        if (distance < best)
            best = distance;
    }
    return (float)best;
}

static float PointCurveDistanceSquared(FontOutlinePoint point, const FontSDFSegment& segment,
                                       const double* coefficients)
{
    double roots[5];
    uint32_t root_count = FindPolynomialRoots(coefficients,
                                               segment.m_Type == FONT_SDF_SEGMENT_QUADRATIC ? 3 : 5,
                                               roots);
    return PointCurveDistanceSquaredFromRoots(point, segment, roots, root_count);
}

#if defined(__clang__)
static void PointCurveDistanceSquaredPair(const FontOutlinePoint points[2], const FontSDFSegment& segment,
                                          float distances_squared[2])
{
    double polynomial[2][6];
    BuildDistancePolynomial(points[0], segment, polynomial[0]);
    BuildDistancePolynomial(points[1], segment, polynomial[1]);
    double roots[2];
    uint32_t degree = segment.m_Type == FONT_SDF_SEGMENT_QUADRATIC ? 3 : 5;
    if (FindPolynomialRootPair(polynomial, degree, roots))
    {
        distances_squared[0] = PointCurveDistanceSquaredFromRoots(points[0], segment, &roots[0], 1);
        distances_squared[1] = PointCurveDistanceSquaredFromRoots(points[1], segment, &roots[1], 1);
    }
    else
    {
        distances_squared[0] = PointCurveDistanceSquared(points[0], segment, polynomial[0]);
        distances_squared[1] = PointCurveDistanceSquared(points[1], segment, polynomial[1]);
    }
}
#endif

static float FindCurveCrossingX(const FontSDFSegment& segment, float y)
{
    // FontOutline curves are y-monotonic, so a scanline has at most one
    // intersection with a segment and bisection cannot select the wrong root.
    if (segment.m_Type == FONT_SDF_SEGMENT_LINE)
    {
        float t = (y - segment.m_P0.m_Y) / (segment.m_P1.m_Y - segment.m_P0.m_Y);
        return (float)EvaluateSegmentCoordinate(segment, t, true);
    }

    double low = 0.0;
    double high = 1.0;
    bool increasing = segment.m_Direction > 0;
    FontOutlinePoint end = segment.m_Type == FONT_SDF_SEGMENT_QUADRATIC ? segment.m_P2 : segment.m_P3;
    double t = (y - segment.m_P0.m_Y) / (end.m_Y - segment.m_P0.m_Y);
    for (uint32_t i = 0; i < 8; ++i)
    {
        double curve_y = EvaluateSegmentCoordinate(segment, t, false);
        bool below = curve_y < y;
        if (below == increasing)
            low = t;
        else
            high = t;

        double derivative_y;
        if (segment.m_Type == FONT_SDF_SEGMENT_QUADRATIC)
            derivative_y = 2.0 * segment.m_A.m_Y * t + segment.m_B.m_Y;
        else
            derivative_y = (3.0 * segment.m_A.m_Y * t + 2.0 * segment.m_B.m_Y) * t + segment.m_C.m_Y;
        double refined = derivative_y == 0.0 ? low : t - (curve_y - y) / derivative_y;
        t = refined > low && refined < high ? refined : (low + high) * 0.5;
    }
    return (float)EvaluateSegmentCoordinate(segment, t, true);
}

static FontOutlinePoint EvaluateSegmentPoint(const FontSDFSegment& segment, double t)
{
    FontOutlinePoint point = {
        (float)EvaluateSegmentCoordinate(segment, t, true),
        (float)EvaluateSegmentCoordinate(segment, t, false)
    };
    return point;
}

static FontOutlinePoint EvaluateSegmentTangent(const FontSDFSegment& segment, double t)
{
    FontOutlinePoint tangent;
    if (segment.m_Type == FONT_SDF_SEGMENT_LINE)
    {
        tangent.m_X = segment.m_P1.m_X - segment.m_P0.m_X;
        tangent.m_Y = segment.m_P1.m_Y - segment.m_P0.m_Y;
    }
    else if (segment.m_Type == FONT_SDF_SEGMENT_QUADRATIC)
    {
        tangent.m_X = (float)(2.0 * segment.m_A.m_X * t + segment.m_B.m_X);
        tangent.m_Y = (float)(2.0 * segment.m_A.m_Y * t + segment.m_B.m_Y);
    }
    else
    {
        tangent.m_X = (float)((3.0 * segment.m_A.m_X * t + 2.0 * segment.m_B.m_X) * t + segment.m_C.m_X);
        tangent.m_Y = (float)((3.0 * segment.m_A.m_Y * t + 2.0 * segment.m_B.m_Y) * t + segment.m_C.m_Y);
    }
    return tangent;
}

static int32_t GetWindingAtPoint(const dmArray<FontSDFSegment>& segments, FontOutlinePoint point)
{
    // Cast a horizontal ray to the right. Horizontal segments do not cross
    // the ray, and the lower-inclusive/upper-exclusive interval counts shared
    // contour vertices exactly once.
    int32_t winding = 0;
    for (uint32_t i = 0; i < segments.Size(); ++i)
    {
        const FontSDFSegment& segment = segments[i];
        if (segment.m_Direction == 0)
            continue;

        FontOutlinePoint end = GetSegmentEnd(segment);
        float min_y = segment.m_P0.m_Y < end.m_Y ? segment.m_P0.m_Y : end.m_Y;
        float max_y = segment.m_P0.m_Y > end.m_Y ? segment.m_P0.m_Y : end.m_Y;
        if (point.m_Y < min_y || point.m_Y >= max_y)
            continue;

        if (FindCurveCrossingX(segment, point.m_Y) > point.m_X)
            winding += segment.m_Direction;
    }
    return winding;
}

static bool IsBoundaryPoint(const dmArray<FontSDFSegment>& segments,
                            const FontSDFSegment& segment, double t)
{
    // Avoid sampling exactly at a shared contour vertex, where the tangent of
    // only one adjoining segment does not describe both sides of the corner.
    const double endpoint_inset = 1.0e-5;
    if (t < endpoint_inset) t = endpoint_inset;
    if (t > 1.0 - endpoint_inset) t = 1.0 - endpoint_inset;

    FontOutlinePoint point = EvaluateSegmentPoint(segment, t);
    FontOutlinePoint tangent = EvaluateSegmentTangent(segment, t);
    float tangent_length_squared = tangent.m_X * tangent.m_X + tangent.m_Y * tangent.m_Y;
    if (tangent_length_squared < 1.0e-12f)
        return true;

    // One thousandth of a bitmap pixel is large enough to produce distinct
    // float coordinates and small enough not to cross a neighboring feature.
    float inverse_length = 1.0f / sqrtf(tangent_length_squared);
    float normal_x = -tangent.m_Y * inverse_length * 1.0e-3f;
    float normal_y = tangent.m_X * inverse_length * 1.0e-3f;
    int32_t left_winding;
    int32_t right_winding;
    if (!segment.m_RequiresTwoSidedWinding)
    {
        // For a non-horizontal, non-coincident segment, crossing from left to
        // right removes exactly m_Direction from this rightward-ray winding.
        FontOutlinePoint horizontal_left = { point.m_X - 1.0e-3f, point.m_Y };
        left_winding = GetWindingAtPoint(segments, horizontal_left);
        right_winding = left_winding - segment.m_Direction;
    }
    else
    {
        FontOutlinePoint left = { point.m_X + normal_x, point.m_Y + normal_y };
        FontOutlinePoint right = { point.m_X - normal_x, point.m_Y - normal_y };
        left_winding = GetWindingAtPoint(segments, left);
        right_winding = GetWindingAtPoint(segments, right);
    }
    bool left_filled = left_winding != 0;
    bool right_filled = right_winding != 0;
    return left_filled != right_filled;
}

static bool IsBoundaryPoint(const FontSDFSegment& segment,
                            const dmArray<float>& boundary_transitions, double t)
{
    bool visible = segment.m_BoundaryStartsVisible;
    for (uint32_t i = 0; i < segment.m_BoundaryTransitionCount; ++i)
    {
        if (t < boundary_transitions[segment.m_BoundaryTransitionOffset + i])
            break;

        visible = !visible;
    }
    return visible;
}

static void AccumulatePointDistanceSquared(FontOutlinePoint point,
                                           const FontSDFSegment& segment,
                                           double t, double* best)
{
    FontOutlinePoint curve_point = EvaluateSegmentPoint(segment, t);
    double distance_x = point.m_X - curve_point.m_X;
    double distance_y = point.m_Y - curve_point.m_Y;
    double distance = distance_x * distance_x + distance_y * distance_y;
    if (distance < *best)
        *best = distance;
}

static bool SegmentBoundsOverlap(const FontSDFSegment& left, const FontSDFSegment& right)
{
    return left.m_MinX <= right.m_MaxX && left.m_MaxX >= right.m_MinX &&
           left.m_MinY <= right.m_MaxY && left.m_MaxY >= right.m_MinY;
}

static float PointLineDistanceSquared(FontOutlinePoint point, FontOutlinePoint line_start,
                                      FontOutlinePoint line_end)
{
    float dx = line_end.m_X - line_start.m_X;
    float dy = line_end.m_Y - line_start.m_Y;
    float length_squared = dx * dx + dy * dy;
    if (length_squared == 0.0f)
    {
        dx = point.m_X - line_start.m_X;
        dy = point.m_Y - line_start.m_Y;
        return dx * dx + dy * dy;
    }

    float cross = dx * (line_start.m_Y - point.m_Y) - dy * (line_start.m_X - point.m_X);
    return cross * cross / length_squared;
}

static bool SegmentIsFlat(const FontSDFSegment& segment)
{
    if (segment.m_Type == FONT_SDF_SEGMENT_LINE)
        return true;

    FontOutlinePoint end = segment.m_Type == FONT_SDF_SEGMENT_QUADRATIC ? segment.m_P2 : segment.m_P3;
    const float flatness_squared = 1.0e-6f;
    if (PointLineDistanceSquared(segment.m_P1, segment.m_P0, end) > flatness_squared)
        return false;

    return segment.m_Type != FONT_SDF_SEGMENT_CUBIC ||
           PointLineDistanceSquared(segment.m_P2, segment.m_P0, end) <= flatness_squared;
}

static void SetIntersectionSegmentBounds(FontSDFSegment* segment)
{
    segment->m_MinX = segment->m_MaxX = segment->m_P0.m_X;
    segment->m_MinY = segment->m_MaxY = segment->m_P0.m_Y;
    ExtendSegmentBounds(segment, segment->m_P1);
    if (segment->m_Type != FONT_SDF_SEGMENT_LINE)
        ExtendSegmentBounds(segment, segment->m_P2);
    if (segment->m_Type == FONT_SDF_SEGMENT_CUBIC)
        ExtendSegmentBounds(segment, segment->m_P3);
}

static FontOutlinePoint Midpoint(FontOutlinePoint left, FontOutlinePoint right)
{
    FontOutlinePoint result = { (left.m_X + right.m_X) * 0.5f,
                                (left.m_Y + right.m_Y) * 0.5f };
    return result;
}

static void SplitIntersectionSegment(const FontSDFSegment& segment,
                                     FontSDFSegment* left, FontSDFSegment* right)
{
    *left = segment;
    *right = segment;
    if (segment.m_Type == FONT_SDF_SEGMENT_LINE)
    {
        FontOutlinePoint middle = Midpoint(segment.m_P0, segment.m_P1);
        left->m_P1 = middle;
        right->m_P0 = middle;
    }
    else if (segment.m_Type == FONT_SDF_SEGMENT_QUADRATIC)
    {
        FontOutlinePoint p01 = Midpoint(segment.m_P0, segment.m_P1);
        FontOutlinePoint p12 = Midpoint(segment.m_P1, segment.m_P2);
        FontOutlinePoint middle = Midpoint(p01, p12);
        left->m_P1 = p01;
        left->m_P2 = middle;
        right->m_P0 = middle;
        right->m_P1 = p12;
    }
    else
    {
        FontOutlinePoint p01 = Midpoint(segment.m_P0, segment.m_P1);
        FontOutlinePoint p12 = Midpoint(segment.m_P1, segment.m_P2);
        FontOutlinePoint p23 = Midpoint(segment.m_P2, segment.m_P3);
        FontOutlinePoint p012 = Midpoint(p01, p12);
        FontOutlinePoint p123 = Midpoint(p12, p23);
        FontOutlinePoint middle = Midpoint(p012, p123);
        left->m_P1 = p01;
        left->m_P2 = p012;
        left->m_P3 = middle;
        right->m_P0 = middle;
        right->m_P1 = p123;
        right->m_P2 = p23;
    }
    SetIntersectionSegmentBounds(left);
    SetIntersectionSegmentBounds(right);
}

static float Orientation(FontOutlinePoint a, FontOutlinePoint b, FontOutlinePoint c)
{
    return (b.m_X - a.m_X) * (c.m_Y - a.m_Y) -
           (b.m_Y - a.m_Y) * (c.m_X - a.m_X);
}

static bool FlatSegmentsIntersect(const FontSDFSegment& left, const FontSDFSegment& right)
{
    FontOutlinePoint left_end = GetSegmentEnd(left);
    FontOutlinePoint right_end = GetSegmentEnd(right);
    float a = Orientation(left.m_P0, left_end, right.m_P0);
    float b = Orientation(left.m_P0, left_end, right_end);
    float c = Orientation(right.m_P0, right_end, left.m_P0);
    float d = Orientation(right.m_P0, right_end, left_end);
    const float epsilon = 1.0e-5f;
    if (((a > epsilon && b < -epsilon) || (a < -epsilon && b > epsilon)) &&
        ((c > epsilon && d < -epsilon) || (c < -epsilon && d > epsilon)))
        return true;

    // Bounds already overlap. Near-zero orientations therefore represent a
    // touching endpoint or a collinear overlap, both of which can change which
    // portion of a contour is exposed by the non-zero fill.
    return fabsf(a) <= epsilon || fabsf(b) <= epsilon ||
           fabsf(c) <= epsilon || fabsf(d) <= epsilon;
}

static bool SegmentsAreCoincident(const FontSDFSegment& left, const FontSDFSegment& right)
{
    if (!SegmentBoundsOverlap(left, right))
        return false;

    if (SegmentIsFlat(left) && SegmentIsFlat(right))
    {
        FontOutlinePoint left_end = GetSegmentEnd(left);
        FontOutlinePoint right_end = GetSegmentEnd(right);
        const float epsilon = 1.0e-5f;
        return fabsf(Orientation(left.m_P0, left_end, right.m_P0)) <= epsilon &&
               fabsf(Orientation(left.m_P0, left_end, right_end)) <= epsilon;
    }

    if (left.m_Type != right.m_Type)
        return false;

    FontOutlinePoint left_end = left.m_Type == FONT_SDF_SEGMENT_QUADRATIC ? left.m_P2 : left.m_P3;
    FontOutlinePoint right_end = right.m_Type == FONT_SDF_SEGMENT_QUADRATIC ? right.m_P2 : right.m_P3;
    bool same_direction = left.m_P0.m_X == right.m_P0.m_X && left.m_P0.m_Y == right.m_P0.m_Y &&
                          left_end.m_X == right_end.m_X && left_end.m_Y == right_end.m_Y;
    bool opposite_direction = left.m_P0.m_X == right_end.m_X && left.m_P0.m_Y == right_end.m_Y &&
                              left_end.m_X == right.m_P0.m_X && left_end.m_Y == right.m_P0.m_Y;
    if (!same_direction && !opposite_direction)
        return false;

    if (left.m_Type == FONT_SDF_SEGMENT_QUADRATIC)
        return left.m_P1.m_X == right.m_P1.m_X && left.m_P1.m_Y == right.m_P1.m_Y;

    return same_direction
        ? left.m_P1.m_X == right.m_P1.m_X && left.m_P1.m_Y == right.m_P1.m_Y &&
          left.m_P2.m_X == right.m_P2.m_X && left.m_P2.m_Y == right.m_P2.m_Y
        : left.m_P1.m_X == right.m_P2.m_X && left.m_P1.m_Y == right.m_P2.m_Y &&
          left.m_P2.m_X == right.m_P1.m_X && left.m_P2.m_Y == right.m_P1.m_Y;
}

static bool SegmentsIntersect(const FontSDFSegment& left, const FontSDFSegment& right,
                              uint32_t depth = 0)
{
    if (!SegmentBoundsOverlap(left, right))
        return false;

    bool left_flat = SegmentIsFlat(left);
    bool right_flat = SegmentIsFlat(right);
    if (left_flat && right_flat)
        return FlatSegmentsIntersect(left, right);

    // A depth limit makes pathological, nearly coincident curves conservative:
    // they use boundary checks instead of risking a buried edge being retained.
    if (depth == 16)
        return true;

    float left_extent = (left.m_MaxX - left.m_MinX) + (left.m_MaxY - left.m_MinY);
    float right_extent = (right.m_MaxX - right.m_MinX) + (right.m_MaxY - right.m_MinY);
    if (!left_flat && (right_flat || left_extent >= right_extent))
    {
        FontSDFSegment first;
        FontSDFSegment second;
        SplitIntersectionSegment(left, &first, &second);
        return SegmentsIntersect(first, right, depth + 1) ||
               SegmentsIntersect(second, right, depth + 1);
    }

    FontSDFSegment first;
    FontSDFSegment second;
    SplitIntersectionSegment(right, &first, &second);
    return SegmentsIntersect(left, first, depth + 1) ||
           SegmentsIntersect(left, second, depth + 1);
}

static float FindBoundaryTransition(const dmArray<FontSDFSegment>& segments,
                                    const FontSDFSegment& segment,
                                    double low, double high, bool low_visible)
{
    // Visibility changes only where this segment meets another contour. The
    // coarse samples locate that interval; bisection then retains a stable
    // transition point for the distance pass instead of quantizing it to a
    // sample-mask bit.
    for (uint32_t iteration = 0; iteration < 12; ++iteration)
    {
        double middle = (low + high) * 0.5;
        bool middle_visible = IsBoundaryPoint(segments, segment, middle);
        if (middle_visible == low_visible)
            low = middle;
        else
            high = middle;
    }
    return (float)((low + high) * 0.5);
}

static void ClassifySegmentBoundaries(dmArray<FontSDFSegment>& segments,
                                      dmArray<float>& boundary_transitions)
{
    if (segments.Empty())
        return;

    // A single contour cannot bury one of its own edges in normal font
    // outlines, so preserve the original zero-overhead path for this common
    // case. Different contours may overlap, nest as holes, or fully cover one
    // another and therefore require non-zero-fill boundary classification.
    uint32_t contour_count = segments[segments.Size() - 1].m_Contour + 1;
    if (contour_count == 1)
        return;

    dmArray<uint8_t> variable_segments;
    dmArray<uint8_t> intersecting_contours;
    variable_segments.SetCapacity(segments.Size());
    variable_segments.SetSize(segments.Size());
    intersecting_contours.SetCapacity(contour_count);
    intersecting_contours.SetSize(contour_count);
    memset(variable_segments.Begin(), 0, variable_segments.Size());
    memset(intersecting_contours.Begin(), 0, intersecting_contours.Size());

    // Test each cross-contour pair once. Only segments that actually meet need
    // parameter-dependent visibility; conservative control-box overlap alone
    // is not enough because nested hole contours overlap extensively in bounds.
    for (uint32_t i = 0; i < segments.Size(); ++i)
    {
        for (uint32_t j = i + 1; j < segments.Size(); ++j)
        {
            if (segments[i].m_Contour == segments[j].m_Contour)
                continue;

            if (SegmentsIntersect(segments[i], segments[j]))
            {
                variable_segments[i] = 1;
                variable_segments[j] = 1;
                intersecting_contours[segments[i].m_Contour] = 1;
                intersecting_contours[segments[j].m_Contour] = 1;
                if (SegmentsAreCoincident(segments[i], segments[j]))
                {
                    segments[i].m_RequiresTwoSidedWinding = true;
                    segments[j].m_RequiresTwoSidedWinding = true;
                }
            }
        }
    }

    uint32_t variable_segment_count = 0;
    for (uint32_t i = 0; i < variable_segments.Size(); ++i)
        variable_segment_count += variable_segments[i] != 0;
    if (variable_segment_count != 0)
    {
        // At most 32 samples create at most 31 transitions between adjacent
        // samples. Reserve one extra slot per segment for a simple bound.
        boundary_transitions.SetCapacity(variable_segment_count * 32);
    }

    // A non-intersecting contour has the same visible/buried state everywhere,
    // even when it is nested inside another contour. Classify it once instead
    // of repeating a complete winding query for each of its segments.
    for (uint32_t contour = 0; contour < contour_count; ++contour)
    {
        if (intersecting_contours[contour])
            continue;

        uint32_t representative = UINT32_MAX;
        for (uint32_t i = 0; i < segments.Size(); ++i)
        {
            if (segments[i].m_Contour == contour)
            {
                representative = i;
                break;
            }
        }
        if (representative == UINT32_MAX)
            continue;

        bool visible = IsBoundaryPoint(segments, segments[representative], 0.5);
        if (!visible)
        {
            for (uint32_t i = representative; i < segments.Size() && segments[i].m_Contour == contour; ++i)
                segments[i].m_BoundaryType = FONT_SDF_BOUNDARY_BURIED;
        }
    }

    for (uint32_t i = 0; i < segments.Size(); ++i)
    {
        FontSDFSegment& segment = segments[i];
        if (!intersecting_contours[segment.m_Contour])
            continue;

        if (variable_segments[i])
        {
            // Cache the parameters at which a segment changes between exposed
            // and buried. Two samples per pixel locate those transitions; a
            // short bisection preserves their position accurately enough for
            // the later exact curve-distance calculation.
            float extent = fmaxf(segment.m_MaxX - segment.m_MinX,
                                 segment.m_MaxY - segment.m_MinY);
            uint32_t sample_count = (uint32_t)ceilf(extent * 2.0f);
            if (sample_count < 4) sample_count = 4;
            if (sample_count > 32) sample_count = 32;

            segment.m_BoundaryTransitionOffset = boundary_transitions.Size();
            double previous_t = 0.5 / sample_count;
            bool previous_visible = IsBoundaryPoint(segments, segment, previous_t);
            segment.m_BoundaryStartsVisible = previous_visible;
            for (uint32_t sample = 1; sample < sample_count; ++sample)
            {
                double t = ((double)sample + 0.5) / sample_count;
                bool visible = IsBoundaryPoint(segments, segment, t);
                if (visible != previous_visible)
                    boundary_transitions.Push(FindBoundaryTransition(segments, segment,
                                                                     previous_t, t, previous_visible));
                previous_t = t;
                previous_visible = visible;
            }
            segment.m_BoundaryTransitionCount = boundary_transitions.Size() -
                                                segment.m_BoundaryTransitionOffset;
            if (segment.m_BoundaryTransitionCount != 0)
                segment.m_BoundaryType = FONT_SDF_BOUNDARY_VARIABLE;
            else
                segment.m_BoundaryType = segment.m_BoundaryStartsVisible
                    ? FONT_SDF_BOUNDARY_VISIBLE
                    : FONT_SDF_BOUNDARY_BURIED;
        }
        else if (!IsBoundaryPoint(segments, segment, 0.5))
            segment.m_BoundaryType = FONT_SDF_BOUNDARY_BURIED;
    }
}

static float PointVariableLineDistanceSquared(FontOutlinePoint point,
                                              const FontSDFSegment& segment,
                                              const dmArray<float>& boundary_transitions)
{
    double closest_parameter;
    PointLineDistanceSquared(point, segment, &closest_parameter);
    double parameters[3] = { 0.0, closest_parameter, 1.0 };
    double best = FLT_MAX;
    for (uint32_t i = 0; i < 3; ++i)
    {
        if (!IsBoundaryPoint(segment, boundary_transitions, parameters[i]))
            continue;

        AccumulatePointDistanceSquared(point, segment, parameters[i], &best);
    }
    // A contour intersection becomes a vertex of the boolean-filled shape.
    // Even though visibility changes at that parameter, the point itself is
    // part of the boundary and can be the nearest point to the pixel.
    for (uint32_t i = 0; i < segment.m_BoundaryTransitionCount; ++i)
    {
        double t = boundary_transitions[segment.m_BoundaryTransitionOffset + i];
        AccumulatePointDistanceSquared(point, segment, t, &best);
    }
    return (float)best;
}

static float PointVariableCurveDistanceSquaredFromRoots(FontOutlinePoint point,
                                                        const FontSDFSegment& segment,
                                                        const double* roots,
                                                        uint32_t root_count,
                                                        const dmArray<float>& boundary_transitions)
{
    double parameters[7];
    parameters[0] = 0.0;
    memcpy(parameters + 1, roots, root_count * sizeof(double));
    parameters[root_count + 1] = 1.0;
    double best = FLT_MAX;
    for (uint32_t i = 0; i < root_count + 2; ++i)
    {
        if (!IsBoundaryPoint(segment, boundary_transitions, parameters[i]))
            continue;

        AccumulatePointDistanceSquared(point, segment, parameters[i], &best);
    }
    for (uint32_t i = 0; i < segment.m_BoundaryTransitionCount; ++i)
    {
        double t = boundary_transitions[segment.m_BoundaryTransitionOffset + i];
        AccumulatePointDistanceSquared(point, segment, t, &best);
    }
    return (float)best;
}

static float PointVariableCurveDistanceSquared(FontOutlinePoint point,
                                               const FontSDFSegment& segment,
                                               const double* coefficients,
                                               const dmArray<float>& boundary_transitions)
{
    double roots[5];
    uint32_t root_count = FindPolynomialRoots(coefficients,
                                               segment.m_Type == FONT_SDF_SEGMENT_QUADRATIC ? 3 : 5,
                                               roots);
    return PointVariableCurveDistanceSquaredFromRoots(point, segment, roots, root_count,
                                                      boundary_transitions);
}

#if defined(__clang__)
static void PointVariableCurveDistanceSquaredPair(const FontOutlinePoint points[2],
                                                  const FontSDFSegment& segment,
                                                  float distances_squared[2],
                                                  const dmArray<float>& boundary_transitions)
{
    double polynomial[2][6];
    BuildDistancePolynomial(points[0], segment, polynomial[0]);
    BuildDistancePolynomial(points[1], segment, polynomial[1]);
    double roots[2];
    uint32_t degree = segment.m_Type == FONT_SDF_SEGMENT_QUADRATIC ? 3 : 5;
    if (FindPolynomialRootPair(polynomial, degree, roots))
    {
        distances_squared[0] = PointVariableCurveDistanceSquaredFromRoots(points[0], segment, &roots[0], 1,
                                                                          boundary_transitions);
        distances_squared[1] = PointVariableCurveDistanceSquaredFromRoots(points[1], segment, &roots[1], 1,
                                                                          boundary_transitions);
    }
    else
    {
        distances_squared[0] = PointVariableCurveDistanceSquared(points[0], segment, polynomial[0],
                                                                 boundary_transitions);
        distances_squared[1] = PointVariableCurveDistanceSquared(points[1], segment, polynomial[1],
                                                                 boundary_transitions);
    }
}
#endif

static int CompareCrossings(const void* left, const void* right)
{
    // Left-to-right order lets the final pixel loop update winding in one pass.
    float left_x = ((const FontSDFCrossing*)left)->m_X;
    float right_x = ((const FontSDFCrossing*)right)->m_X;
    return left_x < right_x ? -1 : left_x > right_x ? 1 : 0;
}

static void GetSegmentPixelRange(const FontSDFSegment& segment, float maximum_distance,
                                 int32_t origin_x, uint32_t width, uint32_t* begin, uint32_t* end)
{
    // Limit exact distance work to columns within the SDF spread of the
    // segment's conservative x bounds.
    int32_t first = (int32_t)floorf(segment.m_MinX - maximum_distance - origin_x - 0.5f);
    int32_t last = (int32_t)ceilf(segment.m_MaxX + maximum_distance - origin_x - 0.5f) + 1;
    if (first < 0) first = 0;
    if (last < 0) last = 0;
    if (first > (int32_t)width) first = width;
    if (last > (int32_t)width) last = width;
    *begin = (uint32_t)first;
    *end = (uint32_t)last;
}

static void AccumulateSegmentDistances(const FontSDFSegment& segment, float sample_y,
                                       int32_t origin_x, uint32_t width, float maximum_distance,
                                       float* distances_squared,
                                       const dmArray<float>& boundary_transitions)
{
    // Segment-major traversal reuses coefficients and bounds across a row. The
    // AABB test removes pixels whose current nearest distance cannot improve.
    uint32_t begin;
    uint32_t end;
    GetSegmentPixelRange(segment, maximum_distance, origin_x, width, &begin, &end);
    if (segment.m_Type == FONT_SDF_SEGMENT_LINE)
    {
        for (uint32_t x = begin; x < end; ++x)
        {
            FontOutlinePoint point = { (float)origin_x + (float)x + 0.5f, sample_y };
            if (PointAABBDistanceSquared(point, segment) >= distances_squared[x])
                continue;

            float distance_squared = segment.m_BoundaryType == FONT_SDF_BOUNDARY_VARIABLE
                ? PointVariableLineDistanceSquared(point, segment, boundary_transitions)
                : PointLineDistanceSquared(point, segment);
            if (distance_squared < distances_squared[x])
                distances_squared[x] = distance_squared;
        }
        return;
    }

    if (segment.m_BoundaryType == FONT_SDF_BOUNDARY_VARIABLE)
    {
#if defined(__clang__)
        uint32_t pending_x = UINT32_MAX;
        FontOutlinePoint pending_point = {};
        for (uint32_t x = begin; x < end; ++x)
        {
            FontOutlinePoint point = { (float)origin_x + (float)x + 0.5f, sample_y };
            if (PointAABBDistanceSquared(point, segment) >= distances_squared[x])
                continue;

            if (pending_x == UINT32_MAX)
            {
                pending_x = x;
                pending_point = point;
                continue;
            }

            FontOutlinePoint points[2] = { pending_point, point };
            float pair_distances[2];
            PointVariableCurveDistanceSquaredPair(points, segment, pair_distances,
                                                  boundary_transitions);
            if (pair_distances[0] < distances_squared[pending_x])
                distances_squared[pending_x] = pair_distances[0];
            if (pair_distances[1] < distances_squared[x])
                distances_squared[x] = pair_distances[1];
            pending_x = UINT32_MAX;
        }
        if (pending_x != UINT32_MAX)
        {
            double coefficients[6];
            BuildDistancePolynomial(pending_point, segment, coefficients);
            float distance_squared = PointVariableCurveDistanceSquared(pending_point, segment, coefficients,
                                                                       boundary_transitions);
            if (distance_squared < distances_squared[pending_x])
                distances_squared[pending_x] = distance_squared;
        }
#else
        for (uint32_t x = begin; x < end; ++x)
        {
            FontOutlinePoint point = { (float)origin_x + (float)x + 0.5f, sample_y };
            if (PointAABBDistanceSquared(point, segment) >= distances_squared[x])
                continue;

            double coefficients[6];
            BuildDistancePolynomial(point, segment, coefficients);
            float distance_squared = PointVariableCurveDistanceSquared(point, segment, coefficients,
                                                                       boundary_transitions);
            if (distance_squared < distances_squared[x])
                distances_squared[x] = distance_squared;
        }
#endif
        return;
    }

#if defined(__clang__)
    // Batch adjacent surviving pixels so Clang can lower Double2 to NEON or
    // wasm SIMD. An unpaired tail pixel uses the same scalar calculation.
    uint32_t pending_x = UINT32_MAX;
    FontOutlinePoint pending_point = {};
    for (uint32_t x = begin; x < end; ++x)
    {
        FontOutlinePoint point = { (float)origin_x + (float)x + 0.5f, sample_y };
        if (PointAABBDistanceSquared(point, segment) >= distances_squared[x])
            continue;

        if (pending_x == UINT32_MAX)
        {
            pending_x = x;
            pending_point = point;
            continue;
        }

        FontOutlinePoint points[2] = { pending_point, point };
        float pair_distances[2];
        PointCurveDistanceSquaredPair(points, segment, pair_distances);
        if (pair_distances[0] < distances_squared[pending_x])
            distances_squared[pending_x] = pair_distances[0];
        if (pair_distances[1] < distances_squared[x])
            distances_squared[x] = pair_distances[1];
        pending_x = UINT32_MAX;
    }
    if (pending_x != UINT32_MAX)
    {
        double coefficients[6];
        BuildDistancePolynomial(pending_point, segment, coefficients);
        float distance_squared = PointCurveDistanceSquared(pending_point, segment, coefficients);
        if (distance_squared < distances_squared[pending_x])
            distances_squared[pending_x] = distance_squared;
    }
#else
    for (uint32_t x = begin; x < end; ++x)
    {
        FontOutlinePoint point = { (float)origin_x + (float)x + 0.5f, sample_y };
        if (PointAABBDistanceSquared(point, segment) >= distances_squared[x])
            continue;

        double coefficients[6];
        BuildDistancePolynomial(point, segment, coefficients);
        float distance_squared = PointCurveDistanceSquared(point, segment, coefficients);
        if (distance_squared < distances_squared[x])
            distances_squared[x] = distance_squared;
    }
#endif
}

static void GenerateDistanceField(const dmArray<FontSDFSegment>& segments,
                                  const dmArray<float>& boundary_transitions,
                                  int32_t origin_x, int32_t origin_y,
                                  uint32_t width, uint32_t height, float maximum_distance_squared,
                                  float distance_scale, uint8_t on_edge_value, uint8_t* bitmap)
{
    // Each row is generated in three phases:
    // 1. Collect segments close enough to affect distance and collect their
    //    exact scanline crossings for the winding calculation.
    // 2. Accumulate the nearest unsigned distance for every pixel in the row.
    // 3. Walk the sorted crossings while encoding distance as inside/outside.
    dmArray<FontSDFCrossing> crossings;
    dmArray<uint32_t> candidate_segments;
    dmArray<float> distances_squared;
    // A y-monotonic segment crosses a row at most once, and each segment can
    // enter the distance candidate list at most once. Reserve those known
    // upper bounds so neither row-local list grows during rasterization.
    crossings.SetCapacity(segments.Size());
    candidate_segments.SetCapacity(segments.Size());
    distances_squared.SetCapacity(width);
    distances_squared.SetSize(width);
    float maximum_distance = sqrtf(maximum_distance_squared);

    for (uint32_t y = 0; y < height; ++y)
    {
        // Build distance candidates and winding crossings in one segment scan.
        // The y bounds discard segments that cannot affect this row.
        float sample_y = (float)origin_y + (float)y + 0.5f;
        candidate_segments.SetSize(0);
        crossings.SetSize(0);
        int32_t winding = 0;
        for (uint32_t i = 0; i < segments.Size(); ++i)
        {
            const FontSDFSegment& segment = segments[i];
            if (sample_y >= segment.m_MinY - maximum_distance &&
                sample_y <= segment.m_MaxY + maximum_distance)
            {
                if (segment.m_BoundaryType != FONT_SDF_BOUNDARY_BURIED)
                    candidate_segments.Push(i);
            }
            if (segment.m_Direction == 0)
                continue;

            FontOutlinePoint end = GetSegmentEnd(segment);
            float min_y = segment.m_P0.m_Y < end.m_Y ? segment.m_P0.m_Y : end.m_Y;
            float max_y = segment.m_P0.m_Y > end.m_Y ? segment.m_P0.m_Y : end.m_Y;
            // Include the lower endpoint and exclude the upper endpoint. This
            // counts a shared contour vertex once instead of once per segment.
            if (sample_y < min_y || sample_y >= max_y)
                continue;

            FontSDFCrossing crossing = { FindCurveCrossingX(segment, sample_y), segment.m_Direction };
            crossings.Push(crossing);
            // This is the winding for a ray extending right from the current
            // pixel. Start with all crossings, then remove each one after the
            // pixel sweep passes its x coordinate.
            winding += segment.m_Direction;
        }
        qsort(crossings.Begin(), crossings.Size(), sizeof(FontSDFCrossing), CompareCrossings);
        for (uint32_t x = 0; x < width; ++x)
        {
            // Distances outside the spread all encode to the same saturated
            // value, so there is no need to search beyond this initial limit.
            distances_squared[x] = maximum_distance_squared;
        }
        for (uint32_t i = 0; i < candidate_segments.Size(); ++i)
        {
            const FontSDFSegment& segment = segments[candidate_segments[i]];
            AccumulateSegmentDistances(segment, sample_y, origin_x, width, maximum_distance,
                                       distances_squared.Begin(), boundary_transitions);
        }
        uint32_t crossing_index = 0;
        for (uint32_t x = 0; x < width; ++x)
        {
            FontOutlinePoint point = { (float)origin_x + (float)x + 0.5f, (float)origin_y + (float)y + 0.5f };
            while (crossing_index < crossings.Size() && crossings[crossing_index].m_X <= point.m_X)
                winding -= crossings[crossing_index++].m_Direction;
            float distance_squared = distances_squared[x];
            float distance = sqrtf(distance_squared) * distance_scale;
            // The edge maps to m_OnEdgeValue. Distance raises values inside
            // the non-zero winding fill and lowers values outside it.
            float value = winding != 0 ? on_edge_value + distance : on_edge_value - distance;
            if (value < 0.0f) value = 0.0f;
            if (value > 255.0f) value = 255.0f;
            bitmap[y * width + x] = (uint8_t)value;
        }
    }
}

FontResult FontSDFGenerate(const FontOutline* outline, const FontSDFParams* params,
                           FontGlyphBitmap* bitmap, int32_t* offset_x, int32_t* offset_y)
{
    memset(bitmap, 0, sizeof(*bitmap));
    *offset_x = 0;
    *offset_y = 0;
    if (params->m_Spread == 0)
        return FONT_RESULT_ERROR;

    dmArray<FontSDFSegment> segments;
    BuildSegments(outline, params->m_Scale, segments);
    if (segments.Empty())
        return FONT_RESULT_OK;

    dmArray<float> boundary_transitions;
    ClassifySegmentBoundaries(segments, boundary_transitions);

    float outline_min_x;
    float outline_min_y;
    float outline_max_x;
    float outline_max_y;
    if (!FontGetOutlineBounds(outline, &outline_min_x, &outline_min_y, &outline_max_x, &outline_max_y))
        return FONT_RESULT_OK;

    float min_x = outline_min_x * params->m_Scale;
    float max_x = outline_max_x * params->m_Scale;
    float min_y = -outline_max_y * params->m_Scale;
    float max_y = -outline_min_y * params->m_Scale;

    int32_t x0 = (int32_t)floorf(min_x) - params->m_Spread;
    int32_t y0 = (int32_t)floorf(min_y) - params->m_Spread;
    int32_t x1 = (int32_t)ceilf(max_x) + params->m_Spread;
    int32_t y1 = (int32_t)ceilf(max_y) + params->m_Spread;
    uint64_t width_64 = (int64_t)x1 - x0;
    uint64_t height_64 = (int64_t)y1 - y0;
    uint64_t pixel_count_64 = width_64 * height_64;
    if (width_64 > UINT32_MAX || height_64 > UINT32_MAX || pixel_count_64 > UINT32_MAX)
        return FONT_RESULT_ERROR;

    uint32_t width = (uint32_t)width_64;
    uint32_t height = (uint32_t)height_64;
    uint32_t pixel_count = (uint32_t)pixel_count_64;
    uint8_t* sdf = (uint8_t*)calloc(pixel_count, 1);
    if (!sdf)
        return FONT_RESULT_ERROR;

    float pixel_distance_scale = (float)params->m_OnEdgeValue / params->m_Spread;
    float maximum_distance_squared = (float)params->m_Spread * params->m_Spread;
    GenerateDistanceField(segments, boundary_transitions, x0, y0, width, height, maximum_distance_squared,
                          pixel_distance_scale, params->m_OnEdgeValue, sdf);

    bitmap->m_Data = sdf;
    bitmap->m_DataSize = pixel_count;
    bitmap->m_Width = width;
    bitmap->m_Height = height;
    bitmap->m_Channels = 1;
    bitmap->m_Flags = FONT_GLYPH_BM_FLAG_COMPRESSION_NONE;
    *offset_x = x0;
    *offset_y = y0;
    return FONT_RESULT_OK;
}

void FontSDFFree(FontGlyphBitmap* bitmap)
{
    free(bitmap->m_Data);
    memset(bitmap, 0, sizeof(*bitmap));
}
