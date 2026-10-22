#ifndef DEFOLD_FONT_VECTOR_PAINT
#define DEFOLD_FONT_VECTOR_PAINT

// Numeric COLR layer, transform and paint records, indexed by each glyph quad.
uniform highp sampler2D paint_texture;
vec4 paintData(int index)
{
    return texelFetch(paint_texture, ivec2(index & 4095, index >> 12), 0);
}

vec3 linearColor(vec3 color)
{
    return mix(color / 12.92, pow((color + 0.055) / 1.055, vec3(2.4)), step(vec3(0.04045), color));
}

vec3 srgbColor(vec3 color)
{
    return mix(12.92 * color, 1.055 * pow(color, vec3(1.0 / 2.4)) - 0.055, step(vec3(0.0031308), color));
}

vec4 stopColor(int index, vec4 foreground)
{
    vec4 color = paintData(index);
    if (paintData(index + 1).y > 0.5)
        color.rgb = foreground.rgb;
    return vec4(color.rgb * color.a, color.a);
}

vec4 colorLine(float t, int count, int extend, int paint, vec4 foreground)
{
    int first = paint + 3;
    int last = first + (count - 1) * 2;
    float lo = paintData(first + 1).x;
    float hi = paintData(last + 1).x;
    if (hi > lo && extend != 0)
    {
        float u = (t - lo) / (hi - lo);
        u = extend == 1 ? fract(u) : 1.0 - abs(mod(u, 2.0) - 1.0);
        t = mix(lo, hi, u);
    }
    vec4 previous = stopColor(first, foreground);
    float previousOffset = lo;
    if (t < lo) return previous;
    for (int i = 1; i < count; ++i)
    {
        int index = first + i * 2;
        vec4 color = stopColor(index, foreground);
        float offset = paintData(index + 1).x;
        if (t < offset)
            return mix(previous, color, clamp((t - previousOffset) / (offset - previousOffset), 0.0, 1.0));
        previous = color;
        previousOffset = offset;
    }
    return previous;
}

vec4 FontVectorPaintColor(int paint, vec2 p, vec4 foreground)
{
    vec4 header = paintData(paint);
    vec4 p0 = paintData(paint + 1);
    vec4 p1 = paintData(paint + 2);
    float t = 0.0;
    if (header.x == 1.0)
    {
        // p0 holds the projected linear-gradient direction and origin.
        t = dot(p - p0.zw, p0.xy);
    }
    else if (header.x == 2.0)
    {
        // Solve |p - (c0 + t * dc)| = r0 + t * dr. COLR chooses
        // the greatest real root whose interpolated radius is nonnegative.
        vec2 q = p - p0.xy;
        vec2 dc = p1.xy - p0.xy;
        float dr = p1.z - p0.z;
        float a = dot(dc, dc) - dr * dr;
        float b = -2.0 * (dot(q, dc) + p0.z * dr);
        float c = dot(q, q) - p0.z * p0.z;
        if (abs(a) < 0.00001)
        {
            if (abs(b) < 0.00001) return vec4(0.0);
            t = -c / b;
            if (p0.z + t * dr < 0.0) return vec4(0.0);
        }
        else
        {
            float d = b * b - 4.0 * a * c;
            if (d < 0.0) return vec4(0.0);
            float t0 = (-b - sqrt(d)) / (2.0 * a);
            float t1 = (-b + sqrt(d)) / (2.0 * a);
            float hi = max(t0, t1);
            float lo = min(t0, t1);
            t = p0.z + hi * dr >= 0.0 ? hi : lo;
            if (p0.z + t * dr < 0.0) return vec4(0.0);
        }
    }
    return colorLine(t, int(header.y), int(header.z), paint, foreground);
}

vec4 FontVectorPaintGlyphColor(int glyph, vec2 p, vec2 dx, vec2 dy, vec4 foreground)
{
    // Palette stops are already linearized in the cache. Foreground is dynamic.
    foreground.rgb = linearColor(foreground.rgb);
    vec4 result = vec4(0.0);
    int count = int(paintData(glyph).x);
    vec4 composite = vec4(0.0);
    float coverage = 0.0;
    int compositeEnd = count;
    // Front-to-back source-over permits stopping behind an opaque layer.
    // Derivatives are supplied by the caller outside non-uniform control flow.
    for (int i = count - 1; i >= 0; --i)
    {
        int layer = glyph + 1 + i * 6;
        vec4 clip = paintData(layer + 5);
        bool clipped = any(lessThan(p, clip.xy)) || any(greaterThan(p, clip.zw));
        vec4 y = paintData(layer + 1);
        if (i < compositeEnd)
        {
            compositeEnd = y.w < 0.0 ? i - int(paintData(layer + 3).w) + 1 : i;
            if (i == compositeEnd && clipped)
                continue;
            vec4 x = paintData(layer);
            vec2 uv = vec2(dot(x.xyz, vec3(p, 1.0)), dot(y.xyz, vec3(p, 1.0)));
            vec2 emsPerPixel = abs(vec2(dot(x.xy, dx), dot(y.xy, dx))) + abs(vec2(dot(x.xy, dy), dot(y.xy, dy)));
            if (any(lessThan(uv, -2.0 * emsPerPixel)) || any(greaterThan(uv, vec2(1.0) + 2.0 * emsPerPixel)))
            {
                i = compositeEnd;
                continue;
            }
            int band = int(x.w);
            coverage = SlugRender(uv, emsPerPixel, paintData(layer + 2), ivec4(band & 4095, band >> 12, 7, 7));
            if (coverage <= 0.0)
            {
                i = compositeEnd;
                continue;
            }
        }
        if (!clipped)
        {
            vec2 position = vec2(dot(paintData(layer + 3).xyz, vec3(p, 1.0)), dot(paintData(layer + 4).xyz, vec3(p, 1.0)));
            int paint = int(y.w < 0.0 ? -y.w - 1.0 : y.w);
            composite += (1.0 - composite.a) * FontVectorPaintColor(paint, position, foreground);
        }
        if (i == compositeEnd || composite.a >= 1.0)
        {
            // A composite inside PaintGlyph shares its clip. Apply Slug coverage
            // once to the completed fill, including at partially covered edges.
            result += (1.0 - result.a) * coverage * composite;
            composite = vec4(0.0);
            i = compositeEnd;
            if (result.a >= 1.0)
                break;
        }
    }
    if (result.a <= 0.0)
        return vec4(0.0);
    // Return premultiplied sRGB to match ordinary text and framebuffer blending.
    result.rgb = srgbColor(result.rgb / result.a) * result.a;
    // Text opacity belongs to the completed glyph, not its overlapping paints.
    return foreground.a * result;
}

#endif
