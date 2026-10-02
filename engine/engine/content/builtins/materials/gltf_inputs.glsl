#ifndef DEFOLD_PBR_INPUTS
#define DEFOLD_PBR_INPUTS

// Fragment inputs only. Lighting bindings are supplied by gltf_lights.glsl.
in highp vec4 var_position;
in mediump vec3 var_normal;
in mediump vec2 var_texcoord0;
in mediump vec4 var_color;
in mediump vec3 var_tangent;
in mediump vec3 var_bitangent;
in mediump float var_has_tangent;
in highp mat4 var_view;

#endif
