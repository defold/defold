#version 140

in mediump vec2 var_texcoord0;
in mediump float var_page_index;

uniform mediump sampler2DArray texture_sampler;

out vec4 out_color;

void main() {
    vec4 texture_color = texture(texture_sampler, vec3(var_texcoord0, var_page_index));
    vec3 linear_color = max(texture_color.rgb, vec3(0.0));
    vec3 mapped_color = linear_color / (linear_color + vec3(1.0));
    out_color = vec4(pow(mapped_color, vec3(1.0 / 2.2)), clamp(texture_color.a, 0.0, 1.0));
}
