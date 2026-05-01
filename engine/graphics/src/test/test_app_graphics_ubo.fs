#version 430

struct LightColor
{
    vec3  color;
    float intensity;
};
struct Light
{
    vec3       position;
    LightColor light_color;
};
uniform LightData
{
    Light lights[4];
    float light_count;
};

out vec4 FragColor;

void main()
{
    vec4 c0 = vec4(lights[0].light_color.color, lights[0].light_color.intensity);
    vec4 c1 = vec4(lights[1].light_color.color, lights[1].light_color.intensity);
    vec4 c2 = vec4(lights[2].light_color.color, lights[2].light_color.intensity);
    vec4 c3 = vec4(lights[3].light_color.color, lights[3].light_color.intensity);

    FragColor = c0 + c1 + c2 + c3;
}
