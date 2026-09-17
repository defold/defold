uniform sampler2D      u_sampler2d;
uniform samplerCube    u_samplerCube;
uniform sampler2DArray u_sampler2dArray;

layout(rgba32f) uniform image2D texture_out;

uniform vec4           u_user;
uniform mat4           u_user_mat4;
uniform vec4           u_user_arr[2];
uniform mat4           u_user_mat4_arr[2];

void main()
{
    vec4 sample_a     = texture(u_sampler2d, vec2(0.0));
    vec4 sample_b     = texture(u_samplerCube, vec3(0.0));
    vec4 sample_c     = texture(u_sampler2dArray, vec3(0.0));
    vec4 output_value = u_user + u_user_mat4[0] + u_user_arr[1] + u_user_mat4_arr[1][0] + sample_a + sample_b + sample_c;

    ivec2 tex_coord = ivec2(gl_GlobalInvocationID.xy);
    imageStore(texture_out, tex_coord, output_value);
}
