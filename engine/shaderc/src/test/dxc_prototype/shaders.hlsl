cbuffer Params : register(b0, space1)
{
    float4 tint;
    float4x4 transform;
};
Texture2D<float4> textures[2] : register(t0, space2);
SamplerState texture_sampler : register(s0, space2);
RWStructuredBuffer<float4> output_buffer : register(u0, space3);

float4 vs_main(float4 position : POSITION) : SV_Position
{
    return mul(transform, position) + tint;
}

float4 ps_main(float4 position : SV_Position) : SV_Target
{
    return textures[0].Sample(texture_sampler, position.xy) * tint
         + textures[1].Sample(texture_sampler, position.xy);
}

[numthreads(8, 1, 1)]
void cs_main(uint3 id : SV_DispatchThreadID)
{
    output_buffer[id.x] = textures[0].Load(int3(id.x, 0, 0)) * tint;
}
