struct pixel_in
{
    float4 position : POSITION;
    float4 color : COLOR0;
    float2 texcoord : TEXCOORD0;
};

void main(pixel_in IN, uniform sampler2D texture : TEXUNIT0,
          out float4 oColor     : COLOR)
{
    float3 t = tex2D(texture, IN.texcoord.xy).xyz;
    oColor.xyz = float3(t.x,t.x,t.x) * IN.color.xyz;
    oColor.w = t.x * IN.color.w;
}
