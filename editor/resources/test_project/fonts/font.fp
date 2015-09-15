struct pixel_in
{
    float4 position : POSITION;
    float2 texcoord : TEXCOORD0;
};

void main(pixel_in IN,
          uniform sampler2D texture : TEXUNIT0,
          uniform float4 face_color     : C0,
          uniform float4 outline_color  : C1,
          uniform float4 shadow_color   : C2,
          out float4 out_color          : COLOR)
{
    float4 t = tex2D(texture, IN.texcoord.xy);
    face_color.w *= t.x;
    outline_color.w *= t.y;
    shadow_color.w *= t.z;
    out_color = face_color + outline_color + shadow_color;
}
