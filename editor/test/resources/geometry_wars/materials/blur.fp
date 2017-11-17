uniform sampler2D T;
uniform vec4 offsets;
uniform vec4 coefficients;

varying vec2 var_texcoord0;

void main()
{
    vec4 sum = vec4(0.0);
    vec2 offset1 = offsets.xy;
    vec2 offset2 = offsets.zw;
    sum += texture2D(T, var_texcoord0 - offset1) * coefficients.x;
    sum += texture2D(T, var_texcoord0 - offset2) * coefficients.y;
    sum += texture2D(T, var_texcoord0) * coefficients.z;
    sum += texture2D(T, var_texcoord0 + offset2) * coefficients.y;
    sum += texture2D(T, var_texcoord0 + offset1) * coefficients.x;

    gl_FragColor = sum;
    gl_FragColor.w = 1.0;
}
