#version 140

in highp vec3 var_world_position;
in mediump vec3 var_world_normal;

uniform fragment_uniforms {
    highp vec3 camera_position;
};

uniform mediump samplerCube environment_sampler;

out vec4 out_color;

void main() {
    vec3 camera_to_vertex = normalize(var_world_position - camera_position);
    vec3 reflection_direction = reflect(camera_to_vertex, var_world_normal);
    out_color = texture(environment_sampler, reflection_direction);
}
