varying mediump vec2 var_texcoord0;

uniform lowp sampler2D a;
uniform lowp sampler2D b;
uniform lowp vec4 asdf;

void main()
{
    // Pre-multiply alpha since all runtime textures already are
    lowp vec4 tint_pm = vec4(asdf.xyz * asdf.w, asdf.w);
    gl_FragColor = mix(
	  texture2D(a, vec2(0.9, 0.9) - var_texcoord0.xy) * tint_pm,
	  texture2D(b, vec2(1.0, 1.0) - var_texcoord0.xy) * tint_pm,
	  0.5
	);
	gl_FragColor = texture2D(b, var_texcoord0.xy) * tint_pm;
}
