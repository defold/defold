
layout (local_size_x = 1, local_size_y = 1, local_size_z = 1) in;

layout(rgba32f) uniform image2D texture_a;
uniform sampler2D texture_b;
uniform sampler2D texture_c;

uniform vec4 buffer_a;
uniform vec4 buffer_b;
uniform mat4 buffer_c;
uniform mat4 buffer_d;

void main()
{
    ivec2 tex_coord   = ivec2(gl_GlobalInvocationID.xy);
    vec4 sample_b     = texture(texture_b, tex_coord);
    vec4 sample_c     = texture(texture_c, tex_coord);
    vec4 output_value = buffer_a + buffer_b + buffer_c[0] + buffer_d[0] + sample_b + sample_c;
    imageStore(texture_a, tex_coord, output_value);
}
