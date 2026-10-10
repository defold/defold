#version 140

in mediump vec2 var_texcoord0;

uniform mediump sampler2D texture_sampler;

uniform fragment_uniforms {
    lowp vec4 tint;
};

out vec4 out_color;

void main() {
    out_color = texture(texture_sampler, var_texcoord0) * tint;
}
