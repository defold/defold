varying vec2 var_texcoord0;

uniform sampler2D texture;

uniform vec4 uni_sdf_params;
uniform vec4 uni_face_color;
uniform vec4 uni_outline_color;

float sdf_edge = uni_sdf_params.x;
float sdf_outline = uni_sdf_params.y;
float sdf_smoothing = uni_sdf_params.z;

float sample_df(vec2 where)
{
    return texture2D(texture, where.xy).x;
}

vec2 get_alphas(float distance)
{
    float alpha = smoothstep(sdf_edge - sdf_smoothing, sdf_edge + sdf_smoothing, distance);
    float outline_alpha = smoothstep(sdf_outline - sdf_smoothing, sdf_outline + sdf_smoothing, distance);
    return vec2(alpha, outline_alpha);
}

void main_default()
{
    vec2 alphas = get_alphas(sample_df(var_texcoord0));
    vec4 color = mix(uni_outline_color, uni_face_color, alphas.x);
    gl_FragColor = color * alphas.y;
}

void main()
{
    main_default();
}