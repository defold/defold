#version 140

in vec3 var_normal;
in vec2 var_uv;
in vec4 var_color;

out vec4 color_out;

void main()
{
    color_out = var_normal.rgbb + var_uv.stst + var_color;
}
