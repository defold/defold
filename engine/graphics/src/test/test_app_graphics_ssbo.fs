#version 430

layout (location = 0) out vec4 outColor;

struct Data
{
	vec4 member1;
};

layout (std430, binding = 0) readonly buffer Test
{
	Data my_data[];
};

void main()
{
	outColor = vec4(my_data[1].member1.rgb, 1.0);
}
