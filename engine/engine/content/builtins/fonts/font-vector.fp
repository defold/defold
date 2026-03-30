#version 140

in mediump vec2 var_texcoord;
in mediump vec4 var_color;
in mediump vec4 var_banding;
in mediump vec4 var_jacobian;
in mediump vec4 var_glyph;
in mediump vec4 var_params;

out vec4 out_fragColor;

uniform mediump sampler2D curve_texture;
uniform mediump sampler2D band_texture;

const float CURVE_TEXTURE_WIDTH = 512.0;
const float CURVE_TEXTURE_HEIGHT = 64.0;
const float BAND_TEXTURE_WIDTH = 2048.0;
const float BAND_TEXTURE_HEIGHT = 128.0;
const int MAX_VECTOR_CURVES = 256;
const int MAX_VECTOR_BAND_CURVES = 256;
const float LAYER_MODE_FACE = 0.0;
const float LAYER_MODE_OUTLINE = 1.0;
const float LAYER_MODE_SHADOW = 2.0;

vec4 SampleCurveTexel(float texel_index)
{
    float texel_x = mod(texel_index, CURVE_TEXTURE_WIDTH);
    float texel_y = floor(texel_index / CURVE_TEXTURE_WIDTH);
    return textureLod(curve_texture, vec2((texel_x + 0.5) / CURVE_TEXTURE_WIDTH, (texel_y + 0.5) / CURVE_TEXTURE_HEIGHT), 0.0);
}

vec4 SampleBandTexel(float row, float column)
{
    return textureLod(band_texture, vec2((column + 0.5) / BAND_TEXTURE_WIDTH, (row + 0.5) / BAND_TEXTURE_HEIGHT), 0.0);
}

int CalcRootCode(float y1, float y2, float y3)
{
    int i1 = (y1 < 0.0) ? 1 : 0;
    int i2 = (y2 < 0.0) ? 2 : 0;
    int i3 = (y3 < 0.0) ? 4 : 0;
    int shift = i1 + i2 + i3;
    return (0x2E74 >> shift) & 0x0101;
}

vec2 SolveHorizPoly(vec4 p12, vec2 p3)
{
    vec2 a = p12.xy - p12.zw * 2.0 + p3;
    vec2 b = p12.xy - p12.zw;
    float ra = 1.0 / a.y;
    float rb = 0.5 / b.y;

    float d = sqrt(max(b.y * b.y - a.y * p12.y, 0.0));
    float t1 = (b.y - d) * ra;
    float t2 = (b.y + d) * ra;

    if (abs(a.y) < 1.0 / 65536.0)
    {
        t1 = t2 = p12.y * rb;
    }

    return vec2((a.x * t1 - b.x * 2.0) * t1 + p12.x, (a.x * t2 - b.x * 2.0) * t2 + p12.x);
}

vec2 SolveVertPoly(vec4 p12, vec2 p3)
{
    vec2 a = p12.xy - p12.zw * 2.0 + p3;
    vec2 b = p12.xy - p12.zw;
    float ra = 1.0 / a.x;
    float rb = 0.5 / b.x;

    float d = sqrt(max(b.x * b.x - a.x * p12.x, 0.0));
    float t1 = (b.x - d) * ra;
    float t2 = (b.x + d) * ra;

    if (abs(a.x) < 1.0 / 65536.0)
    {
        t1 = t2 = p12.x * rb;
    }

    return vec2((a.y * t1 - b.y * 2.0) * t1 + p12.y, (a.y * t2 - b.y * 2.0) * t2 + p12.y);
}

float CalcCoverage(float xcov, float ycov, float xwgt, float ywgt)
{
    float coverage = max(abs(xcov * xwgt + ycov * ywgt) / max(xwgt + ywgt, 1.0 / 65536.0), min(abs(xcov), abs(ycov)));
    return clamp(coverage, 0.0, 1.0);
}

