#version 140

in highp vec4 var_position;
in mediump vec3 var_normal;
in mediump vec2 var_texcoord0;
in highp mat4 var_view;

out vec4 out_fragColor;

uniform mediump sampler2D tex0;

uniform fs_uniforms
{
    mediump vec4 tint;
};

#define MAX_LIGHT_COUNT 8
#include "/builtins/materials/lighting.glsl"

void main()
{
    // Texture profiles can premultiply color textures.
    // We need to premultiply the tint as well so tint alpha scales both color and alpha.
    vec4 tint_pm = vec4(tint.xyz * tint.w, tint.w);
    vec4 color = texture(tex0, var_texcoord0.xy) * tint_pm;
    vec3 ambient = ambient_light();
    vec3 diffuse = diffuse_lambert(normalize(var_normal), var_position.xyz);
    out_fragColor = vec4(color.rgb * (ambient + diffuse), color.a);
}
