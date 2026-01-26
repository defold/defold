#version 140

in mediump vec2 var_texcoord0;
in lowp vec4 var_tint_pm;

out vec4 out_frag_color;

uniform mediump sampler2D texture_sampler;

void main() {
    out_frag_color = texture(texture_sampler, var_texcoord0.xy) * var_tint_pm;
}
