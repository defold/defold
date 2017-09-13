varying vec2 var_texcoord0;

uniform sampler2D texture;

uniform vec4 uni_face_color;
uniform vec4 uni_outline_color;
uniform vec4 uni_sdf_params;

void main()
{
    float distance = texture2D(texture, var_texcoord0).x;

    float sdf_edge = uni_sdf_params.x;
    float sdf_outline = uni_sdf_params.y;
    float sdf_smoothing = uni_sdf_params.z;
    
    float alpha = smoothstep(sdf_edge - sdf_smoothing, sdf_edge + sdf_smoothing, distance);
    float outline_alpha = smoothstep(sdf_outline - sdf_smoothing, sdf_outline + sdf_smoothing, distance);
    vec4 color = mix(uni_outline_color, uni_face_color, alpha);

    gl_FragColor = color * outline_alpha;
}