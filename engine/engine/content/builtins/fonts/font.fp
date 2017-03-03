varying mediump vec2 var_texcoord0;
varying lowp vec4 var_face_color;
varying lowp vec4 var_outline_color;
varying lowp vec4 var_shadow_color;

uniform lowp vec4 texture_size_recip;
uniform lowp sampler2D texture;

void main()
{
    // Outline
    lowp vec2 t = texture2D(texture, var_texcoord0.xy).xy;
    gl_FragColor = vec4(var_face_color.xyz, 1.0) * t.x * var_face_color.w + vec4(var_outline_color.xyz * t.y * var_outline_color.w, t.y * var_outline_color.w) * (1.0 - t.x);
}
