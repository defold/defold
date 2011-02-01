struct Fragment
{
    float4 position : POSITION;
    float2 uv       : TEXCOORD0;
};

uniform sampler2D DIFFUSE_TEXTURE : TEXUNIT0;

void main(in Fragment f_in,
          out float4 c_out : COLOR)
{
    c_out = tex2D(DIFFUSE_TEXTURE, f_in.uv.xy);
}
