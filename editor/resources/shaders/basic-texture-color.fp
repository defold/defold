#version 140

in mediump vec2 var_texcoord0;
in lowp vec4 var_color;

uniform mediump sampler2D texture_sampler;

out vec4 out_color;

void main() {
    vec4 texture_color = texture(texture_sampler, var_texcoord0);
    out_color = texture_color * var_color;
}
