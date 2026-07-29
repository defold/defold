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
flat in highp vec4 var_effect_params;
flat in highp vec4 var_glyph;

out vec4 out_fragColor;

uniform highp sampler2D curve_texture;

const int CURVE_TEXTURE_WIDTH_TEXELS = 512;
const int MAX_VECTOR_CURVES = 256;

vec4 SampleCurveTexel(int texel)
{
    int texel_x = texel % CURVE_TEXTURE_WIDTH_TEXELS;
    int texel_y = texel / CURVE_TEXTURE_WIDTH_TEXELS;
    return texelFetch(curve_texture, ivec2(texel_x, texel_y), 0);
}

void LoadCurve(int curve_texel, out vec2 p0, out vec2 p1, out vec2 p2)
{
    vec4 curve_a = SampleCurveTexel(curve_texel);
    vec4 curve_b = SampleCurveTexel(curve_texel + 1);
    p0 = curve_a.xy;
    p1 = curve_a.zw;
    p2 = curve_b.xy;
}

void LoadScanlineStripeRange(float y, out int curve_start, out int curve_count)
{
    float stripe_count = var_effect_params.y;
    float stripe_index = floor(clamp(y, 0.0, 0.999999) * stripe_count);
    int stripe_texel = int(var_effect_params.x + 0.5);
    vec4 stripe_metadata = SampleCurveTexel(stripe_texel + int(stripe_index));
    curve_start = stripe_texel + int(stripe_metadata.x + 0.5);
    curve_count = int(stripe_metadata.y + 0.5);
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

float ScanlineSweep(vec2 size, vec2 offset, vec2 p0, vec2 p1, vec2 p2)
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

float ScanlineSweepRender(vec2 render_coord, int curve_start, int curve_count, vec2 filter_width)
{
    float stripe_count = var_effect_params.y;
    if (stripe_count > 0.5)
    {
        float stripe_height = 1.0 / stripe_count;
        float half_filter_height = filter_width.y * 0.5;
        float stripe_index = floor(clamp(render_coord.y, 0.0, 0.999999) * stripe_count);
        float stripe_min_y = stripe_index * stripe_height;
        float stripe_max_y = stripe_min_y + stripe_height;
        bool crosses_lower_boundary = stripe_index > 0.5 && render_coord.y - half_filter_height < stripe_min_y;
        bool crosses_upper_boundary = stripe_index < stripe_count - 0.5 && render_coord.y + half_filter_height > stripe_max_y;
        if (half_filter_height <= stripe_height && !crosses_lower_boundary && !crosses_upper_boundary)
        {
            LoadScanlineStripeRange(render_coord.y, curve_start, curve_count);
        }
    }

    vec2 inv_filter_width = 1.0 / max(filter_width, vec2(1.0 / 65536.0));
    vec2 size = vec2(1.0);
    vec2 offset = render_coord * inv_filter_width - size * 0.5;
    float signed_area = 0.0;
    int curve_texel = curve_start;

    for (int i = 0; i < MAX_VECTOR_CURVES; ++i)
    {
        if (i >= curve_count)
        {
            break;
        }

        vec2 p0;
        vec2 p1;
        vec2 p2;
        LoadCurve(curve_texel, p0, p1, p2);
        curve_texel += 2;
        signed_area += ScanlineSweep(size,
                                     offset,
                                     p0 * inv_filter_width,
                                     p1 * inv_filter_width,
                                     p2 * inv_filter_width);
    }

    return clamp(abs(signed_area), 0.0, 1.0);
}

void main()
{
    int curve_count = int(var_glyph.x + 0.5);
    if (curve_count <= 0)
    {
        discard;
    }

    highp vec2 p = var_texcoord;
    int curve_start = int(var_glyph.z + 0.5);
    vec2 pixel_filter_width = max(fwidth(p), vec2(1.0 / 65536.0));
    float coverage = ScanlineSweepRender(p, curve_start, curve_count, pixel_filter_width);
    if (coverage <= 0.0)
    {
        discard;
    }

    float alpha = var_color.a * coverage;
    out_fragColor = vec4(var_color.rgb * alpha, alpha);
}
