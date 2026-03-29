#version 140

in mediump vec2 var_texcoord;
in mediump vec4 var_color;
in mediump vec4 var_banding;
in mediump vec4 var_glyph;

out vec4 out_fragColor;

uniform mediump sampler2D curve_texture;
uniform mediump sampler2D band_texture;

vec4 SampleCurveTexel(float x)
{
    return texture(curve_texture, vec2((x + 0.5) / 8.0, 0.5));
}

vec2 SampleBandTexel(float x)
{
    return texture(band_texture, vec2((x + 0.5) / 5.0, 0.5)).xy;
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
    mediump float curve_count = SampleBandTexel(0.0).x;
    if (curve_count <= 0.0)
    {
        discard;
    }

    mediump vec2 p = var_texcoord;
    vec2 curve_loc0 = SampleBandTexel(1.0);
    vec2 curve_loc1 = SampleBandTexel(2.0);
    vec2 curve_loc2 = SampleBandTexel(3.0);
    vec2 curve_loc3 = SampleBandTexel(4.0);

    vec4 curve0a = SampleCurveTexel(curve_loc0.x);
    vec4 curve0b = SampleCurveTexel(curve_loc0.x + 1.0);
    vec4 curve1a = SampleCurveTexel(curve_loc1.x);
    vec4 curve1b = SampleCurveTexel(curve_loc1.x + 1.0);
    vec4 curve2a = SampleCurveTexel(curve_loc2.x);
    vec4 curve2b = SampleCurveTexel(curve_loc2.x + 1.0);
    vec4 curve3a = SampleCurveTexel(curve_loc3.x);
    vec4 curve3b = SampleCurveTexel(curve_loc3.x + 1.0);

    float use0 = step(0.5, curve_count);
    float use1 = step(1.5, curve_count);
    float use2 = step(2.5, curve_count);
    float use3 = step(3.5, curve_count);

    mediump float crossings = 0.0;
    crossings += use0 * CountQuadraticCrossings(p, curve0a.xy, curve0a.zw, curve0b.xy);
    crossings += use1 * CountQuadraticCrossings(p, curve1a.xy, curve1a.zw, curve1b.xy);
    crossings += use2 * CountQuadraticCrossings(p, curve2a.xy, curve2a.zw, curve2b.xy);
    crossings += use3 * CountQuadraticCrossings(p, curve3a.xy, curve3a.zw, curve3b.xy);

    float inside = mod(crossings, 2.0);
    if (inside < 0.5)
    {
        discard;
    }

    out_fragColor = var_color;
}
