varying mediump vec4 position;
varying mediump vec2 var_texcoord0;
varying lowp vec4 var_color;

uniform lowp sampler2D texture0;

void main() {
	// Pre-multiply alpha since all runtime textures already are
	lowp vec4 color_pm = vec4(var_color.xyz * var_color.w, var_color.w);
	gl_FragColor = texture2D(texture0, var_texcoord0.xy) * color_pm;
}
