varying mediump vec2 var_texcoord0;
varying lowp vec4 var_face_color;

uniform lowp sampler2D texture_sampler;

void main()
{
    gl_FragColor = texture2D(texture_sampler, var_texcoord0.xy) * var_face_color * var_face_color.a;
}
