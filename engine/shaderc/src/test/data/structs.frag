#version 140

out vec4 FragColor;

uniform sampler2D base_type_sampler2D;
uniform samplerCube base_type_samplerCube;

struct nested_struct
{
    vec4 nested_member;
};

struct base_struct
{
    vec4          member;
    nested_struct nested;
};

uniform fs_uniforms
{
    vec4        base_type_vec4;
    vec3        base_type_vec3;
    vec2        base_type_vec2;
    float       base_type_float;
    int         base_type_int;
    bool        base_type_bool;
    base_struct base;
};

void main()
{
    FragColor = texture(base_type_sampler2D, vec2(0.0, 0.0));
    FragColor += texture(base_type_samplerCube, vec3(0.0, 0.0, 0.0));
    FragColor += base.member + base.nested.nested_member + base_type_vec4;
}
