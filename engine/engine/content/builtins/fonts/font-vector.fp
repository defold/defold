#version 140

// Based on Rook & Possum's Scanline Sweeper work:
// "The Scanline Sweeper: A Glyph Rendering Algorithm"
// https://rookandpossum.com/posts/scanline-sweeper
//
// Mozilla Public License 2.0:
// https://www.mozilla.org/en-GB/MPL/2.0/

precision highp float;
precision highp int;

in highp vec2 var_texcoord;
in mediump vec4 var_color;
in highp vec4 var_banding;
flat in highp vec4 var_jacobian;
flat in highp vec4 var_glyph;
flat in highp vec4 var_params;

out vec4 out_fragColor;

uniform highp sampler2D curve_texture;

uniform fs_uniforms
{
    highp vec4 scanline_debug;
    highp vec4 scanline_debug_filter;
};

const float CURVE_TEXTURE_WIDTH = 512.0;
const float CURVE_TEXTURE_HEIGHT = 64.0;
const int CURVE_TEXTURE_WIDTH_TEXELS = 512;
const int MAX_VECTOR_CURVES = 256;
const float LAYER_MODE_FACE = 0.0;
const float LAYER_MODE_OUTLINE = 1.0;
const float LAYER_MODE_SHADOW = 2.0;
#define SHADOW_USE_SEPARABLE_BOX_REFERENCE 0
#define SHADOW_USE_LEGACY_SDF_REFERENCE 1
#define SHADOW_LEGACY_SDF_BLUR_3X3 0

vec4 SampleCurveTexel(float texel_index)
{
    int texel = int(texel_index + 0.5);
    int texel_x = texel % CURVE_TEXTURE_WIDTH_TEXELS;
    int texel_y = texel / CURVE_TEXTURE_WIDTH_TEXELS;
    return texelFetch(curve_texture, ivec2(texel_x, texel_y), 0);
}

float QuadraticAxis(float t, float p0, float p1, float p2)
{
    float mt = 1.0 - t;
    return mt * mt * p0 + 2.0 * mt * t * p1 + t * t * p2;
}

vec2 evaluate_bezier(vec2 p0, vec2 p1, vec2 p2, float t)
{
    float mt = 1.0 - t;
    return mt * mt * p0 + 2.0 * mt * t * p1 + t * t * p2;
}

vec2 QuadraticTangent(float t, vec2 p0, vec2 p1, vec2 p2)
{
    return 2.0 * mix(p1 - p0, p2 - p1, t);
}

vec2 QuadraticSecondDerivative(vec2 p0, vec2 p1, vec2 p2)
{
    return 2.0 * (p2 - 2.0 * p1 + p0);
}

vec2 CalculateQuadraticRoots(float a, float b, float c)
{
    const float epsilon = 0.00001;
    vec2 roots = vec2(-99999.0);

    if (abs(a) < epsilon)
    {
        if (abs(b) > epsilon)
        {
            roots.x = -c / b;
        }
        return roots;
    }

    float discriminant = b * b - 4.0 * a * c;
    if (discriminant > -epsilon)
    {
        float s = sqrt(max(0.0, discriminant));
        roots.x = (-b + s) / (2.0 * a);
        roots.y = (-b - s) / (2.0 * a);
    }

    return roots;
}

float intersect_monotonic(float qa, float c0, float c1, float c2, float target)
{
    // Match the Scanline Sweeper reference cutoff. With the smaller epsilon,
    // nearly-linear font edges can take the quadratic branch in float32 and
    // collapse both scanline intersections to the same root.
    const float epsilon = 0.001;

    if (abs(qa) < epsilon)
    {
        float denom = c2 - c0;
        return abs(denom) < 0.000001 ? 0.0 : (target - c0) / denom;
    }

    float qb = 2.0 * (c1 - c0);
    float qc = c0 - target;
    float discriminant = max(qb * qb - 4.0 * qa * qc, 0.0);
    float direction = c2 >= c0 ? 1.0 : -1.0;
    return (-qb + direction * sqrt(discriminant)) * (0.5 / qa);
}

float scanline_sweep_area_to_right(float t0, float t1, float right, vec2 p0, vec2 p1, vec2 p2)
{
    float ax = p0.x - 2.0 * p1.x + p2.x;
    float bx = 2.0 * (p1.x - p0.x);
    float cx = p0.x;
    float ay = p0.y - 2.0 * p1.y + p2.y;
    float by = 2.0 * (p1.y - p0.y);

    float y_delta = QuadraticAxis(t1, p0.y, p1.y, p2.y) - QuadraticAxis(t0, p0.y, p1.y, p2.y);
    float t02 = t0 * t0;
    float t03 = t02 * t0;
    float t04 = t02 * t02;
    float t12 = t1 * t1;
    float t13 = t12 * t1;
    float t14 = t12 * t12;

    float xy0 = 0.5 * ax * ay * t04
              + (ax * by + 2.0 * bx * ay) * t03 / 3.0
              + 0.5 * (bx * by + 2.0 * cx * ay) * t02
              + cx * by * t0;
    float xy1 = 0.5 * ax * ay * t14
              + (ax * by + 2.0 * bx * ay) * t13 / 3.0
              + 0.5 * (bx * by + 2.0 * cx * ay) * t12
              + cx * by * t1;

    return right * y_delta - (xy1 - xy0);
}

