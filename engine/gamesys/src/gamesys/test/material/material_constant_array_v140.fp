#version 140

out vec4 FragColor;

struct nested_nested_type
{
    vec4 tint[2];
    vec4 leaf_value[2];
    vec4 overridden_value[2];
};

struct nested_type
{
    vec4               tint[2];
    vec4               overridden_value[2];
    nested_nested_type nested;
};

uniform fs_uniforms
{
    vec4        tint[2];
    vec4        overridden_value[2];
    nested_type nested;
};

void main()
{
    FragColor = tint[1] + nested.tint[1] + nested.nested.tint[1];
}
