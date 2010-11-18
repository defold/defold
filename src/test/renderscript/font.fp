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
    shadow_color.w *= t.z;
    float4 glyph_color = outline_color;
    glyph_color.w *= t.y;
    glyph_color.xyz = glyph_color.xyz * (1 - t.x) + face_color.xyz * t.x;
    glyph_color.w += face_color.w * t.x;
    out_color = glyph_color + shadow_color;
}
