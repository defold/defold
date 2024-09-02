varying float var_page_index;
varying vec2  var_custom_one;
varying vec3  var_custom_two;
varying vec4  var_custom_three;

uniform sampler2D      u_sampler2d;
uniform samplerCube    u_samplerCube;
uniform sampler2DArray u_sampler2dArray;

uniform vec4           u_user;
uniform mat4           u_user_mat4;
uniform vec4           u_user_arr[2];
uniform mat4           u_user_mat4_arr[2];

void main()
{
    vec4 u_sampler2d_val = texture2D(u_sampler2d, vec2(0.0, 0.0));
    vec4 u_samplerCube_val = textureCube(u_samplerCube, vec3(0.0, 0.0, 0.0));
    vec4 u_sampler2dArray_val = texture2DArray(u_sampler2dArray, vec3(0.0, 0.0, 0.0), 0.0);

    gl_FragColor = vec4(0.0);
    gl_FragColor.r = var_page_index + var_custom_one.x + var_custom_two.x + var_custom_three.x;
    gl_FragColor.g = u_user.x + u_user_mat4[0][0] + u_user_arr[1].x + u_user_mat4_arr[1][0][0];
    gl_FragColor.b = u_sampler2d_val.r + u_samplerCube_val.r + u_sampler2dArray_val.r;
}
