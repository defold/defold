varying mediump vec4 position;
varying mediump vec2 var_texcoord0;
varying lowp vec4 var_color;

uniform lowp sampler2D DIFFUSE_TEXTURE;
uniform lowp vec4 tint;

void main()
{
    // Pre-multiply alpha since all runtime textures already are
    lowp vec4 color = tint * var_color;
    lowp vec4 color_pm = vec4(color.xyz * color.w, color.w);
    gl_FragColor = texture2D(DIFFUSE_TEXTURE, var_texcoord0.xy) * color_pm;
}
