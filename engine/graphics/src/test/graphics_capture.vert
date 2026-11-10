#version 450
layout(location = 0) in vec3 position;
layout(location = 1) in vec3 color;
layout(location = 0) out vec3 vertex_color;
void main()
{
    // Match production SPIR-V: viewport orientation is handled by the backend.
    gl_Position = vec4(position, 1.0);
#ifdef CAPTURE_OPENGL
    gl_Position.z = position.z * 2.0 - 1.0;
#endif
    vertex_color = color;
}
