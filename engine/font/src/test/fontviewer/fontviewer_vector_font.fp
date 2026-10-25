#version 330

// The production Slug coverage shader is inserted here by fontviewer.
#include "font-vector-slug.glsl"
#include "font-vector-paint.glsl"
#include "/builtins/fonts/font-effect.glsl"
flat in int var_paint;
in vec4 var_color;
in vec4 var_effect;
flat in vec4 var_clip;
in vec2 var_font_position;
in vec2 var_position;
out vec4 out_fragColor;

void main()
{
    vec2 dx = dFdx(var_font_position);
    vec2 dy = dFdy(var_font_position);
    if (any(lessThan(var_position, var_clip.xy)) || any(greaterThan(var_position, var_clip.zw)))
        discard;
    out_fragColor = FontVectorPaintGlyphColor(var_paint, var_font_position, dx, dy, font_effect_face_color(var_effect, var_color, false));
}
