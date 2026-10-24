#version 140

#extension GL_EXT_samplerless_texture_functions : require

precision highp float;
precision highp int;

#define SLUG_EVENODD
#define SLUG_ANIMATION
#include "/builtins/materials/slug.glsl"
#include "/builtins/materials/slug-paint.glsl"

in highp vec2 var_svg_position;
flat in highp float var_paint;
flat in highp float var_svg_time;

uniform fs_uniforms
{
    vec4 tint;
};

#ifdef SPRITE_VECTOR_PICKING
uniform picking_uniforms
{
    vec4 id;
};
#endif

out vec4 out_fragColor;

void main()
{
    svgTime = var_svg_time;
    svgLayer = 0;
    vec2 dx = dFdx(var_svg_position);
    vec2 dy = dFdy(var_svg_position);
    out_fragColor = SlugPaintImageColor(int(var_paint + 0.5), var_svg_position, dx, dy, tint);
    if (out_fragColor.a <= 0.0)
        discard;
#ifdef SPRITE_VECTOR_PICKING
    out_fragColor = id;
#endif
}
