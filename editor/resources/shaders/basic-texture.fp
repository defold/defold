#version 140

in mediump vec2 var_texcoord0;

uniform mediump sampler2D texture_sampler;

out vec4 out_color;

void main() {
    out_color = texture(texture_sampler, var_texcoord0);
}
