#version 140

uniform sampler2DArray font_effects;
out vec4 color;

void main()
{
    color = texture(font_effects, vec3(0.5, 0.5, 0.0))
          + texture(font_effects, vec3(0.5, 0.5, 1.0));
}