float scanline_sweep(vec2 size, vec2 offset, vec2 p0, vec2 p1, vec2 p2)
{
    if (max(p0.y, p2.y) <= offset.y || min(p0.y, p2.y) >= offset.y + size.y)
    {
        return 0.0;
    }

    vec2 delta = p2 - p0;
    if (abs(delta.y) < 0.000001)
    {
        return 0.0;
    }

    p0 -= offset;
    p1 -= offset;
    p2 -= offset;

    const float segment_epsilon = 0.000001;
    if (abs(p0.x - p1.x) < segment_epsilon && abs(p0.x - p2.x) < segment_epsilon)
    {
        if (p0.x >= size.x)
        {
            return 0.0;
        }

        float top = min(max(p0.y, p2.y), size.y);
        float bottom = max(min(p0.y, p2.y), 0.0);
        float h = top - bottom;
        float b = min(size.x, size.x - p0.x);
        return sign(delta.y) * b * h;
    }

    float qa = p0.y + p2.y - 2.0 * p1.y;
    float bt = intersect_monotonic(qa, p0.y, p1.y, p2.y, 0.0);
    float tt = intersect_monotonic(qa, p0.y, p1.y, p2.y, size.y);
    float v_min_t = delta.y > 0.0 ? bt : tt;
    float v_max_t = delta.y > 0.0 ? tt : bt;

    vec2 v_min = evaluate_bezier(p0, p1, p2, clamp(v_min_t, 0.0, 1.0));
    vec2 v_max = evaluate_bezier(p0, p1, p2, clamp(v_max_t, 0.0, 1.0));

    if (max(v_min.x, v_max.x) <= 0.0)
    {
        return (v_max.y - v_min.y) * size.x;
    }

    if (min(v_min.x, v_max.x) >= size.x)
    {
        return 0.0;
    }

    qa = p0.x + p2.x - 2.0 * p1.x;
    vec4 h_check = delta.x > 0.0
        ? vec4(p0.x, p2.x, 0.0, 0.0)
        : vec4(p2.x, p0.x, size.x, 1.0);

    float h_min_t;
    if (h_check.x >= h_check.z)
    {
        h_min_t = h_check.w;
    }
    else if (h_check.y <= h_check.z)
    {
        h_min_t = 1.0 - h_check.w;
    }
    else
    {
        h_min_t = intersect_monotonic(qa, p0.x, p1.x, p2.x, h_check.z);
    }

    h_check.z = size.x - h_check.z;

    float h_max_t;
    if (h_check.x >= h_check.z)
    {
        h_max_t = h_check.w;
    }
    else if (h_check.y <= h_check.z)
    {
        h_max_t = 1.0 - h_check.w;
    }
    else
    {
        h_max_t = intersect_monotonic(qa, p0.x, p1.x, p2.x, h_check.z);
    }

    float min_t = clamp(max(v_min_t, h_min_t), 0.0, 1.0);
    float max_t = clamp(min(v_max_t, h_max_t), 0.0, 1.0);

    vec2 q0 = v_min_t >= h_min_t ? v_min : evaluate_bezier(p0, p1, p2, min_t);
    vec2 q1 = v_max_t <= h_max_t ? v_max : evaluate_bezier(p0, p1, p2, max_t);

    float coverage = 0.0;
    if (min_t > 0.0 && delta.x > 0.0)
    {
        float h = delta.y > 0.0
            ? q0.y - max(0.0, p0.y)
            : min(size.y, p0.y) - q0.y;
        coverage = sign(delta.y) * h * size.x;
    }

    if (max_t < 1.0 && delta.x < 0.0)
    {
        float h = delta.y > 0.0
            ? min(size.y, p2.y) - q1.y
            : q1.y - max(0.0, p2.y);
        coverage += sign(delta.y) * h * size.x;
    }

    coverage += scanline_sweep_area_to_right(min_t, max_t, size.x, p0, p1, p2);
    return coverage;
}

float EncodeDebugUnit(float value)
{
    return clamp(value, 0.0, 1.0);
}

float EncodeDebugSigned(float value)
{
    return clamp(0.5 + value * 0.25, 0.0, 1.0);
}

vec4 EncodeDebugUnit4(vec4 value)
{
    return clamp(value, vec4(0.0), vec4(1.0));
}

vec4 EncodeDebugSigned4(vec4 value)
{
    return clamp(vec4(0.5) + value * 0.25, vec4(0.0), vec4(1.0));
}

