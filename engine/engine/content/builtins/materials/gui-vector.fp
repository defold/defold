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
in mediump vec4 var_color;

out vec4 out_fragColor;

void main()
{
    svgTime = var_svg_time;
    svgLayer = 0;
    // Keep box clipping rectangular, as in the raster GUI shader.
    out_fragColor = SlugPaintImageColor(int(var_paint + 0.5), var_svg_position, dFdx(var_svg_position), dFdy(var_svg_position), vec4(1.0)) * var_color;
}
