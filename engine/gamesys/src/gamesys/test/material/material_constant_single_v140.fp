#version 140

struct type_nested_nested
{
    vec4 tint;
    vec4 leaf_value;
    vec4 overridden_value;
};

struct type_nested
{
    vec4               tint;
    vec4               overridden_value;
    type_nested_nested nested;
};

uniform ubo
{
    vec4        tint;
    vec4        overridden_value;
    type_nested nested;
};

out vec4 FragColor;

void main()
{
    FragColor = tint + nested.tint + nested.nested.tint;
}
