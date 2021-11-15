varying mediump vec2 var_texcoord0;
uniform lowp vec4 uniform_array[16];

void main()
{
    gl_FragColor = uniform_array[15];
}