vec4 ScanlineSweepDebugPageForCurve(vec2 render_coord,
                                    vec2 filter_width,
                                    int page,
                                    vec2 p0,
                                    vec2 p1,
                                    vec2 p2)
{
    vec2 inv_filter_width = 1.0 / max(filter_width, vec2(1.0 / 65536.0));
    vec2 size = vec2(1.0);
    vec2 offset = render_coord * inv_filter_width - size * 0.5;

    p0 *= inv_filter_width;
    p1 *= inv_filter_width;
    p2 *= inv_filter_width;

    if (max(p0.y, p2.y) <= offset.y || min(p0.y, p2.y) >= offset.y + size.y)
    {
        return vec4(0.0, 0.0, 0.0, 1.0);
    }

    vec2 delta = p2 - p0;
    p0 -= offset;
    p1 -= offset;
    p2 -= offset;

    const float segment_epsilon = 0.000001;
    if (abs(p0.x - p1.x) < segment_epsilon && abs(p0.x - p2.x) < segment_epsilon)
    {
        float top = min(max(p0.y, p2.y), size.y);
        float bottom = max(min(p0.y, p2.y), 0.0);
        float h = top - bottom;
        float b = p0.x >= size.x ? 0.0 : min(size.x, size.x - p0.x);
        float coverage = sign(delta.y) * b * h;
        if (page == 4)
        {
            return EncodeDebugSigned4(vec4(0.0, 0.0, coverage, coverage));
        }
        return vec4(0.0, EncodeDebugSigned(coverage), 0.0, 1.0);
    }

    float qa_y = p0.y + p2.y - 2.0 * p1.y;
    float bt = intersect_monotonic(qa_y, p0.y, p1.y, p2.y, 0.0);
    float tt = intersect_monotonic(qa_y, p0.y, p1.y, p2.y, size.y);
    float v_min_t = delta.y > 0.0 ? bt : tt;
    float v_max_t = delta.y > 0.0 ? tt : bt;

    vec2 v_min = evaluate_bezier(p0, p1, p2, clamp(v_min_t, 0.0, 1.0));
    vec2 v_max = evaluate_bezier(p0, p1, p2, clamp(v_max_t, 0.0, 1.0));

    if (page == 1)
    {
        return EncodeDebugUnit4(vec4(bt, tt, v_min_t, v_max_t));
    }

    if (max(v_min.x, v_max.x) <= 0.0)
    {
        float coverage = (v_max.y - v_min.y) * size.x;
        return page == 4 ? EncodeDebugSigned4(vec4(0.0, 0.0, coverage, coverage)) : vec4(0.0, 0.0, 0.0, 1.0);
    }

    if (min(v_min.x, v_max.x) >= size.x)
    {
        return page == 4 ? EncodeDebugSigned4(vec4(0.0)) : vec4(0.0, 0.0, 0.0, 1.0);
    }

    float qa_x = p0.x + p2.x - 2.0 * p1.x;
    vec4 h_check = delta.x > 0.0
        ? vec4(p0.x, p2.x, 0.0, 0.0)
        : vec4(p2.x, p0.x, size.x, 1.0);

    float h_min_t;
    if (h_check.x >= h_check.z)
    {
        h_min_t = h_check.w;
    }
    else if (h_check.y <= h_check.z)
    {
        h_min_t = 1.0 - h_check.w;
    }
    else
    {
        h_min_t = intersect_monotonic(qa_x, p0.x, p1.x, p2.x, h_check.z);
    }

    h_check.z = size.x - h_check.z;

    float h_max_t;
    if (h_check.x >= h_check.z)
    {
        h_max_t = h_check.w;
    }
    else if (h_check.y <= h_check.z)
    {
        h_max_t = 1.0 - h_check.w;
    }
    else
    {
        h_max_t = intersect_monotonic(qa_x, p0.x, p1.x, p2.x, h_check.z);
    }

    float min_t = clamp(max(v_min_t, h_min_t), 0.0, 1.0);
    float max_t = clamp(min(v_max_t, h_max_t), 0.0, 1.0);

    if (page == 2)
    {
        return EncodeDebugUnit4(vec4(h_min_t, h_max_t, min_t, max_t));
    }

    vec2 q0 = v_min_t >= h_min_t ? v_min : evaluate_bezier(p0, p1, p2, min_t);
    vec2 q1 = v_max_t <= h_max_t ? v_max : evaluate_bezier(p0, p1, p2, max_t);

    if (page == 3)
    {
        return EncodeDebugUnit4(vec4(q0.x, q0.y, q1.x, q1.y));
    }

    float pre_coverage = 0.0;
    if (min_t > 0.0 && delta.x > 0.0)
    {
        float h = delta.y > 0.0
            ? q0.y - max(0.0, p0.y)
            : min(size.y, p0.y) - q0.y;
        pre_coverage = sign(delta.y) * h * size.x;
    }

    float post_coverage = 0.0;
    if (max_t < 1.0 && delta.x < 0.0)
    {
        float h = delta.y > 0.0
            ? min(size.y, p2.y) - q1.y
            : q1.y - max(0.0, p2.y);
        post_coverage = sign(delta.y) * h * size.x;
    }

    float body_coverage = scanline_sweep_area_to_right(min_t, max_t, size.x, p0, p1, p2);
    float coverage = pre_coverage + post_coverage + body_coverage;

    if (page == 4)
    {
        return EncodeDebugSigned4(vec4(pre_coverage, post_coverage, body_coverage, coverage));
    }

    return vec4(EncodeDebugUnit(filter_width.x * 512.0),
                EncodeDebugUnit(filter_width.y * 512.0),
                EncodeDebugUnit(abs(delta.x)),
                EncodeDebugUnit(abs(delta.y)));
}

vec4 ScanlineSweepDebugPage(vec2 render_coord,
                            float curve_start,
                            float curve_count,
                            vec2 filter_width,
                            int curve_index,
                            int page)
{
    if (curve_index < 0 || float(curve_index) >= curve_count)
    {
        return vec4(1.0, 0.0, 1.0, 1.0);
    }

    float curve_texel = curve_start + float(curve_index * 2);
    vec4 curve_a = SampleCurveTexel(curve_texel);
    vec4 curve_b = SampleCurveTexel(curve_texel + 1.0);
    return ScanlineSweepDebugPageForCurve(render_coord, filter_width, page, curve_a.xy, curve_a.zw, curve_b.xy);
}

