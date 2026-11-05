#version 430

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

struct Data
{
    vec4 member1;
};

layout (std430, binding = 0) writeonly buffer Test
{
    Data my_data[];
};

void main()
{
    my_data[1].member1 = vec4(0.0, 1.0, 0.0, 1.0);
}
