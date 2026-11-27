#version 450
layout(location = 0) in vec4 position;
layout(location = 1) in vec4 offset;
void main()
{
    // Consume all 32 bytes per vertex, including vertices that are clipped.
    gl_Position = position + offset;
}
