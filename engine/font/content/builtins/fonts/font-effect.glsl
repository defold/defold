// Layout and dimensions are shared with font_effect.h / FontPackEffect.
uniform mediump sampler2D font_effects;

#ifdef GL_FRAGMENT_PRECISION_HIGH
#define FONT_EFFECT_PRECISION highp
#else
#define FONT_EFFECT_PRECISION mediump
#endif

vec4 font_effect_texel(FONT_EFFECT_PRECISION vec2 base, float offset)
{
    return texture(font_effects, base + vec2(offset * (1.0 / 16.0), 0.0));
}

FONT_EFFECT_PRECISION float font_effect_position(vec2 bytes)
{
    return bytes.x * (256.0 / 257.0) + bytes.y * (1.0 / 257.0);
}

FONT_EFFECT_PRECISION float font_effect_parameter(vec4 bytes)
{
    FONT_EFFECT_PRECISION vec4 b = floor(bytes * 255.0 + 0.5);
    FONT_EFFECT_PRECISION float exponent = mod(b.w, 128.0) * 2.0 + floor(b.z / 128.0);
    if (exponent == 0.0)
        return 0.0;
    FONT_EFFECT_PRECISION float mantissa = 1.0 + (mod(b.z, 128.0) + b.y * (1.0 / 256.0) + b.x * (1.0 / 65536.0)) * (1.0 / 128.0);
    return (b.w < 128.0 ? mantissa : -mantissa) * exp2(exponent - 127.0);
}

float font_effect_mirror(float t)
{
    return 1.0 - abs(mod(t, 2.0) - 1.0);
}

FONT_EFFECT_PRECISION float font_effect_segment(FONT_EFFECT_PRECISION float t, FONT_EFFECT_PRECISION float a, FONT_EFFECT_PRECISION float b)
{
    if (b <= a)
        return step(b, t);
    return clamp((t - a) / (b - a), 0.0, 1.0);
}

// Returns straight RGBA. Each font shader applies its existing alpha convention.
// effect contains the fit UVs (xy), signed metadata row (z), and animation phase (w).
vec4 font_effect_color(highp vec4 effect)
{
    effect.z = abs(effect.z);
    if (effect.z < 0.5)
        return vec4(1.0);
    FONT_EFFECT_PRECISION float reciprocal_height = font_effect_parameter(texture(font_effects, vec2(0.0)));
    FONT_EFFECT_PRECISION vec2 base = vec2(0.5 / 16.0, (floor(effect.z + 0.5) + 0.5) * reciprocal_height);
    vec4 header = font_effect_texel(base, 0.0);
    float type = floor(header.x * 255.0 + 0.5);
    vec4 c0 = font_effect_texel(base, 1.0);
    if (type < 0.5)
        return c0;
    vec4 c1 = font_effect_texel(base, 2.0);
    vec4 c2 = font_effect_texel(base, 3.0);
    vec4 c3 = font_effect_texel(base, 4.0);
    if (type > 2.5)
    {
        vec2 uv = effect.xy;
        if (header.z > 0.0)
            uv = vec2(font_effect_mirror(uv.x + effect.w), font_effect_mirror(uv.y + effect.w));
        uv = clamp(uv, 0.0, 1.0);
        return mix(mix(c0, c1, uv.x), mix(c2, c3, uv.x), uv.y);
    }
    FONT_EFFECT_PRECISION vec2 parameters = vec2(font_effect_parameter(font_effect_texel(base, 7.0)),
                                                font_effect_parameter(font_effect_texel(base, 8.0)));
    FONT_EFFECT_PRECISION float t;
    if (type < 1.5)
        t = dot(effect.xy, parameters) + font_effect_parameter(font_effect_texel(base, 9.0));
    else
    {
        // CPU supplies the inverse ellipse transform, including fit dimensions.
        FONT_EFFECT_PRECISION vec2 scale = vec2(font_effect_parameter(font_effect_texel(base, 9.0)),
                                               font_effect_parameter(font_effect_texel(base, 10.0)));
        FONT_EFFECT_PRECISION vec2 cross_scale = vec2(font_effect_parameter(font_effect_texel(base, 11.0)),
                                                     font_effect_parameter(font_effect_texel(base, 12.0)));
        FONT_EFFECT_PRECISION vec2 offset = effect.xy - parameters;
        t = length(offset * scale + offset.yx * cross_scale);
    }
    t = header.z > 0.0 ? font_effect_mirror(t + effect.w) : clamp(t, 0.0, 1.0);
    vec4 p01 = font_effect_texel(base, 5.0);
    vec4 p23 = font_effect_texel(base, 6.0);
    FONT_EFFECT_PRECISION float p0 = font_effect_position(p01.xy);
    FONT_EFFECT_PRECISION float p1 = font_effect_position(p01.zw);
    FONT_EFFECT_PRECISION float p2 = font_effect_position(p23.xy);
    FONT_EFFECT_PRECISION float p3 = font_effect_position(p23.zw);
    vec4 color = mix(c0, c1, font_effect_segment(t, p0, p1));
    if (t >= p1)
        color = mix(c1, c2, font_effect_segment(t, p1, p2));
    if (t >= p2)
        color = mix(c2, c3, font_effect_segment(t, p2, p3));
    return color;
}

// Match markup precedence: gradients replace RGB and cap inherited alpha.
// Distance-field shaders pass premultiplied face colors; bitmap shaders do not.
vec4 font_effect_face_color(highp vec4 effect, vec4 face_color, bool premultiplied)
{
    if (abs(effect.z) < 0.5)
        return face_color;
    vec4 gradient = font_effect_color(effect);
    float alpha = min(face_color.a, gradient.a);
    return vec4(gradient.rgb * (premultiplied ? alpha : 1.0), alpha);
}

// Legacy corner colors were evaluated before rasterization, including mirrored
// animation and alpha caps. Negative rows select that behavior without fetching
// metadata for spatial effects here. Clear the row after resolving the color so
// the fragment shader uses the interpolated result. Input/output are straight RGBA.
vec4 font_effect_vertex_color(inout highp vec4 effect, vec4 face_color)
{
    if (effect.z >= 0.0)
        return face_color;
    vec4 color = font_effect_face_color(effect, face_color, false);
    effect.z = 0.0;
    return color;
}
