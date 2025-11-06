#version 140

in mediump vec2 var_texcoord0;
in mediump float var_page_index;
in lowp vec4 var_color;

uniform mediump sampler2DArray texture_sampler;

out vec4 out_color;

void main() {
    vec4 texture_color = texture(texture_sampler, vec3(var_texcoord0, var_page_index));
    out_color = texture_color * var_color;
}
