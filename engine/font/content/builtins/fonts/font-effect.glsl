// Layout and dimensions are shared with font_effect.h / FontPackEffect.
uniform mediump sampler2D font_effects;

// Keep metadata decoding and texture addressing at highp even when the
// project's default float precision is mediump.
vec4 font_effect_texel(highp vec2 base, float offset)
{
    return texture(font_effects, base + vec2(offset * (1.0 / 16.0), 0.0));
}

highp float font_effect_position(vec2 bytes)
{
    return bytes.x * (256.0 / 257.0) + bytes.y * (1.0 / 257.0);
}

highp float font_effect_parameter(vec4 bytes)
{
    highp vec4 b = floor(bytes * 255.0 + 0.5);
    highp float exponent = mod(b.w, 128.0) * 2.0 + floor(b.z / 128.0);
    highp float mantissa = 1.0 + (mod(b.z, 128.0) + b.y * (1.0 / 256.0) + b.x * (1.0 / 65536.0)) * (1.0 / 128.0);
    highp float value = (b.w < 128.0 ? mantissa : -mantissa) * exp2(exponent - 127.0);
    return exponent == 0.0 ? 0.0 : value;
}

float font_effect_mirror(float t)
{
    return 1.0 - abs(mod(t, 2.0) - 1.0);
}

highp float font_effect_segment(highp float t, highp float a, highp float b)
{
    return b <= a ? step(b, t) : clamp((t - a) / (b - a), 0.0, 1.0);
}

// Early returns around texture reads become nested loops in the generated WebGL
// shaders and make ANGLE's D3D11 backend slow to link. Keep one return per helper.
vec4 font_effect_row_color(highp vec4 effect)
{
    highp float row = abs(effect.z);
    highp float reciprocal_height = font_effect_parameter(texture(font_effects, vec2(0.0)));
    highp vec2 base = vec2(0.5 / 16.0, (floor(row + 0.5) + 0.5) * reciprocal_height);
    vec4 header = font_effect_texel(base, 0.0);
    float type = floor(header.x * 255.0 + 0.5);
    bool mirrored = header.z > 0.0;
    vec4 color = font_effect_texel(base, 1.0);
    if (type > 0.5)
    {
        vec4 c0 = color;
        vec4 c1 = font_effect_texel(base, 2.0);
        vec4 c2 = font_effect_texel(base, 3.0);
        vec4 c3 = font_effect_texel(base, 4.0);
        if (type > 2.5)
        {
            vec2 uv = mirrored ? vec2(font_effect_mirror(effect.x + effect.w), font_effect_mirror(effect.y + effect.w))
                               : effect.xy;
            uv = clamp(uv, 0.0, 1.0);
            color = mix(mix(c0, c1, uv.x), mix(c2, c3, uv.x), uv.y);
        }
        else
        {
            highp vec2 parameters = vec2(font_effect_parameter(font_effect_texel(base, 7.0)),
                                         font_effect_parameter(font_effect_texel(base, 8.0)));
            highp float parameter9 = font_effect_parameter(font_effect_texel(base, 9.0));
            highp float t;
            if (type < 1.5)
            {
                t = dot(effect.xy, parameters) + parameter9;
            }
            else
            {
                // CPU supplies the inverse ellipse transform, including fit dimensions.
                highp vec2 scale = vec2(parameter9, font_effect_parameter(font_effect_texel(base, 10.0)));
                highp vec2 cross_scale = vec2(font_effect_parameter(font_effect_texel(base, 11.0)),
                                              font_effect_parameter(font_effect_texel(base, 12.0)));
                highp vec2 offset = effect.xy - parameters;
                t = length(offset * scale + offset.yx * cross_scale);
            }
            t = mirrored ? font_effect_mirror(t + effect.w) : clamp(t, 0.0, 1.0);
            vec4 p01 = font_effect_texel(base, 5.0);
            vec4 p23 = font_effect_texel(base, 6.0);
            highp float p0 = font_effect_position(p01.xy);
            highp float p1 = font_effect_position(p01.zw);
            highp float p2 = font_effect_position(p23.xy);
            highp float p3 = font_effect_position(p23.zw);
            color = mix(c0, c1, font_effect_segment(t, p0, p1));
            color = t >= p1 ? mix(c1, c2, font_effect_segment(t, p1, p2)) : color;
            color = t >= p2 ? mix(c2, c3, font_effect_segment(t, p2, p3)) : color;
        }
    }
    return color;
}

// Returns straight RGBA. Each font shader applies its existing alpha convention.
// effect contains the fit UVs (xy), signed metadata row (z), and animation phase (w).
vec4 font_effect_color(highp vec4 effect)
{
    vec4 color = vec4(1.0);
    if (abs(effect.z) >= 0.5)
    {
        color = font_effect_row_color(effect);
    }
    return color;
}

// Match markup precedence: gradients replace RGB and cap inherited alpha.
// Distance-field shaders pass premultiplied face colors; bitmap shaders do not.
vec4 font_effect_face_color(highp vec4 effect, vec4 face_color, bool premultiplied)
{
    if (abs(effect.z) >= 0.5)
    {
        vec4 gradient = font_effect_row_color(effect);
        float alpha = min(face_color.a, gradient.a);
        face_color = vec4(gradient.rgb * (premultiplied ? alpha : 1.0), alpha);
    }
    return face_color;
}

// Legacy corner colors were evaluated before rasterization, including mirrored
// animation and alpha caps. Negative rows select that behavior without fetching
// metadata for spatial effects here. Clear the row after resolving the color so
// the fragment shader uses the interpolated result. Input/output are straight RGBA.
vec4 font_effect_vertex_color(inout highp vec4 effect, vec4 face_color)
{
    if (effect.z < 0.0)
    {
        face_color = font_effect_face_color(effect, face_color, false);
        effect.z = 0.0;
    }
    return face_color;
}
