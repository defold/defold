#version 430

layout(location = 0) out vec4 outColor;
struct Data { vec4 member1; };
layout(std430, binding = 0) writeonly buffer Test { Data my_data[]; };

void main()
{
    my_data[1].member1 = vec4(0, 1, 0, 1);
    outColor = vec4(1);
}
