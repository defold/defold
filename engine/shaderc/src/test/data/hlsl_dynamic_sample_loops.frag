#version 450

layout(binding = 0) uniform uniforms
{
    ivec4 counts;
};
layout(binding = 1) uniform sampler2D tex;
layout(location = 0) in vec2 uv;
layout(location = 0) out vec4 color;

void main()
{
    float sum = 0.0;
    for (int i = 0; i < counts.x; ++i)
    {
        if (uv.x < float(i) * 0.1)
            continue;
        for (int j = 0; j < counts.y; ++j)
        {
            for (int k = 0; k < counts.z; ++k)
                sum += texture(tex, uv + vec2(j, k) * 0.001).r;
        }
        if (sum > float(counts.w))
            break;
    }
    color = vec4(sum);
}
