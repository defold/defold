#version 450
layout(location = 0) in vec3 vertex_color;
layout(set = 0, binding = 0) uniform texture2D test_texture;
layout(set = 0, binding = 1) uniform sampler test_sampler;
layout(location = 0) out vec4 fragment_color;
void main() { fragment_color = vec4(textureLod(sampler2D(test_texture, test_sampler), vertex_color.xy, 0.0).rgb, 1.0); }
