#version 450

in vec2 var_texcoord;
out vec4 outColor;

void main()
{
	outColor = vec4(var_texcoord, 0.0, 1.0);
}
