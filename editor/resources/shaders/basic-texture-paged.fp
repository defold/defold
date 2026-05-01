#version 140

in mediump vec2 var_texcoord0;
in mediump float var_page_index;

uniform mediump sampler2DArray texture_sampler;

out vec4 out_color;

void main() {
    out_color = texture(texture_sampler, vec3(var_texcoord0, var_page_index));
}
