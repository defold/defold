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

vec2 get_alphas(float distance)
{
    lowp float sdf_edge = var_sdf_params.x;
    lowp float sdf_outline = var_sdf_params.y;
    lowp float sdf_smoothing = var_sdf_params.z;

    lowp float alpha = smoothstep(sdf_edge - sdf_smoothing, sdf_edge + sdf_smoothing, distance);
    lowp float outline_alpha = smoothstep(sdf_outline - sdf_smoothing, sdf_outline + sdf_smoothing, distance);

    return vec2(alpha, outline_alpha);
}

void main()
{
    lowp vec2 dtex = vec2(0.5 * texture_size_recip.xy);
    // sample 4 points around var_texcoord0
    lowp vec4 dt = vec4(vec2(var_texcoord0 - dtex), vec2(var_texcoord0 + dtex));
    lowp float dist = 2.0 * (sample_df(var_texcoord0))
                   + sample_df(dt.xy) // upper left
                   + sample_df(dt.xw) // bottom left
                   + sample_df(dt.zy) // upper right
                   + sample_df(dt.zw); // bottom right
    dist = (1.0 / 6.0) * dist;
    lowp vec2 alphas = get_alphas(dist);
    lowp vec4 color = mix(var_outline_color, var_face_color, alphas.x);
    gl_FragColor = color * alphas.y;
}