varying mediump vec2 var_texcoord0;
varying lowp vec4 var_color;
varying lowp float var_page_index;

uniform lowp sampler2DArray texture_sampler;

void main()
{
    lowp vec4 tex = texture2DArray(texture_sampler, vec3(var_texcoord0.xy, var_page_index));
    gl_FragColor = tex * var_color;
}
