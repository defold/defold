struct pixel_in 
{
    float4 position : POSITION;
};

void main(pixel_in IN, 
          out float4 color     : COLOR,
          uniform float4 frag_color : C0)
{
    color = frag_color;
}
