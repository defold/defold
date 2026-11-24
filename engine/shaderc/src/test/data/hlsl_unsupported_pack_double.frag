#version 450

layout(binding = 0) uniform uniforms
{
    uvec2 value;
};

layout(location = 0) out vec4 color;

void main()
{
    // SPIRV-Cross cannot translate packed-double operations to HLSL, including SM6.
    color = vec4(float(packDouble2x32(value)));
}
