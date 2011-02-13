struct pixel_in
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

void main(pixel_in IN,
          uniform sampler2D texture : TEXUNIT0,
          uniform float4 diffuse_color : C0,
          out float4 oColor : COLOR)
{
    float4 tex = tex2D(texture, IN.texcoord.xy);
    oColor = tex * diffuse_color;
}
