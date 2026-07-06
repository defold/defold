#version 140

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
uniform highp sampler2D band_texture;

const float CURVE_TEXTURE_WIDTH = 512.0;
const float CURVE_TEXTURE_HEIGHT = 64.0;
const int CURVE_TEXTURE_WIDTH_TEXELS = 512;
const int MAX_VECTOR_CURVES = 256;
const float LAYER_MODE_FACE = 0.0;
const float LAYER_MODE_OUTLINE = 1.0;
const float LAYER_MODE_SHADOW = 2.0;

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
    const float epsilon = 0.000001;
    float min_c = min(c0, c2);
    float max_c = max(c0, c2);
    if (target <= min_c)
    {
        return c0 < c2 ? 0.0 : 1.0;
    }
    if (target >= max_c)
    {
        return c0 < c2 ? 1.0 : 0.0;
    }

    if (abs(qa) < epsilon)
    {
        float denom = c2 - c0;
        return abs(denom) < epsilon ? 0.0 : clamp((target - c0) / denom, 0.0, 1.0);
    }

    float qb = 2.0 * (c1 - c0);
    float qc = c0 - target;
    float discriminant = max(qb * qb - 4.0 * qa * qc, 0.0);
    float direction = c2 >= c0 ? 1.0 : -1.0;
    return clamp((-qb + direction * sqrt(discriminant)) * (0.5 / qa), 0.0, 1.0);
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
    p0 -= offset;
    p1 -= offset;
    p2 -= offset;

    if (abs(delta.y) < 0.000001)
    {
        return 0.0;
    }

    float qa_y = p0.y + p2.y - 2.0 * p1.y;
    float y0_t = intersect_monotonic(qa_y, p0.y, p1.y, p2.y, 0.0);
    float y1_t = intersect_monotonic(qa_y, p0.y, p1.y, p2.y, size.y);
    float y_min_t = min(y0_t, y1_t);
    float y_max_t = max(y0_t, y1_t);

    vec2 v_min = evaluate_bezier(p0, p1, p2, y_min_t);
    vec2 v_max = evaluate_bezier(p0, p1, p2, y_max_t);
    float y_delta = v_max.y - v_min.y;
    if (max(v_min.x, v_max.x) <= 0.0)
    {
        return y_delta * size.x;
    }

    if (min(v_min.x, v_max.x) >= size.x)
    {
        return 0.0;
    }

    if (abs(delta.x) < 0.000001)
    {
        return clamp(size.x - v_min.x, 0.0, size.x) * y_delta;
    }

    float qa_x = p0.x + p2.x - 2.0 * p1.x;
    float x0_t = intersect_monotonic(qa_x, p0.x, p1.x, p2.x, 0.0);
    float x1_t = intersect_monotonic(qa_x, p0.x, p1.x, p2.x, size.x);
    float x_enter_t = delta.x > 0.0 ? x0_t : x1_t;
    float x_exit_t = delta.x > 0.0 ? x1_t : x0_t;

    float coverage = 0.0;
    if (delta.x > 0.0)
    {
        float left_end_t = min(y_max_t, x_enter_t);
        if (left_end_t > y_min_t)
        {
            coverage += size.x * (QuadraticAxis(left_end_t, p0.y, p1.y, p2.y) - v_min.y);
        }
    }
    else
    {
        float left_start_t = max(y_min_t, x_exit_t);
        if (y_max_t > left_start_t)
        {
            coverage += size.x * (v_max.y - QuadraticAxis(left_start_t, p0.y, p1.y, p2.y));
        }
    }

    float middle_start_t = max(y_min_t, x_enter_t);
    float middle_end_t = min(y_max_t, x_exit_t);
    if (middle_end_t > middle_start_t)
    {
        coverage += scanline_sweep_area_to_right(middle_start_t, middle_end_t, size.x, p0, p1, p2);
    }

    return coverage;
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

float EvaluateFilteredShadowAlpha(vec2 p,
                                  float curve_start,
                                  float curve_count,
                                  vec2 glyph_scale,
                                  float face_coverage,
                                  float outline_width,
                                  float shadow_blur)
{
    float curve_distance = sqrt(ComputeCurveDistanceSqPixels(p, curve_start, curve_count, glyph_scale));
    float center_alpha = EvaluateShadowSilhouetteAlpha(face_coverage, curve_distance, outline_width);
    if (shadow_blur <= 0.0)
    {
        return center_alpha;
    }

    float feather_alpha = 1.0 - smoothstep(outline_width, outline_width + shadow_blur, curve_distance);
    return max(center_alpha, feather_alpha);
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
    float coverage = ComputeGlyphCoverage(p, curve_start, curve_count, glyph_screen_scale);

    if (abs(layer_mode - LAYER_MODE_FACE) < 0.5)
    {
        if (coverage <= 0.0)
        {
            discard;
        }
        out_fragColor = vec4(var_color.rgb, var_color.a * coverage);
        return;
    }

    if (abs(layer_mode - LAYER_MODE_SHADOW) < 0.5)
    {
        float shadow_alpha = EvaluateFilteredShadowAlpha(p,
                                                         curve_start,
                                                         curve_count,
                                                         glyph_metric_scale,
                                                         coverage,
                                                         outline_width,
                                                         shadow_blur);

        if (shadow_alpha <= 0.0)
        {
            discard;
        }

        out_fragColor = vec4(var_color.rgb, var_color.a * shadow_alpha);
        return;
    }

    float curve_distance = sqrt(ComputeCurveDistanceSqPixels(p, curve_start, curve_count, glyph_metric_scale));
    if (coverage <= 0.0 && curve_distance > outline_width)
    {
        discard;
    }

    out_fragColor = var_color;
}
