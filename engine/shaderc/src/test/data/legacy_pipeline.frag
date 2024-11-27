#version 140

out vec4 color_out;

struct Light
{
    int type;
    vec3 position;
    vec4 color;
};

layout(set=1) uniform uniforms {
	Light u_test;
	Light u_test2;
	Light u_lights[4];
};

void main()
{
    color_out = u_test.color + u_lights[0].color + u_lights[3].color;
}
