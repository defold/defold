#version 450

layout (input_attachment_index = 0, binding = 0, set = 1) uniform usubpassInput inputColor;

layout (location = 0) out vec4 outColor;

void main()
{
	vec3 color = subpassLoad(inputColor).rgb;

	outColor = vec4(1.0 - color, 1.0);
}
