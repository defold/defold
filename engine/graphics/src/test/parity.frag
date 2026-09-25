#version 450
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;

void main()
{
    color = vec4(step(0.5, uv.x), step(0.5, uv.y), 1, 1);
}
