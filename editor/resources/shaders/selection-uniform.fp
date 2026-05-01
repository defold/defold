#version 140

in mediump vec2 var_texcoord0;

uniform fragment_uniforms {
    lowp vec4 id_color;
};

uniform mediump sampler2D texture_sampler;

out vec4 out_color;

void main() {
    float alpha = texture(texture_sampler, var_texcoord0).a;

    if (alpha > 0.05) {
        out_color = id_color;
    } else {
        discard;
    }
}
