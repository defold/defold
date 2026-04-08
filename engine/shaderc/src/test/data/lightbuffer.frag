#version 140

#define MAX_LIGHTS 8

uniform Light
{
	vec4 position;
    vec4 color;
    vec4 direction_range;
    vec4 params;
} lights[MAX_LIGHTS];

uniform fs_uniforms
{
	vec4 lights_count;
};

out vec4 FragColor;

void main()
{
    FragColor = lights[0].color + lights[7].color + lights_count;
}