float ScanlineSweepRender(vec2 render_coord, float curve_start, float curve_count, vec2 filter_width)
{
    vec2 inv_filter_width = 1.0 / max(filter_width, vec2(1.0 / 65536.0));
    vec2 size = vec2(1.0);
    vec2 offset = render_coord * inv_filter_width - size * 0.5;
    float signed_area = 0.0;

    for (int i = 0; i < MAX_VECTOR_CURVES; ++i)
    {
        if (float(i) >= curve_count)
        {
            break;
        }

        float curve_texel = curve_start + float(i * 2);
        vec4 curve_a = SampleCurveTexel(curve_texel);
        vec4 curve_b = SampleCurveTexel(curve_texel + 1.0);
        signed_area += scanline_sweep(size,
                                      offset,
                                      curve_a.xy * inv_filter_width,
                                      curve_a.zw * inv_filter_width,
                                      curve_b.xy * inv_filter_width);
    }

    return clamp(abs(signed_area), 0.0, 1.0);
}

float ComputeQuadraticWinding(vec2 p, vec2 p0, vec2 p1, vec2 p2)
{
    const float x_epsilon = 0.0001;
    const float root_epsilon = 0.0001;
    const float tangent_epsilon = 0.0001;
    float a = p0.y - 2.0 * p1.y + p2.y;
    float b = -2.0 * p0.y + 2.0 * p1.y;
    float c = p0.y - p.y;
    float winding = 0.0;
    vec2 roots = CalculateQuadraticRoots(a, b, c);

    if ((roots.x >= -root_epsilon) && (roots.x <= 1.0 + root_epsilon))
    {
        float t0 = clamp(roots.x, 0.0, 1.0);
        float dy0 = 2.0 * a * t0 + b;
        bool eligible0 = false;
        if (dy0 > tangent_epsilon)
        {
            eligible0 = roots.x >= -root_epsilon && roots.x < 1.0 - root_epsilon;
        }
        else if (dy0 < -tangent_epsilon)
        {
            eligible0 = roots.x > root_epsilon && roots.x <= 1.0 + root_epsilon;
        }

        if (eligible0)
        {
            float x0 = QuadraticAxis(t0, p0.x, p1.x, p2.x);
            if (x0 > (p.x + x_epsilon))
            {
                winding += sign(dy0);
            }
        }
    }

    if ((roots.y >= -root_epsilon) && (roots.y <= 1.0 + root_epsilon) && (abs(roots.y - roots.x) > root_epsilon))
    {
        float t1 = clamp(roots.y, 0.0, 1.0);
        float dy1 = 2.0 * a * t1 + b;
        bool eligible1 = false;
        if (dy1 > tangent_epsilon)
        {
            eligible1 = roots.y >= -root_epsilon && roots.y < 1.0 - root_epsilon;
        }
        else if (dy1 < -tangent_epsilon)
        {
            eligible1 = roots.y > root_epsilon && roots.y <= 1.0 + root_epsilon;
        }

        if (eligible1)
        {
            float x1 = QuadraticAxis(t1, p0.x, p1.x, p2.x);
            if (x1 > (p.x + x_epsilon))
            {
                winding += sign(dy1);
            }
        }
    }

    return winding;
}

float ComputeGlyphWinding(vec2 p, float curve_start, float curve_count)
{
    float winding = 0.0;
    for (int i = 0; i < MAX_VECTOR_CURVES; ++i)
    {
        if (float(i) >= curve_count)
        {
            break;
        }

        float curve_texel = curve_start + float(i * 2);
        vec4 curve_a = SampleCurveTexel(curve_texel);
        vec4 curve_b = SampleCurveTexel(curve_texel + 1.0);
        winding += ComputeQuadraticWinding(p, curve_a.xy, curve_a.zw, curve_b.xy);
    }

    return winding;
}

float IsInsideGlyph(vec2 p, float curve_start, float curve_count)
{
    return step(0.5, abs(ComputeGlyphWinding(p, curve_start, curve_count)));
}

