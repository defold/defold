#version 140

in lowp vec4 var_color;
in highp vec3 var_eye_space_position;

uniform fragment_uniforms {
    lowp vec4 fog_color;
    highp vec4 fog_parameters;
};

out vec4 out_color;

void main() {
    float fog_start = fog_parameters.x;
    float fog_end = fog_parameters.y;
    float fog_enabled = fog_parameters.z;
    float fog_range = max(fog_end - fog_start, 0.000001);
    // Match fixed-function fog's eye-to-fragment distance instead of its optional abs(z) approximation.
    float fog_distance = length(var_eye_space_position);
    float fog_factor = clamp((fog_end - fog_distance) / fog_range, 0.0, 1.0);
    fog_factor = mix(1.0, fog_factor, fog_enabled);
    out_color = vec4(mix(fog_color.rgb, var_color.rgb, fog_factor), var_color.a);
}
