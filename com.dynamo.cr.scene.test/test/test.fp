struct Fragment
{
    float4 color : COLOR;
};

void main(in Fragment f_in,
          out float4 c_out)
{
    c_out = f_in.color;
}
