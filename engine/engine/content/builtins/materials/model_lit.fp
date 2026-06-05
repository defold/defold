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

#define MAX_LIGHTS 8
#include "/builtins/materials/lighting.glsl"

vec3 evaluate_lighting(vec3 normal, vec3 view_position)
{
    int  light_count = int(light_info.w);
    vec3 total = light_info.xyz;

    // Keep the light indices explicit so ES2/editor GLSL and DX cross-compilation
    // both see compile-time constant accesses into the light array.
    if (0 < light_count)
        total += calculate_light(0, normal, view_position);
    if (1 < light_count)
        total += calculate_light(1, normal, view_position);
    if (2 < light_count)
        total += calculate_light(2, normal, view_position);
    if (3 < light_count)
        total += calculate_light(3, normal, view_position);
    if (4 < light_count)
        total += calculate_light(4, normal, view_position);
    if (5 < light_count)
        total += calculate_light(5, normal, view_position);
    if (6 < light_count)
        total += calculate_light(6, normal, view_position);
    if (7 < light_count)
        total += calculate_light(7, normal, view_position);

    return total;
}

void main()
{
    // Pre-multiply alpha since all runtime textures already are.
    vec4 tint_pm = vec4(tint.xyz * tint.w, tint.w);
    vec4 color = texture(tex0, var_texcoord0.xy) * tint_pm;

    vec3 lighting = evaluate_lighting(normalize(var_normal), var_position.xyz);
    out_fragColor = vec4(color.rgb * lighting, 1.0);
}
