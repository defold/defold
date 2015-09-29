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
