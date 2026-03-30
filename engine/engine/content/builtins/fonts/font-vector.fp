#version 140

in mediump vec2 var_texcoord;
in mediump vec4 var_color;
in mediump vec4 var_banding;
in mediump vec4 var_glyph;

out vec4 out_fragColor;

uniform mediump sampler2D curve_texture;
uniform mediump sampler2D band_texture;

const float CURVE_TEXTURE_WIDTH = 512.0;
const float BAND_TEXTURE_WIDTH = 128.0;
const int MAX_VECTOR_CURVES = 64;

vec4 SampleCurveTexel(float x)
{
    return textureLod(curve_texture, vec2((x + 0.5) / CURVE_TEXTURE_WIDTH, 0.5), 0.0);
}

vec2 SampleBandTexel(float x)
{
    return textureLod(band_texture, vec2((x + 0.5) / BAND_TEXTURE_WIDTH, 0.5), 0.0).xy;
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

float CountQuadraticCrossings(vec2 p, vec2 p0, vec2 p1, vec2 p2)
{
    // Use a half-open y-interval on the segment endpoints so shared contour
    // endpoints are counted by only one neighboring curve. This removes the
    // visible spike at extrema such as the top of the hardcoded circle.
    const float x_epsilon = 0.0001;
    float y_min = min(p0.y, p2.y);
    float y_max = max(p0.y, p2.y);
    if ((p.y < y_min) || (p.y >= y_max))
    {
        return 0.0;
    }

    float a = p0.y - 2.0 * p1.y + p2.y;
    float b = -2.0 * p0.y + 2.0 * p1.y;
    float c = p0.y - p.y;
    float crossings = 0.0;

    if (abs(a) < 0.00001)
    {
        if (abs(b) < 0.00001)
        {
            return 0.0;
        }

        float t = -c / b;
        if ((t >= 0.0) && (t < 1.0))
        {
            float x = QuadraticAxis(t, p0.x, p1.x, p2.x);
            if (x > (p.x + x_epsilon))
            {
                crossings += 1.0;
            }
        }

        return crossings;
    }

    float discriminant = b * b - 4.0 * a * c;
    if (discriminant < 0.0)
    {
        return 0.0;
    }

    float root = sqrt(discriminant);
    float inv = 0.5 / a;
    float t0 = (-b - root) * inv;
    float t1 = (-b + root) * inv;

    if ((t0 >= 0.0) && (t0 < 1.0))
    {
        float x0 = QuadraticAxis(t0, p0.x, p1.x, p2.x);
        if (x0 > (p.x + x_epsilon))
        {
            crossings += 1.0;
        }
    }

    if ((t1 >= 0.0) && (t1 < 1.0) && (abs(t1 - t0) > 0.0001))
    {
        float x1 = QuadraticAxis(t1, p0.x, p1.x, p2.x);
        if (x1 > (p.x + x_epsilon))
        {
            crossings += 1.0;
        }
    }

    return crossings;
}

float ComputeGlyphCrossings(vec2 p, float curve_start, float curve_count)
{
    mediump float crossings = 0.0;
    for (int i = 0; i < MAX_VECTOR_CURVES; ++i)
    {
        if (float(i) >= curve_count)
        {
            break;
        }

        float curve_texel = curve_start + float(i * 2);
        vec4 curve_a = SampleCurveTexel(curve_texel);
        vec4 curve_b = SampleCurveTexel(curve_texel + 1.0);
        crossings += CountQuadraticCrossings(p, curve_a.xy, curve_a.zw, curve_b.xy);
    }

    return crossings;
}

float IsInsideGlyph(vec2 p, float curve_start, float curve_count)
{
    return step(0.5, mod(ComputeGlyphCrossings(p, curve_start, curve_count), 2.0));
}

float QuadraticScaledDistanceSq(vec2 p, vec2 p0, vec2 p1, vec2 p2, vec2 outline_radius)
{
    const int COARSE_STEPS = 8;
    const int REFINE_STEPS = 4;
    vec2 inv_radius = 1.0 / max(outline_radius, vec2(0.0001));

    float best_t = 0.0;
    float best_distance_sq = 1e20;

    for (int i = 0; i <= COARSE_STEPS; ++i)
    {
        float t = float(i) / float(COARSE_STEPS);
        vec2 curve_p = QuadraticPoint(t, p0, p1, p2);
        vec2 delta = (curve_p - p) * inv_radius;
        float distance_sq = dot(delta, delta);
        if (distance_sq < best_distance_sq)
        {
            best_distance_sq = distance_sq;
            best_t = t;
        }
    }

    float span = 1.0 / float(COARSE_STEPS);
    for (int i = 0; i < REFINE_STEPS; ++i)
    {
        float t_min = max(0.0, best_t - span);
        float t_max = min(1.0, best_t + span);
        float t_a = mix(t_min, t_max, 0.33333334);
        float t_b = mix(t_min, t_max, 0.66666669);

        vec2 curve_a = QuadraticPoint(t_a, p0, p1, p2);
        vec2 curve_b = QuadraticPoint(t_b, p0, p1, p2);

        vec2 delta_a = (curve_a - p) * inv_radius;
        vec2 delta_b = (curve_b - p) * inv_radius;
        float distance_sq_a = dot(delta_a, delta_a);
        float distance_sq_b = dot(delta_b, delta_b);

        if (distance_sq_a < distance_sq_b)
        {
            best_t = t_a;
            best_distance_sq = min(best_distance_sq, distance_sq_a);
            t_max = t_b;
        }
        else
        {
            best_t = t_b;
            best_distance_sq = min(best_distance_sq, distance_sq_b);
            t_min = t_a;
        }

        span = (t_max - t_min) * 0.5;
    }

    return best_distance_sq;
}

float ComputeOutlineDistanceSq(vec2 p, float curve_start, float curve_count, vec2 outline_radius)
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
            QuadraticScaledDistanceSq(p, curve_a.xy, curve_a.zw, curve_b.xy, outline_radius));
    }
    return best_distance_sq;
}

void main()
{
    vec2 glyph_header = SampleBandTexel(var_glyph.z);
    mediump float curve_start = glyph_header.x;
    mediump float curve_count = glyph_header.y;
    if (curve_count <= 0.0)
    {
        discard;
    }

    mediump vec2 p = var_texcoord;
    float inside = IsInsideGlyph(p, curve_start, curve_count);
    float layer_mode = var_banding.z;

    if (layer_mode < 0.5)
    {
        if (inside < 0.5)
        {
            discard;
        }
        out_fragColor = var_color;
        return;
    }

    if (inside >= 0.5)
    {
        discard;
    }

    vec2 outline_radius = max(var_banding.xy, vec2(0.0001));
    float outline_distance_sq = ComputeOutlineDistanceSq(p, curve_start, curve_count, outline_radius);
    if (outline_distance_sq > 1.0)
    {
        discard;
    }

    out_fragColor = var_color;
}
