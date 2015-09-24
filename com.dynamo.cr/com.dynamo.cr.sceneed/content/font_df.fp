varying vec2 var_texcoord0;

uniform sampler2D texture;

uniform vec4 uni_sdf_params;
uniform vec4 uni_face_color;
uniform vec4 uni_outline_color;

float sample_df(vec2 where)
{
    return texture2D(texture, where.xy).x;
}

float scale_sample(float v)
{
    return v * uni_sdf_params.x + uni_sdf_params.y;
}


vec2 eval_borders(vec2 where)
{
    float df = scale_sample(sample_df(where));
    vec2 borders = vec2(clamp((df - uni_sdf_params.z), 0.0, uni_sdf_params.w), clamp(df, 0.0, uni_sdf_params.w));
    return borders * borders * (3.0 - 2.0 * borders);
}

void main()
{
    vec2 borders = eval_borders(var_texcoord0);
    gl_FragColor = mix(mix(uni_face_color, uni_outline_color, borders.y), vec4(0.0), borders.x);
}

/*
varying mediump vec4 position;
varying mediump vec2 var_texcoord0;
varying lowp vec4 var_face_color;
varying lowp vec4 var_outline_color;
varying lowp vec4 var_shadow_color;
varying mediump vec4 var_sdf_params;
uniform lowp sampler2D texture;
uniform lowp vec4 texture_size_recip;

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

void main_default()
{
    lowp vec2 borders = eval_borders(var_texcoord0);
    gl_FragColor = mix(mix(var_face_color, var_outline_color, borders.y), vec4(0), borders.x);
}

void main()
{
      main_default();
}
*/