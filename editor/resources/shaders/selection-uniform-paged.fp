#version 140

in mediump vec2 var_texcoord0;
in mediump float var_page_index;

uniform fragment_uniforms {
    lowp vec4 id_color;
};

uniform mediump sampler2DArray texture_sampler;

out vec4 out_color;

void main() {
    float alpha = texture(texture_sampler, vec3(var_texcoord0, var_page_index)).a;

    if (alpha > 0.05) {
        out_color = id_color;
    } else {
        discard;
    }
}
