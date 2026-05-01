#version 140

in mediump vec2 var_texcoord0;
in mediump vec4 var_face_color;
in mediump vec4 var_outline_color;
in mediump vec4 var_shadow_color;
in mediump vec4 var_sdf_params;
in mediump vec4 var_layer_mask;

out vec4 out_fragColor;

uniform mediump sampler2D texture_sampler;
uniform fs_uniforms
{
    mediump vec4 texture_size_recip;
};

vec3 sample_df(vec2 where)
{
    return texture(texture_sampler, where).xyz;
}

void main()
{
    mediump float sdf_edge      = var_sdf_params.x;
    mediump float sdf_outline   = var_sdf_params.y;
    mediump float sdf_smoothing = var_sdf_params.z;
    mediump float sdf_shadow    = var_sdf_params.w;

    // sample 4 points around var_texcoord0
    mediump vec2 dtex = vec2(0.5 * texture_size_recip.xy);
    mediump vec4 dt = vec4(vec2(var_texcoord0 - dtex), vec2(var_texcoord0 + dtex));
    mediump vec3 df_sample = 2.0 * (sample_df(var_texcoord0))
                   + sample_df(dt.xy) // upper left
                   + sample_df(dt.xw) // bottom left
                   + sample_df(dt.zy) // upper right
                   + sample_df(dt.zw); // bottom right
    df_sample = (1.0 / 6.0) * df_sample;

    mediump float distance        = df_sample.x;
    mediump float distance_shadow = df_sample.z;

    // If there is no blur, the shadow should behave in the same way as the outline.
    mediump float sdf_shadow_as_outline = floor(sdf_shadow);
    // If this is a single layer font, we must make sure to not mix alpha between layers.
    mediump float sdf_is_single_layer   = var_layer_mask.a;

    mediump float face_alpha      = smoothstep(sdf_edge - sdf_smoothing, sdf_edge + sdf_smoothing, distance);
    mediump float outline_alpha   = smoothstep(sdf_outline - sdf_smoothing, sdf_outline + sdf_smoothing, distance);
    mediump float shadow_alpha    = smoothstep(sdf_shadow - sdf_smoothing, sdf_edge + sdf_smoothing, distance_shadow);

    shadow_alpha = mix(shadow_alpha,outline_alpha,sdf_shadow_as_outline);

    out_fragColor = face_alpha * var_face_color * var_layer_mask.x +
      outline_alpha * var_outline_color * var_layer_mask.y * (1.0 - face_alpha * sdf_is_single_layer) +
      shadow_alpha * var_shadow_color * var_layer_mask.z * (1.0 - min(1.0,outline_alpha + face_alpha) * sdf_is_single_layer);
}
