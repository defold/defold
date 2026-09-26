#version 330
in vec3 vertex_color; // Sampling direction, passed through the shared vertex shader.
uniform samplerCube cubemap;
out vec4 fragment_color;
void main() { fragment_color = textureLod(cubemap, vertex_color, 0.0); }
