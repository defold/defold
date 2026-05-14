#version 140

in highp vec4 var_color;

out vec4 out_fragColor;

void main()
{
    out_fragColor = var_color;
    out_fragColor.a = 1.0;
}
