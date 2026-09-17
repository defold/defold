#version 140

in highp vec3 var_position;
in lowp vec4 var_color;
in mediump vec3 var_normal;
in mediump vec2 var_texcoord0;

out vec4 out_color;

void main() {
    // Light calculations.
    vec3 surface_normal = normalize(var_normal);
    vec3 surface_to_eye_dir = normalize(-var_position);
    float light_intensity = clamp(dot(surface_normal, surface_to_eye_dir), 0.0, 1.0);

    // Visualize UVs using a checkerboard pattern.
    vec2 posterized_texcoord0 = floor(var_texcoord0 * 32.0);
    float checker_pattern = mod(posterized_texcoord0.x + posterized_texcoord0.y, 2.0);
    float checker_intensity = mix(1.0, 0.96, checker_pattern);

    // Composite into final color.
    float intensity = light_intensity * checker_intensity;
    out_color = vec4(var_color.rgb * intensity, var_color.a);
}
