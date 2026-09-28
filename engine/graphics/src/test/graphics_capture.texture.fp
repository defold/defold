#version 330
in vec3 vertex_color; // UV coordinates, passed through the shared vertex shader.
uniform sampler2D test_texture;
out vec4 fragment_color;
void main() { fragment_color = vec4(textureLod(test_texture, vertex_color.xy, 0.0).rgb, 1.0); }
