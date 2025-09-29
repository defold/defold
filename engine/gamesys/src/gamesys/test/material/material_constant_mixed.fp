uniform lowp vec4 constant_v4;
uniform lowp vec4 constant_v4_array[8];

uniform lowp mat4 constant_m4;
uniform lowp mat4 constant_m4_array[2];

void main()
{
    gl_FragColor = vec4(1.0) + constant_v4 + constant_v4_array[7] + constant_m4[1] + constant_m4_array[1][1];
}
