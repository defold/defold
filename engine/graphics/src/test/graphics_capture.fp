#version 330
in vec3 vertex_color;
out vec4 fragment_color;
void main() { fragment_color = vec4(vertex_color, 1.0); }
