#version 140

in vec4 position;
in vec3 normal;
in vec2 tex_coord;

uniform matrices
{
    mat4 mtx_world;
    mat4 mtx_view;
    mat4 mtx_projection;
    mat4 mtx_normal;
};

uniform extra
{
    vec4 offset;
} extra_variable;

out vec2 oTexcoord;
out vec3 oNormal;
out vec4 oPositionWorld;

void main()
{
    gl_Position = mtx_projection * mtx_view * mtx_world * position + extra_variable.offset;

    oTexcoord = tex_coord;
    oNormal = (mtx_normal * vec4(normal, 0.0)).xyz;
    oPositionWorld = mtx_world * position;
}
