struct pixel_in 
{
    float4 position : POSITION;
    float3 color : COLOR0;
    float2 texcoord : TEXCOORD0;
    float3 normal : TEXCOORD1;
};

void main(pixel_in IN, uniform sampler2D texture : TEXUNIT0,
          out float4 color     : COLOR,
          uniform float4 frag_color : C0)
{
    float3 c = 1; //tex2D(texture, IN.texcoord.xy).xyz * dot(IN.normal, normalize(float3(0.7, 0.5, 1)) );
    color.xyz = frag_color;
}
