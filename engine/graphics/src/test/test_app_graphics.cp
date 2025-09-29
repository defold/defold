
#version 430

layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(rgba32f) uniform image2D texture_out;

uniform buf
{
	vec4 color;
};

void main() {
    ivec2 tex_coord   = ivec2(gl_GlobalInvocationID.xy);
    vec4 output_value = vec4(0.0, 0.0, 0.0, 1.0);
    output_value.x    = float(tex_coord.x) / gl_NumWorkGroups.x;
    output_value.y    = float(tex_coord.y) / gl_NumWorkGroups.y;

    output_value.rg *= color.rg;
    output_value.b   = color.b;

    imageStore(texture_out, tex_coord, output_value);
}
