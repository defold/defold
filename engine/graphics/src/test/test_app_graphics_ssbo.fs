#version 450

layout (location = 0) out vec4 outColor;

struct Data
{
	vec4 member1;
};

layout (binding = 0) buffer Test
{
	Data my_data[];
};

void main()
{
	vec3 color = my_data[0].member1.rgb;
	outColor = vec4(1.0 - color, 1.0);
}
