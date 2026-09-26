#version 450
layout(location = 0) in vec3 vertex_color;
layout(location = 0) out vec4 fragment_color;
void main() { fragment_color = vec4(vertex_color, 1.0); }
