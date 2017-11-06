varying mediump vec2 var_texcoord0;
varying lowp vec4 var_color;

uniform lowp sampler2D DIFFUSE_TEXTURE;
uniform lowp vec4 tint;

void main()
{
    // Pre-multiply alpha since var_color and all runtime textures already are
    lowp vec4 tint_pm = vec4(tint.xyz * tint.w, tint.w);
    lowp vec4 color_pm = var_color * tint_pm;
    gl_FragColor = texture2D(DIFFUSE_TEXTURE, var_texcoord0.xy) * color_pm;
}