float CalculateHorizontalCoverage(vec2 pixel_pos, float curve_start, float curve_count, float pixel_size_x)
{
    const float epsilon = 0.0001;
    float coverage = 0.0;
    float inv_pixel_size = 1.0 / max(pixel_size_x, 0.0001);

    for (int i = 0; i < MAX_VECTOR_CURVES; ++i)
    {
        if (float(i) >= curve_count)
        {
            break;
        }

        float curve_texel = curve_start + float(i * 2);
        vec4 curve_a = SampleCurveTexel(curve_texel);
        vec4 curve_b = SampleCurveTexel(curve_texel + 1.0);

        vec2 p0 = curve_a.xy - pixel_pos;
        vec2 p1 = curve_a.zw - pixel_pos;
        vec2 p2 = curve_b.xy - pixel_pos;

        bool is_downward_curve = p0.y > 0.0 || p2.y < 0.0;
        if (is_downward_curve)
        {
            if (p0.y < 0.0 && p2.y <= 0.0) continue;
            if (p0.y > 0.0 && p2.y >= 0.0) continue;
        }
        else
        {
            if (p0.y <= 0.0 && p2.y < 0.0) continue;
            if (p0.y >= 0.0 && p2.y > 0.0) continue;
        }

        vec2 a = p0 - 2.0 * p1 + p2;
        vec2 b = 2.0 * (p1 - p0);
        vec2 c = p0;

        vec2 roots = CalculateQuadraticRoots(a.y, b.y, c.y);

        float t0 = clamp(roots.x, 0.0, 1.0);
        float t1 = clamp(roots.y, 0.0, 1.0);
        bool on_seg0 = roots.x >= -epsilon && roots.x <= 1.0 + epsilon;
        bool on_seg1 = roots.y >= -epsilon && roots.y <= 1.0 + epsilon && abs(roots.y - roots.x) > epsilon;

        float intersect0 = a.x * t0 * t0 + b.x * t0 + c.x;
        float intersect1 = a.x * t1 * t1 + b.x * t1 + c.x;

        float sign = is_downward_curve ? 1.0 : -1.0;
        if (on_seg0)
        {
            coverage += clamp(0.5 + intersect0 * inv_pixel_size, 0.0, 1.0) * sign;
        }
        if (on_seg1)
        {
            coverage += clamp(0.5 + intersect1 * inv_pixel_size, 0.0, 1.0) * sign;
        }
    }

    return clamp(coverage, 0.0, 1.0);
}

float ComputeGlyphCoverage(vec2 p, float curve_start, float curve_count, vec2 glyph_scale)
{
    float pixel_size_x = 1.0 / max(glyph_scale.x, 0.0001);
    float pixel_size_y = 1.0 / max(glyph_scale.y, 0.0001);
    float alpha_sum = 0.0;

    for (int y_offset = -1; y_offset <= 1; ++y_offset)
    {
        vec2 sample_pos = p + vec2(0.0, float(y_offset) * pixel_size_y / 3.0);
        alpha_sum += CalculateHorizontalCoverage(sample_pos, curve_start, curve_count, pixel_size_x);
    }

    return alpha_sum / 3.0;
}

float QuadraticDistanceSqPixels(vec2 p, vec2 p0, vec2 p1, vec2 p2, vec2 glyph_scale)
{
    float best_distance_sq = 1e20;
    vec2 dd = QuadraticSecondDerivative(p0, p1, p2) * glyph_scale;

    for (int seed_index = 0; seed_index < 5; ++seed_index)
    {
        float t = float(seed_index) * 0.25;

        for (int iter = 0; iter < 6; ++iter)
        {
            vec2 delta = (evaluate_bezier(p0, p1, p2, t) - p) * glyph_scale;
            vec2 tangent = QuadraticTangent(t, p0, p1, p2) * glyph_scale;

            float f = dot(delta, tangent);
            float df = dot(tangent, tangent) + dot(delta, dd);
            if (abs(df) < 0.000001)
            {
                break;
            }

            t = clamp(t - f / df, 0.0, 1.0);
        }

        vec2 curve_p = evaluate_bezier(p0, p1, p2, t);
        vec2 delta = (curve_p - p) * glyph_scale;
        best_distance_sq = min(best_distance_sq, dot(delta, delta));
    }

    vec2 delta0 = (p0 - p) * glyph_scale;
    vec2 delta1 = (p2 - p) * glyph_scale;
    best_distance_sq = min(best_distance_sq, dot(delta0, delta0));
    best_distance_sq = min(best_distance_sq, dot(delta1, delta1));

    return best_distance_sq;
}

float ComputeCurveDistanceSqPixels(vec2 p, float curve_start, float curve_count, vec2 glyph_scale)
{
    float best_distance_sq = 1e20;
    for (int i = 0; i < MAX_VECTOR_CURVES; ++i)
    {
        if (float(i) >= curve_count)
        {
            break;
        }

        float curve_texel = curve_start + float(i * 2);
        vec4 curve_a = SampleCurveTexel(curve_texel);
        vec4 curve_b = SampleCurveTexel(curve_texel + 1.0);
        best_distance_sq = min(best_distance_sq,
            QuadraticDistanceSqPixels(p, curve_a.xy, curve_a.zw, curve_b.xy, glyph_scale));
    }
    return best_distance_sq;
}

float ComputeStableGlyphCoverage(vec2 p,
                                 float curve_start,
                                 float curve_count,
                                 vec2 pixel_filter_width,
                                 vec2 glyph_screen_scale)
{
    float coverage = ScanlineSweepRender(p, curve_start, curve_count, pixel_filter_width);
    float edge_distance = sqrt(ComputeCurveDistanceSqPixels(p, curve_start, curve_count, glyph_screen_scale));
    if (edge_distance > 1.0)
    {
        coverage = IsInsideGlyph(p, curve_start, curve_count);
    }
    return coverage;
}

float EvaluateOutlineSilhouetteAlpha(float curve_distance, float outline_width)
{
    if (outline_width <= 0.0)
    {
        return 0.0;
    }

    // Keep the outline contribution stable at the contour edge so the shadow can
    // include the styled glyph silhouette, not just the filled face.
    float aa_width = 0.5;
    return 1.0 - smoothstep(max(0.0, outline_width - aa_width), outline_width + aa_width, curve_distance);
}

