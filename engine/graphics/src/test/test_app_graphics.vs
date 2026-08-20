#version 450

in vec2 pos;
in vec2 texcoord;

out vec2 var_texcoord;

void main()
{
	gl_Position = vec4(pos, 0.0, 1.0);
	var_texcoord = texcoord;
}
