#version 140

precision highp float;
precision highp int;

in highp vec2 var_texcoord;
in mediump vec4 var_color;
in highp vec2 var_sdf_texcoord;
flat in highp vec4 var_effect_params;
flat in highp vec4 var_params;
flat in highp float var_layer_mode;

out vec4 out_fragColor;

uniform mediump sampler2D sdf_texture;

const float SDF_EDGE = 0.75;

void main()
{
    float sdf_sample = texture(sdf_texture, var_sdf_texcoord).r;
    float sdf_outline = max(var_effect_params.x, 1.0 / 255.0);
    float sdf_shadow = var_effect_params.y;
    float sdf_spread = max(var_params.w, 0.0001);
    vec2 source_pixel_footprint = fwidth(var_texcoord) * max(var_params.xy, vec2(0.0001));
    float sdf_smoothing = 0.25 * max(source_pixel_footprint.x,
                                     source_pixel_footprint.y) / sdf_spread;

    // The outline is the dilated glyph silhouette. The analytical vector face
    // is drawn over it in the following render object, so the interior does not
    // need to be subtracted here.
    if (abs(var_layer_mode - 1.0) < 0.5)
    {
        float outline_alpha = smoothstep(sdf_outline - sdf_smoothing,
                                         sdf_outline + sdf_smoothing,
                                         sdf_sample);
        if (outline_alpha <= 0.0)
        {
            discard;
        }
        float alpha = var_color.a * outline_alpha;
        out_fragColor = vec4(var_color.rgb * alpha, alpha);
        return;
    }

    // Match the legacy dynamic-font shadow channel for blurred shadows. A hard
    // shadow still uses this SDF pass, but thresholds the raw field at the face
    // edge so an authored outline does not enlarge its silhouette.
    float blurred_shadow = step(1.0, var_effect_params.w);
    float remapped_shadow_sample = min(sdf_sample / sdf_outline, 1.0) * SDF_EDGE;
    float shadow_sample = mix(sdf_sample, remapped_shadow_sample, blurred_shadow);
    float shadow_edge = mix(SDF_EDGE, sdf_shadow, blurred_shadow);
    float shadow_alpha = smoothstep(shadow_edge - sdf_smoothing,
                                    SDF_EDGE + sdf_smoothing,
                                    shadow_sample);
    if (shadow_alpha <= 0.0)
    {
        discard;
    }

    float alpha = var_color.a * shadow_alpha;
    out_fragColor = vec4(var_color.rgb * alpha, alpha);
}
