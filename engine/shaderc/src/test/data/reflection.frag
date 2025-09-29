#version 140

uniform sampler2D tex;

in vec2 TexCoord;

uniform ubo_global_one
{
    vec4 color;
    mat4 mvp;
};

uniform ubo_global_two
{
    vec4 color_two;
    vec4 color_array[4];
};

uniform ubo_inst
{
    vec4 color;
} inst_variable;

out vec4 FragColor;

void main()
{
    vec4 out_color = texture(tex, TexCoord);
    out_color *= color;
    out_color *= color_two;
    out_color *= inst_variable.color;
    out_color += color_array[3];

    FragColor = out_color;
}
