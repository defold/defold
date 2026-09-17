#version 140

struct ColorIntensity
{
    vec3  color;
    float intensity;
};

struct Light
{
    vec3           position;
    ColorIntensity color_intensity;
};

uniform LightData
{
    Light lights[4];
    float light_count;
};

out vec4 FragColor;

void main()
{
    vec4 c0 = vec4(lights[0].color_intensity.color, lights[0].color_intensity.intensity);
    vec4 c1 = vec4(lights[1].color_intensity.color, lights[1].color_intensity.intensity);
    vec4 c2 = vec4(lights[2].color_intensity.color, lights[2].color_intensity.intensity);
    vec4 c3 = vec4(lights[3].color_intensity.color, lights[3].color_intensity.intensity);

    FragColor = c0 + c1 + c2 + c3;
}


