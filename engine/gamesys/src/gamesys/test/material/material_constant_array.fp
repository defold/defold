uniform lowp vec4 tint[2];

struct nested_type
{
    vec4 tint[2];
};

uniform nested_type nested;

void main()
{
    gl_FragColor = tint[1] + nested.tint[1];
}
