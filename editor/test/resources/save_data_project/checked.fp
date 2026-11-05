uniform lowp sampler2DArray texture0;

varying mediump vec4 position;
varying mediump vec3 var_texcoord0;
varying mediump vec4 var_color0;

void main() {
	vec4 texture0_color = texture2DArray(texture0, var_texcoord0);
	gl_FragColor = texture0_color * var_color0;
}
