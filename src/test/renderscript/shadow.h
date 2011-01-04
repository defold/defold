struct Vertex
{
    float4 position : ATTR0;
    float3 normal   : ATTR1;
    float2 uv       : ATTR2;
};

struct Fragment
{
    float4 position     : POSITION;
    float3 normal       : TEXCOORD0;
    float2 uv           : TEXCOORD1;
    float4 shadow_uv    : TEXCOORD2;
    float3 world_pos    : TEXCOORD3;
};
