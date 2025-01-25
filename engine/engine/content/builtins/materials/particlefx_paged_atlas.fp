#version 140

in mediump vec2 var_texcoord0;
in mediump vec4 var_color;
in mediump float var_page_index;

out vec4 out_fragColor;

uniform mediump sampler2DArray texture_sampler;

uniform fs_uniforms
{
    mediump vec4 tint;
};

void main()
{
    // Pre-multiply alpha since all runtime textures already are
    mediump vec4 tint_pm = vec4(tint.xyz * tint.w, tint.w);
    // var_color is vertex color from the particle system, pre-multiplied in vertex program
    out_fragColor = texture(texture_sampler, vec3(var_texcoord0.xy, var_page_index)) * var_color * tint_pm;
}
