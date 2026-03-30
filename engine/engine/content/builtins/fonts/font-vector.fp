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

    float inside = mod(crossings, 2.0);
    if (inside < 0.5)
    {
        discard;
    }

    out_fragColor = var_color;
}