float EvaluateShadowSilhouetteAlpha(float face_coverage, float curve_distance, float outline_width)
{
    float face_alpha = face_coverage;
    float outline_alpha = EvaluateOutlineSilhouetteAlpha(curve_distance, outline_width);
    return max(face_alpha, outline_alpha);
}

float ShadowProfile(float shadow_blur)
{
    return smoothstep(1.0, 13.0, shadow_blur);
}

float ShadowLargeProfile(float shadow_blur)
{
    return smoothstep(7.0, 13.0, shadow_blur);
}

int ShadowSampleCount(float shadow_blur)
{
    if (shadow_blur <= 1.5)
    {
        return 2;
    }
    if (shadow_blur <= 3.5)
    {
        return 3;
    }
    if (shadow_blur <= 6.5)
    {
        return 5;
    }
    if (shadow_blur <= 10.5)
    {
        return 6;
    }
    return 8;
}

float ShadowFilterRadiusScale(float sample_t, float profile)
{
    float max_scale = mix(1.90, 3.05, profile);
    float radius_curve = mix(1.34, 1.08, profile);
    return max_scale * pow(sample_t, radius_curve);
}

float ShadowFilterWeight(float sample_t, float large_profile)
{
    float small_blur_weight = 1.25 - 0.45 * sample_t;
    float large_blur_weight = 1.05 - 0.30 * sample_t;
    return mix(small_blur_weight, large_blur_weight, large_profile * large_profile);
}

float ShadowCutoffAlpha(float alpha, float profile, float large_profile)
{
    float cutoff = mix(mix(0.018, 0.070, profile), 0.026, large_profile);
    float shoulder = mix(mix(0.020, 0.052, profile), 0.090, large_profile);
    float trimmed_alpha = clamp((alpha - cutoff) / max(1.0 - cutoff, 0.0001), 0.0, 1.0);
    return trimmed_alpha * smoothstep(cutoff, cutoff + shoulder, alpha);
}

float ShadowResponseAlpha(float alpha, float profile, float large_profile)
{
    float density = mix(2.20, 5.80, profile);
    float response_alpha = ShadowCutoffAlpha(alpha, profile, large_profile);
    float boosted_alpha = clamp(response_alpha * (mix(1.02, 1.58, profile) + 0.24 * large_profile), 0.0, 1.0);
    float shaped_alpha = 1.0 - pow(max(1.0 - boosted_alpha, 0.0), density);
    float large_blur_gain = 1.0 + 0.35 * profile * profile + 0.35 * large_profile * large_profile;
    return clamp(shaped_alpha * large_blur_gain, 0.0, 1.0);
}

float ShadowFeatherAlpha(float feather_distance, float shadow_blur, float profile)
{
    float feather_radius = max(shadow_blur * mix(1.38, 0.98, profile), 0.0001);
    float feather_strength = mix(0.50, 0.58, profile) * (1.0 - exp(-0.65 * shadow_blur));
    return (1.0 - smoothstep(0.0, feather_radius, feather_distance)) * feather_strength;
}

float ShadowShiftWeight(float axis_offset, float large_profile)
{
    float side_weight = mix(0.14, 0.22, large_profile);
    return abs(axis_offset) < 0.5 ? 1.0 - 2.0 * side_weight : side_weight;
}

float ShadowShiftedCorrectionAlpha(vec2 p,
                                   float curve_start,
                                   float curve_count,
                                   vec2 glyph_metric_scale,
                                   vec2 pixel_filter_width,
                                   float box_radius,
                                   float profile,
                                   float large_profile)
{
    vec2 box_radius_uv = vec2(box_radius) / glyph_metric_scale;
    vec2 shift_radius_uv = box_radius_uv * mix(mix(0.30, 0.38, profile), 0.58, large_profile);
    vec2 sample_filter = pixel_filter_width + 2.0 * box_radius_uv * mix(mix(0.30, 0.36, profile), 0.52, large_profile);

    float shifted_alpha = 0.0;
    float total_weight = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            float fx = float(x);
            float fy = float(y);
            float sample_weight = ShadowShiftWeight(fx, large_profile) * ShadowShiftWeight(fy, large_profile);
            vec2 sample_offset = vec2(fx, fy) * shift_radius_uv;
            shifted_alpha += ScanlineSweepRender(p + sample_offset, curve_start, curve_count, sample_filter) * sample_weight;
            total_weight += sample_weight;
        }
    }

    float response_alpha = ShadowResponseAlpha(shifted_alpha / max(total_weight, 0.0001), profile, large_profile);
    float cutoff = mix(0.075, 0.025, large_profile);
    float shoulder = mix(0.120, 0.090, large_profile);
    return response_alpha * smoothstep(cutoff, cutoff + shoulder, response_alpha);
}

int ShadowSeparableBoxSampleCount(float shadow_blur)
{
    if (shadow_blur <= 1.5)
    {
        return 3;
    }
    if (shadow_blur <= 3.5)
    {
        return 7;
    }
    if (shadow_blur <= 6.5)
    {
        return 13;
    }
    if (shadow_blur <= 10.5)
    {
        return 21;
    }
    return 27;
}

