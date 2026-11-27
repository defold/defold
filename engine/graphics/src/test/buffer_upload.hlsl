float4 vs_main(float4 position : TEXCOORD0, float4 offset : TEXCOORD1) : SV_Position
{
    return position + offset;
}

float4 ps_main() : SV_Target0
{
    return float4(1, 1, 1, 1);
}
