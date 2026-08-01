#version 140

in mediump vec2 var_texcoord0;
in mediump vec4 var_face_color;
in mediump vec4 var_outline_color;
in mediump vec4 var_shadow_color;
in mediump vec4 var_sdf_params;
in mediump vec4 var_layer_mask;

out vec4 out_fragColor;

uniform mediump sampler2D texture_sampler;

void main()
{
    mediump vec4 df_sample = texture(texture_sampler, var_texcoord0);

    mediump float distance        = df_sample.x;
    mediump float distance_shadow = df_sample.z;

    mediump float sdf_edge      = var_sdf_params.x;
    mediump float sdf_outline   = var_sdf_params.y;
    mediump float sdf_smoothing = var_sdf_params.z;
    mediump float sdf_shadow    = var_sdf_params.w;

    // Multi-layer glyphs are emitted as separate, one-hot masked quads. Avoid
    // evaluating the other two layers, and reject the transparent part of the
    // padded glyph quad before doing the smoothing and color work.
    if (var_layer_mask.a < 0.5)
    {
        mediump float layer_distance;
        mediump float layer_edge;
        mediump vec4 layer_color;

        if (var_layer_mask.x > 0.5)
        {
            layer_distance = distance;
            layer_edge = sdf_edge;
            layer_color = var_face_color;
        }
        else if (var_layer_mask.y > 0.5)
        {
            layer_distance = distance;
            layer_edge = sdf_outline;
            layer_color = var_outline_color;
        }
        else
        {
            // A shadow threshold of one denotes the unblurred outline-style
            // path, which uses the regular distance channel.
            bool shadow_as_outline = sdf_shadow >= 1.0;
            layer_distance = shadow_as_outline ? distance : distance_shadow;
            layer_edge = shadow_as_outline ? sdf_edge : sdf_shadow;
            layer_color = var_shadow_color;
        }

        mediump float layer_min = layer_edge - sdf_smoothing;
        if (layer_distance <= layer_min)
            discard;

        mediump float layer_alpha = smoothstep(layer_min, layer_edge + sdf_smoothing, layer_distance);
        out_fragColor = layer_alpha * layer_color;
        return;
    }

    // The face is the topmost single-layer contribution. Once it is fully
    // covered, outline and shadow are completely occluded by the existing
    // compositing equation.
    if (distance >= sdf_edge + sdf_smoothing)
    {
        out_fragColor = var_face_color;
        return;
    }

    // If there is no blur, the shadow should behave in the same way as the outline.
    mediump float sdf_shadow_as_outline = floor(sdf_shadow);
    // If this is a single layer font, we must make sure to not mix alpha between layers.
    mediump float sdf_is_single_layer   = var_layer_mask.a;

    mediump float face_alpha      = smoothstep(sdf_edge - sdf_smoothing, sdf_edge + sdf_smoothing, distance);
    mediump float outline_alpha   = smoothstep(sdf_outline - sdf_smoothing, sdf_outline + sdf_smoothing, distance);
    mediump float shadow_alpha    = smoothstep(sdf_shadow - sdf_smoothing, sdf_edge + sdf_smoothing, distance_shadow);

    shadow_alpha = mix(shadow_alpha, face_alpha, sdf_shadow_as_outline);

    mediump vec4 glyph_color = face_alpha * var_face_color * var_layer_mask.x +
        outline_alpha * var_outline_color * var_layer_mask.y * (1.0 - face_alpha * sdf_is_single_layer) +
        shadow_alpha * var_shadow_color * var_layer_mask.z * (1.0 - min(1.0,outline_alpha + face_alpha) * sdf_is_single_layer);

    if (glyph_color.a <= 0.0)
        discard;

    out_fragColor = glyph_color;
}