float EvaluateSeparableBoxShadowAlpha(vec2 p,
                                      float curve_start,
                                      float curve_count,
                                      vec2 glyph_metric_scale,
                                      vec2 pixel_filter_width,
                                      float outline_width,
                                      float shadow_blur)
{
    float box_radius = shadow_blur + outline_width;
    vec2 box_radius_uv = vec2(box_radius) / glyph_metric_scale;
    vec2 horizontal_filter = vec2(pixel_filter_width.x + 2.0 * box_radius_uv.x,
                                  pixel_filter_width.y);

    float filtered_alpha = 0.0;
    float total_weight = 0.0;
    int sample_count = ShadowSeparableBoxSampleCount(shadow_blur);
    for (int i = 0; i < 27; ++i)
    {
        if (i >= sample_count)
        {
            continue;
        }

        float sample_t = (float(i) + 0.5) / float(sample_count);
        float y_offset = sample_t * 2.0 - 1.0;
        filtered_alpha += ScanlineSweepRender(p + vec2(0.0, y_offset) * box_radius_uv,
                                              curve_start,
                                              curve_count,
                                              horizontal_filter);
        total_weight += 1.0;
    }

    return filtered_alpha / max(total_weight, 0.0001);
}

float LegacySdfShadowSpread(float shadow_blur)
{
    return max(1.4142 + shadow_blur, 0.0001);
}

float LegacySdfShadowEdge(float shadow_blur)
{
    const float sdf_edge = 0.75;
    const float sdf_range = 1.0 - sdf_edge;
    return sdf_edge - sdf_range * shadow_blur / LegacySdfShadowSpread(shadow_blur);
}

float LegacySdfShadowValue(vec2 p,
                           float curve_start,
                           float curve_count,
                           vec2 glyph_metric_scale,
                           float outline_width,
                           float shadow_blur)
{
    const float sdf_edge = 0.75;
    const float sdf_range = 1.0 - sdf_edge;
    float curve_distance = sqrt(ComputeCurveDistanceSqPixels(p, curve_start, curve_count, glyph_metric_scale));
    float inside = IsInsideGlyph(p, curve_start, curve_count);
    float signed_distance = mix(-curve_distance, curve_distance, inside);
    float distance_to_shadow_body = signed_distance + outline_width;

    // Match Fontc.java's shadow-body fill: once the pixel belongs to the
    // face/outline body, the stored shadow channel is kept near the SDF edge
    // instead of growing with distance into the glyph.
    if (distance_to_shadow_body > 0.0)
    {
        distance_to_shadow_body = sdf_edge;
    }

    return clamp(sdf_edge + sdf_range * distance_to_shadow_body / LegacySdfShadowSpread(shadow_blur), 0.0, 1.0);
}

float LegacySdfShadowValue3x3(vec2 p,
                              float curve_start,
                              float curve_count,
                              vec2 glyph_metric_scale,
                              float outline_width,
                              float shadow_blur)
{
    vec2 texel_uv = 1.0 / glyph_metric_scale;
    float total = 0.0;
    for (int y = -1; y <= 1; ++y)
    {
        for (int x = -1; x <= 1; ++x)
        {
            float wx = x == 0 ? 2.0 : 1.0;
            float wy = y == 0 ? 2.0 : 1.0;
            float weight = wx * wy;
            total += LegacySdfShadowValue(p + vec2(float(x), float(y)) * texel_uv,
                                          curve_start,
                                          curve_count,
                                          glyph_metric_scale,
                                          outline_width,
                                          shadow_blur) * weight;
        }
    }
    return total * (1.0 / 16.0);
}

float EvaluateLegacySdfShadowAlpha(vec2 p,
                                   float curve_start,
                                   float curve_count,
                                   vec2 glyph_metric_scale,
                                   float outline_width,
                                   float shadow_blur)
{
    const float sdf_edge = 0.75;
    const float sdf_range = 1.0 - sdf_edge;
    float spread = LegacySdfShadowSpread(shadow_blur);
    float sdf_smoothing = sdf_range / spread;
#if SHADOW_LEGACY_SDF_BLUR_3X3
    float shadow_value = LegacySdfShadowValue3x3(p,
                                                curve_start,
                                                curve_count,
                                                glyph_metric_scale,
                                                outline_width,
                                                shadow_blur);
#else
    float shadow_value = LegacySdfShadowValue(p,
                                              curve_start,
                                              curve_count,
                                              glyph_metric_scale,
                                              outline_width,
                                              shadow_blur);
#endif
    return smoothstep(LegacySdfShadowEdge(shadow_blur) - sdf_smoothing,
                      sdf_edge + sdf_smoothing,
                      shadow_value);
}

