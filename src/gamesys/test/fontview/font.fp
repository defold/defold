struct pixel_in
{
    float4 position : POSITION;
    float4 color : COLOR0;
    float2 texcoord : TEXCOORD0;
};

void main(pixel_in IN, uniform sampler2D texture : TEXUNIT0,
          out float4 oColor     : COLOR)
{
    float x = tex2D(texture, IN.texcoord.xy).x;
    oColor.xyz = IN.color.xyz;
    oColor.w = x * x * IN.color.w;
}
