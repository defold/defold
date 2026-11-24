#version 140

uniform sampler2D font_effects;
out vec4 color;

void main()
{
    color = texture(font_effects, vec2(0.5));
}
