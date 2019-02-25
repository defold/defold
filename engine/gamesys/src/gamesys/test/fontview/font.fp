varying mediump vec2 var_texcoord0;
varying lowp vec4 var_face_color;
varying lowp vec4 var_outline_color;

uniform lowp vec4 texture_size_recip;
uniform lowp sampler2D texture_sampler;

void main()
{
    // Outline
    lowp vec2 t = texture2D(texture_sampler, var_texcoord0.xy).xy;
    gl_FragColor = mix(vec4(var_face_color.xyz, t.x), var_outline_color, t.y);
}
