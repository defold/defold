#version 140

in mediump vec2 var_texcoord0;
in mediump vec4 var_face_color;
in mediump vec4 var_outline_color;
in mediump vec4 var_sdf_params;

out vec4 out_fragColor;

uniform mediump sampler2D texture_sampler;

void main()
{
    mediump float distance = texture(texture_sampler, var_texcoord0).x;

    mediump float sdf_edge = var_sdf_params.x;
    mediump float sdf_outline = var_sdf_params.y;
    mediump float sdf_smoothing = var_sdf_params.z;

    mediump float alpha = smoothstep(sdf_edge - sdf_smoothing, sdf_edge + sdf_smoothing, distance);
    mediump float outline_alpha = smoothstep(sdf_outline - sdf_smoothing, sdf_outline + sdf_smoothing, distance);
    mediump vec4 color = mix(var_outline_color, var_face_color, alpha);

    out_fragColor = color * outline_alpha;
}
