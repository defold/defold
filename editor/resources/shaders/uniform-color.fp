#version 140

uniform fragment_uniforms {
    lowp vec4 color;
};

out vec4 out_color;

void main() {
    out_color = color;
}
