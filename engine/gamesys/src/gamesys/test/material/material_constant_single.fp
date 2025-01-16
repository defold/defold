uniform lowp vec4 tint;

struct nested_type
{
    vec4 tint;
};

uniform nested_type nested;

void main()
{
    gl_FragColor = tint + nested.tint;
}
