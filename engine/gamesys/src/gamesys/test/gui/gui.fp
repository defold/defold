varying mediump vec4 position;
varying mediump vec2 var_texcoord0;
varying lowp vec4 var_color;

uniform lowp sampler2D texture;

void main()
{
    lowp vec4 tex = texture2D(texture, var_texcoord0.xy);
    gl_FragColor = tex * var_color;
}
