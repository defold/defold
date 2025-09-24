#version 140

in mediump vec2 var_texcoord0;
in lowp vec4 var_id_color;

uniform mediump sampler2D tex0;

out vec4 out_color;

void main() {
    float alpha = texture(tex0, var_texcoord0).a;

    if (alpha > 0.05) {
        out_color = var_id_color;
    } else {
        discard;
    }
}
