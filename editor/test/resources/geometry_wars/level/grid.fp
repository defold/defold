varying vec4 position;
varying vec2 var_texcoord0;

uniform sampler2D texture0;

void main()
{
	vec4 t = texture2D(texture0, var_texcoord0.xy);
	float c = 1.0;
	gl_FragColor = vec4(c, c, c, t.w);
}
