#version 140

in mediump vec2 var_texcoord0;
in mediump vec4 var_face_color;
in mediump vec4 var_outline_color;
in mediump vec4 var_sdf_params;

uniform mediump sampler2D texture_sampler;

uniform fs_uniforms
{
    mediump vec4 texture_size_recip;
};

out vec4 out_fragColor;

float sample_df(vec2 where)
{
    return texture(texture_sampler, where).x;
}

void main()
{
    mediump float sdf_edge = var_sdf_params.x;
    mediump float sdf_outline = var_sdf_params.y;
    mediump float sdf_smoothing = var_sdf_params.z;

    // sample 4 points around var_texcoord0
    mediump vec2 dtex = vec2(0.5 * texture_size_recip.xy);
    mediump vec4 dt = vec4(vec2(var_texcoord0 - dtex), vec2(var_texcoord0 + dtex));
    mediump float distance = 2.0 * (sample_df(var_texcoord0))
                   + sample_df(dt.xy) // upper left
                   + sample_df(dt.xw) // bottom left
                   + sample_df(dt.zy) // upper right
                   + sample_df(dt.zw); // bottom right
    distance = (1.0 / 6.0) * distance;

    mediump float alpha = smoothstep(sdf_edge - sdf_smoothing, sdf_edge + sdf_smoothing, distance);
    mediump float outline_alpha = smoothstep(sdf_outline - sdf_smoothing, sdf_outline + sdf_smoothing, distance);
    mediump vec4 color = mix(var_outline_color, var_face_color, alpha);

    out_fragColor = color * outline_alpha;
}
