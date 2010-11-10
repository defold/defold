struct pixel_in
{
    float4 position : POSITION;
    float4 color : COLOR;
};

void main(pixel_in IN,
          out float4 oColor : COLOR)
{
    oColor = IN.color;
}
