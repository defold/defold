#version 430

struct Data
{
    vec4 member1;
};

layout(std430, set = 2, binding = 3) readonly buffer Test
{
    Data my_data_one;
    Data my_data_two[];
};

out vec4 color_out;

void main()
{
    color_out = my_data_one.member1 + my_data_two[0].member1;
}
