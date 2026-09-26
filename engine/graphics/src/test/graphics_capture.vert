#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 0) out vec3 vertex_color;
void main() {
    // Vulkan clip-space Y runs down; the test's coordinates run up.
    gl_Position = vec4(position.x, -position.y, position.z, 1.0);
    vertex_color = color;
}
