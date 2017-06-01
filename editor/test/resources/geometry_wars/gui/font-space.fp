varying mediump vec4 var_pos;
varying mediump vec2 var_texcoord0;
varying lowp vec4 var_face_color;
varying lowp vec4 var_outline_color;
varying lowp vec4 var_shadow_color;
varying mediump vec4 var_sdf_params;
uniform lowp sampler2D texture;
uniform lowp vec4 texture_size_recip;
uniform mediump vec4 config;

mediump float sample_df(mediump vec2 where)
{
    return texture2D(texture, where.xy).x;
}

mediump float scale_sample(mediump float v)
{
    return v * var_sdf_params.x + var_sdf_params.y;
}

mediump vec2 eval_borders(mediump vec2 where)
{
    mediump float df = scale_sample(sample_df(where));
    mediump vec2 borders = vec2(clamp((df - var_sdf_params.z), 0.0, var_sdf_params.w), clamp(df, 0.0, var_sdf_params.w));
    return borders * borders * (3.0 - 2.0 * borders);
}

void main_supersample()
{
    mediump vec2 dtex = vec2(0.5 * texture_size_recip.xy / var_sdf_params.x);
    // sample 4 points around var_texcoord. the sample pattern will not be fixed in texture coordinate space,
    // and it is assumed the text is uniformly scaled.
    mediump vec4 dt = vec4(vec2(var_texcoord0 - dtex), vec2(var_texcoord0 + dtex));
    mediump vec2 borders = 2.0 * eval_borders(var_texcoord0)
                   + eval_borders(dt.xy) // upper left
                   + eval_borders(dt.xw) // bottom left
                   + eval_borders(dt.zy) // upper right
                   + eval_borders(dt.zw); // bottom right

    borders = (1.0/6.0) * borders;
    gl_FragColor = mix(mix(var_face_color, var_outline_color, borders.y), vec4(0), borders.x);
}

vec3 hsv2rgb(vec3 c)
{
    vec4 K = vec4(1.0, 2.0 / 3.0, 1.0 / 3.0, 3.0);
    vec3 p = abs(fract(c.xxx + K.xyz) * 6.0 - K.www);
    return c.z * mix(K.xxx, clamp(p - K.xxx, 0.0, 1.0), c.y);
}

void main_default()
{
    lowp vec2 borders = eval_borders(var_texcoord0);
    vec4 outline = vec4(hsv2rgb(vec3(config.x + var_pos.x, 1.0, 1.0)), 1.0) * var_outline_color.w;
    gl_FragColor = mix(mix(vec4(0, 0, 0, 0), outline, borders.y), vec4(0), borders.x);
}

void main()
{
      main_default();
}
