varying mediump vec2 var_texcoord0;
varying lowp vec4 var_face_color;
varying lowp vec4 var_outline_color;
varying lowp vec4 var_shadow_color;
varying lowp vec3 var_layer_mask;
varying lowp float var_is_single_layer;

uniform lowp vec4 texture_size_recip;
uniform lowp sampler2D texture;

void main()
{
    lowp vec3 t      = texture2D(texture, var_texcoord0.xy).xyz;
    float face_alpha = t.x * var_face_color.w;
    gl_FragColor     = (var_layer_mask.x * face_alpha * vec4(var_face_color.xyz, 1.0) +
        var_layer_mask.y * vec4(var_outline_color.xyz, 1.0) * var_outline_color.w * t.y * (1.0 - face_alpha * var_is_single_layer) +
        var_layer_mask.z * vec4(var_shadow_color.xyz, 1.0) * var_shadow_color.w * t.z * (1.0 - clamp(0.0, 1.0, t.x + t.y) * var_is_single_layer));
}
