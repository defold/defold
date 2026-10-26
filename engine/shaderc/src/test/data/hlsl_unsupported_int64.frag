#version 450
#extension GL_ARB_gpu_shader_int64 : require

layout(binding = 0) uniform uniforms
{
    int64_t value;
};

layout(location = 0) out vec4 color;

void main()
{
    color = vec4(float(value));
}
