#version 140

// Positions can be world or local space, since world and normal
// matrices are identity for world vertex space materials.
// If world vertex space is selected, you can remove the
// normal matrix multiplication for optimal performance.

in highp vec4 position;
in mediump vec2 texcoord0;
in mediump vec3 normal;

// When 'mtx_world' and 'mtx_normal' is specified as attributes,
// instanced rendering is automatically triggered.
// To read more about instancing in Defold, navigate to
// https://defold.com/manuals/material/#instancing
in mediump mat4 mtx_world;
in mediump mat4 mtx_normal;

out highp vec4 var_position;
out mediump vec3 var_normal;
out mediump vec2 var_texcoord0;
out mediump vec4 var_light;

uniform vs_uniforms
{
    mediump mat4 mtx_view;
    mediump mat4 mtx_proj;
    mediump vec4 light;
};

void main()
{
    vec4 p = mtx_view * mtx_world * vec4(position.xyz, 1.0);
    var_light = mtx_view * vec4(light.xyz, 1.0);
    var_position = p;
    var_texcoord0 = texcoord0;
    var_normal = normalize((mtx_normal * vec4(normal, 0.0)).xyz);
    gl_Position = mtx_proj * p;
}