float EvaluateFilteredShadowAlpha(vec2 p,
                                  float curve_start,
                                  float curve_count,
                                  vec2 glyph_metric_scale,
                                  vec2 pixel_filter_width,
                                  float face_coverage,
                                  float outline_width,
                                  float shadow_blur)
{
    if (shadow_blur <= 0.0)
    {
        float curve_distance = sqrt(ComputeCurveDistanceSqPixels(p, curve_start, curve_count, glyph_metric_scale));
        float center_alpha = EvaluateShadowSilhouetteAlpha(face_coverage, curve_distance, outline_width);
        return center_alpha;
    }

#if SHADOW_USE_LEGACY_SDF_REFERENCE
    return EvaluateLegacySdfShadowAlpha(p,
                                        curve_start,
                                        curve_count,
                                        glyph_metric_scale,
                                        outline_width,
                                        shadow_blur);
#elif SHADOW_USE_SEPARABLE_BOX_REFERENCE
    return EvaluateSeparableBoxShadowAlpha(p,
                                           curve_start,
                                           curve_count,
                                           glyph_metric_scale,
                                           pixel_filter_width,
                                           outline_width,
                                           shadow_blur);
#else
    float box_radius = shadow_blur + outline_width;
    vec2 box_radius_uv = vec2(box_radius) / glyph_metric_scale;

    float profile = ShadowProfile(shadow_blur);
    float large_profile = ShadowLargeProfile(shadow_blur);
    float filtered_alpha = 0.0;
    float total_weight = 0.0;
    int sample_count = ShadowSampleCount(shadow_blur);
    for (int i = 0; i < 8; ++i)
    {
        if (i >= sample_count)
        {
            continue;
        }
        float sample_t = (float(i) + 0.5) / float(sample_count);
        float sample_weight = ShadowFilterWeight(sample_t, large_profile);
        float sample_scale = ShadowFilterRadiusScale(sample_t, profile);
        vec2 sample_filter = pixel_filter_width + 2.0 * box_radius_uv * sample_scale;
        filtered_alpha += ScanlineSweepRender(p, curve_start, curve_count, sample_filter) * sample_weight;
        total_weight += sample_weight;
    }

    float soft_alpha = ShadowResponseAlpha(filtered_alpha / max(total_weight, 0.0001), profile, large_profile);
    float curve_distance = sqrt(ComputeCurveDistanceSqPixels(p, curve_start, curve_count, glyph_metric_scale));
    float feather_distance = max(curve_distance - outline_width, 0.0);
    float feather_alpha = ShadowFeatherAlpha(feather_distance, shadow_blur, profile);
    soft_alpha = 1.0 - (1.0 - soft_alpha) * (1.0 - feather_alpha);

    float correction_strength = mix(0.08, 0.34, large_profile) * smoothstep(2.0, 7.0, shadow_blur);
    if (correction_strength > 0.02)
    {
        float correction_alpha = ShadowShiftedCorrectionAlpha(p,
                                                              curve_start,
                                                              curve_count,
                                                              glyph_metric_scale,
                                                              pixel_filter_width,
                                                              box_radius,
                                                              profile,
                                                              large_profile);
        soft_alpha = 1.0 - (1.0 - soft_alpha) * (1.0 - correction_alpha * correction_strength);
    }
    return clamp(soft_alpha, 0.0, 1.0);
#endif
}

void main()
{
    float curve_count = var_glyph.x;
    float curve_start = var_glyph.z;
    float layer_mode = var_glyph.w;
    if (curve_count <= 0.0)
    {
        discard;
    }

    highp vec2 p = var_texcoord;
    float outline_width = max(var_jacobian.z, 0.0);
    float shadow_blur = max(var_jacobian.w, 0.0);
    vec2 glyph_metric_scale = max(var_params.xy, vec2(0.0001));
    vec2 pixel_filter_width = max(fwidth(p), vec2(1.0 / 65536.0));
    vec2 glyph_screen_scale = 1.0 / pixel_filter_width;

    if (scanline_debug.x > 0.5 && abs(layer_mode - LAYER_MODE_FACE) < 0.5)
    {
        vec2 debug_filter_width = scanline_debug_filter.x > 0.0 && scanline_debug_filter.y > 0.0
            ? scanline_debug_filter.xy
            : pixel_filter_width;
        int debug_band = int(floor(clamp(var_texcoord.x, 0.0, 0.999) * 20.0));
        int debug_page = debug_band / 4 + 1;
        int debug_component = debug_band - (debug_page - 1) * 4;
        vec4 debug_value = ScanlineSweepDebugPage(scanline_debug.zw,
                                                  curve_start,
                                                  curve_count,
                                                  debug_filter_width,
                                                  int(scanline_debug.y + 0.5),
                                                  debug_page);
        float debug_scalar = debug_component == 0 ? debug_value.x
            : debug_component == 1 ? debug_value.y
            : debug_component == 2 ? debug_value.z
            : debug_value.w;
        out_fragColor = vec4(vec3(debug_scalar), 1.0);
        return;
    }

    float coverage = ComputeStableGlyphCoverage(p, curve_start, curve_count, pixel_filter_width, glyph_screen_scale);

    if (abs(layer_mode - LAYER_MODE_FACE) < 0.5)
    {
        if (coverage <= 0.0)
        {
            discard;
        }
        float alpha = var_color.a * coverage;
        out_fragColor = vec4(var_color.rgb * alpha, alpha);
        return;
    }

    if (abs(layer_mode - LAYER_MODE_SHADOW) < 0.5)
    {
        float shadow_alpha = EvaluateFilteredShadowAlpha(p,
                                                         curve_start,
                                                         curve_count,
                                                         glyph_metric_scale,
                                                         pixel_filter_width,
                                                         coverage,
                                                         outline_width,
                                                         shadow_blur);

        if (shadow_alpha <= 0.0)
        {
            discard;
        }

        float alpha = var_color.a * shadow_alpha;
        out_fragColor = vec4(var_color.rgb * alpha, alpha);
        return;
    }

    float curve_distance = sqrt(ComputeCurveDistanceSqPixels(p, curve_start, curve_count, glyph_metric_scale));
    if (coverage <= 0.0 && curve_distance > outline_width)
    {
        discard;
    }

    out_fragColor = vec4(var_color.rgb * var_color.a, var_color.a);
}
