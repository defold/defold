struct pixel_in
{
    float4 position : POSITION;
    float3 normal : TEXCOORD0;
    float2 texcoord : TEXCOORD1;
};

void main(pixel_in IN, uniform sampler2D texture : TEXUNIT0,
          out float4 color     : COLOR,
          uniform float4 diffuse_color : C0,
          uniform float4 emissive_color : C1,
          uniform float4 specular_color : C2)
{
    float4 normal = float4(IN.normal.x, IN.normal.y, IN.normal.z, 0);
    float light = dot(IN.normal, float3(1, 1, 1));
    float4 tex = tex2D(texture, IN.texcoord.xy) * light;
    color = (tex + diffuse_color);

#if 0
    float Power = 1;
    float4 N, L, SpecColor, LightColor, H;
    L = float4(1, 0, 0, 0);
    N = float4(IN.normal.x, IN.normal.y, IN.normal.z, 0);
    H = (L + N) * 0.5;
    SpecColor = float4(1, 0, 0, 0);
    LightColor = float4(0.4, 0.4, 0.3, 0);
    color = tex * saturate( dot (N, L) ) * Power * pow ( dot ( N, H), Power) * SpecColor * LightColor;
#endif
}
