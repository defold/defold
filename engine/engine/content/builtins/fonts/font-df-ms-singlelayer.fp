varying lowp vec2 var_texcoord0;
varying lowp vec4 var_face_color;
varying lowp vec4 var_outline_color;
varying lowp vec4 var_sdf_params;

uniform mediump sampler2D texture;
uniform lowp vec4 texture_size_recip;

float sample_df(vec2 where)
{
    return texture2D(texture, where).x;
}

void main()
{
    lowp float sdf_edge = var_sdf_params.x;
    lowp float sdf_outline = var_sdf_params.y;
    lowp float sdf_smoothing = var_sdf_params.z;

    // sample 4 points around var_texcoord0
    lowp vec2 dtex = vec2(0.5 * texture_size_recip.xy);
    lowp vec4 dt = vec4(vec2(var_texcoord0 - dtex), vec2(var_texcoord0 + dtex));
    lowp float distance = 2.0 * (sample_df(var_texcoord0))
                   + sample_df(dt.xy) // upper left
                   + sample_df(dt.xw) // bottom left
                   + sample_df(dt.zy) // upper right
                   + sample_df(dt.zw); // bottom right
    distance = (1.0 / 6.0) * distance;

    lowp float alpha = smoothstep(sdf_edge - sdf_smoothing, sdf_edge + sdf_smoothing, distance);
    lowp float outline_alpha = smoothstep(sdf_outline - sdf_smoothing, sdf_outline + sdf_smoothing, distance);
    lowp vec4 color = mix(var_outline_color, var_face_color, alpha);

    gl_FragColor = color * outline_alpha;
}