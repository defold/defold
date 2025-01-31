#version 140

in mediump vec2 var_texcoord0;
in mediump vec4 var_face_color;
in mediump vec4 var_outline_color;
in mediump vec4 var_shadow_color;
in mediump vec4 var_layer_mask;

out vec4 out_fragColor;

uniform mediump sampler2D texture_sampler;

void main()
{
    mediump float is_single_layer = var_layer_mask.a;
    mediump vec3 t                = texture(texture_sampler, var_texcoord0.xy).xyz;
    float face_alpha              = var_face_color.w * t.x;

    float raw_outline_alpha    = var_outline_color.w * t.y;
    float max_outline_alpha    = (1.0 - face_alpha * is_single_layer);
    float outline_alpha        = min(raw_outline_alpha, max_outline_alpha);

    float raw_shadow_alpha     = var_shadow_color.w * t.z;
    float max_shadow_alpha     = (1.0 - (face_alpha + outline_alpha) * is_single_layer);
    float shadow_alpha         = min(raw_shadow_alpha, max_shadow_alpha);

    mediump vec4 face_color    = var_layer_mask.x * vec4(var_face_color.xyz, 1.0)    * face_alpha;
    mediump vec4 outline_color = var_layer_mask.y * vec4(var_outline_color.xyz, 1.0) * outline_alpha;
    mediump vec4 shadow_color  = var_layer_mask.z * vec4(var_shadow_color.xyz, 1.0)  * shadow_alpha;

    out_fragColor = face_color + outline_color + shadow_color;
}
