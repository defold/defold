varying vec4 position;
varying vec2 var_texcoord0;

uniform sampler2D original;
uniform sampler2D post1;
uniform sampler2D post2;
uniform sampler2D post3;
uniform sampler2D post4;
uniform sampler2D light;

void main()
{
    float glow_rate = 0.7;
    float light_rate = 0.2;

	vec4 sum_post = vec4(0.0);
    sum_post += texture2D(post1, var_texcoord0.xy);
    sum_post += texture2D(post2, var_texcoord0.xy);
    sum_post += texture2D(post3, var_texcoord0.xy);
    sum_post += texture2D(post4, var_texcoord0.xy);
    vec4 color = texture2D(original, var_texcoord0.xy);
    vec4 v_light = texture2D(light, var_texcoord0.xy);
    gl_FragColor = color + glow_rate * clamp(sum_post, vec4(0.0), vec4(1.0)) + v_light * light_rate;
    gl_FragColor.w = clamp(gl_FragColor.r + gl_FragColor.g + gl_FragColor.b, 0.0, 1.0);
}
