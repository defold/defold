vec4 premultiply_alpha(vec4 color) {
	return vec4(color.rgb * color.a, color.a);
}
