#version 140

uniform fragment_uniforms {
    lowp vec4 id_color;
};

out vec4 out_color;

void main() {
    out_color = id_color;
}
