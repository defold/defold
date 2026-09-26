#version 450
layout(location = 0) in vec3 vertex_color;
layout(set = 0, binding = 0) uniform textureCube cubemap;
layout(set = 0, binding = 1) uniform sampler cube_sampler;
layout(location = 0) out vec4 fragment_color;
void main() { fragment_color = textureLod(samplerCube(cubemap, cube_sampler), vertex_color, 0.0); }