float SlugRender(vec2 render_coord, vec4 band_transform, float band_row, ivec2 band_max)
{
    vec2 ems_per_pixel = fwidth(render_coord);
    vec2 pixels_per_em = 1.0 / max(ems_per_pixel, vec2(1.0 / 65536.0));
    ivec2 band_index = clamp(ivec2(render_coord * band_transform.xy + band_transform.zw), ivec2(0, 0), band_max);

    vec4 hband_raw = SampleBandTexel(band_row, float(band_index.y));
    int hband_count = int(hband_raw.x + 0.5);
    int hband_offset = int(hband_raw.y + 0.5);

    float xcov = 0.0;
    float xwgt = 0.0;

    for (int curve_index = 0; curve_index < MAX_VECTOR_BAND_CURVES; ++curve_index)
    {
        if (curve_index >= hband_count)
        {
            break;
        }

        vec4 loc_raw = SampleBandTexel(band_row, float(hband_offset + curve_index));
        float curve_texel = loc_raw.x;

        vec4 p12 = SampleCurveTexel(curve_texel) - vec4(render_coord, render_coord);
        vec2 p3 = SampleCurveTexel(curve_texel + 1.0).xy - render_coord;

        if (max(max(p12.x, p12.z), p3.x) * pixels_per_em.x < -0.5)
        {
            break;
        }

        int code = CalcRootCode(p12.y, p12.w, p3.y);
        if (code != 0)
        {
            vec2 r = SolveHorizPoly(p12, p3) * pixels_per_em.x;

            if ((code & 1) != 0)
            {
                xcov += clamp(r.x + 0.5, 0.0, 1.0);
                xwgt = max(xwgt, clamp(1.0 - abs(r.x) * 2.0, 0.0, 1.0));
            }

            if (code > 1)
            {
                xcov -= clamp(r.y + 0.5, 0.0, 1.0);
                xwgt = max(xwgt, clamp(1.0 - abs(r.y) * 2.0, 0.0, 1.0));
            }
        }
    }

    vec4 vband_raw = SampleBandTexel(band_row, float(band_max.y + 1 + band_index.x));
    int vband_count = int(vband_raw.x + 0.5);
    int vband_offset = int(vband_raw.y + 0.5);

    float ycov = 0.0;
    float ywgt = 0.0;

    for (int curve_index = 0; curve_index < MAX_VECTOR_BAND_CURVES; ++curve_index)
    {
        if (curve_index >= vband_count)
        {
            break;
        }

        vec4 loc_raw = SampleBandTexel(band_row, float(vband_offset + curve_index));
        float curve_texel = loc_raw.x;

        vec4 p12 = SampleCurveTexel(curve_texel) - vec4(render_coord, render_coord);
        vec2 p3 = SampleCurveTexel(curve_texel + 1.0).xy - render_coord;

        if (max(max(p12.y, p12.w), p3.y) * pixels_per_em.y < -0.5)
        {
            break;
        }

        int code = CalcRootCode(p12.x, p12.z, p3.x);
        if (code != 0)
        {
            vec2 r = SolveVertPoly(p12, p3) * pixels_per_em.y;

            if ((code & 1) != 0)
            {
                ycov -= clamp(r.x + 0.5, 0.0, 1.0);
                ywgt = max(ywgt, clamp(1.0 - abs(r.x) * 2.0, 0.0, 1.0));
            }

            if (code > 1)
            {
                ycov += clamp(r.y + 0.5, 0.0, 1.0);
                ywgt = max(ywgt, clamp(1.0 - abs(r.y) * 2.0, 0.0, 1.0));
            }
        }
    }

    return CalcCoverage(xcov, ycov, xwgt, ywgt);
}

float QuadraticAxis(float t, float p0, float p1, float p2)
{
    float mt = 1.0 - t;
    return mt * mt * p0 + 2.0 * mt * t * p1 + t * t * p2;
}

vec2 QuadraticPoint(float t, vec2 p0, vec2 p1, vec2 p2)
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
            vec2 delta = (QuadraticPoint(t, p0, p1, p2) - p) * glyph_scale;
            vec2 tangent = QuadraticTangent(t, p0, p1, p2) * glyph_scale;

            float f = dot(delta, tangent);
            float df = dot(tangent, tangent) + dot(delta, dd);
            if (abs(df) < 0.000001)
            {
                break;
            }

            t = clamp(t - f / df, 0.0, 1.0);
        }

        vec2 curve_p = QuadraticPoint(t, p0, p1, p2);
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

void main()
{
    float curve_count = var_glyph.x;
    float band_row = var_glyph.y;
    float curve_start = var_glyph.z;
    float layer_mode = var_glyph.w;
    if (curve_count <= 0.0)
    {
        discard;
    }

    mediump vec2 p = var_texcoord;
    float outline_width = max(var_jacobian.z, 0.0);
    float shadow_blur = max(var_jacobian.w, 0.0);
    ivec2 band_max = ivec2(int(var_jacobian.x + 0.5), int(var_jacobian.y + 0.5));
    vec2 glyph_scale = 1.0 / max(fwidth(var_texcoord), vec2(0.0001));
    vec2 glyph_metric_scale = max(var_params.xy, vec2(0.0001));
    float coverage = SlugRender(p, var_banding, band_row, band_max);

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
        float curve_distance = sqrt(ComputeCurveDistanceSqPixels(p, curve_start, curve_count, glyph_metric_scale));
        if (coverage >= 0.999 || curve_distance <= outline_width)
        {
            out_fragColor = var_color;
            return;
        }

        if (shadow_blur <= 0.0)
        {
            discard;
        }

        if (curve_distance > (outline_width + shadow_blur))
        {
            discard;
        }

        float shadow_alpha = 1.0 - smoothstep(outline_width, outline_width + shadow_blur, curve_distance);
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
