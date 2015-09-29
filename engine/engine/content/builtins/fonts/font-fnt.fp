varying mediump vec4 position;
varying mediump vec2 var_texcoord0;
varying lowp vec4 var_face_color;
varying lowp vec4 var_outline_color;
varying lowp vec4 var_shadow_color;

uniform lowp vec4 texture_size_recip;
uniform lowp sampler2D texture;

void main()
{
    gl_FragColor = texture2D(texture, var_texcoord0.xy) * var_face_color * var_face_color.a;
}
