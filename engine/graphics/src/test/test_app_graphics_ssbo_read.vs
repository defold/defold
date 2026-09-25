#version 430
layout(location = 0) in vec2 pos;
layout(location = 0) out vec4 varColor;
struct Data { vec4 member1; };
layout(std430, binding = 0) readonly buffer Test { Data my_data[]; };

void main()
{
    gl_Position = vec4(pos, 0, 1);
    varColor = vec4(my_data[1].member1.rgb, 1);
}
