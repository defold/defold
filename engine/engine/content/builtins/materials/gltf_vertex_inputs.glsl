#ifndef DEFOLD_PBR_VERTEX_INPUTS
#define DEFOLD_PBR_VERTEX_INPUTS

in highp vec4 position;
in mediump vec3 normal;
in mediump vec4 tangent;
in mediump vec4 color;
in mediump vec2 texcoord0;

#ifdef PBR_SKINNED
in mediump vec4 bone_weights;
in mediump vec4 bone_indices;
#endif

#ifdef PBR_INSTANCED
in highp mat4 mtx_world;
in mediump mat4 mtx_normal;

#ifdef PBR_SKINNED
in highp vec4 animation_data;
#endif // PBR_SKINNED
#endif // PBR_INSTANCED

out highp vec4 var_position;
out mediump vec3 var_normal;
out mediump vec2 var_texcoord0;
out mediump vec4 var_color;
out mediump vec3 var_tangent;
out mediump vec3 var_bitangent;
out mediump float var_has_tangent;
out highp mat4 var_view;

uniform vs_uniforms
{
    highp mat4 mtx_view;
    highp mat4 mtx_proj;
#ifndef PBR_INSTANCED
    highp mat4 mtx_worldview;
    mediump mat4 mtx_normal;
#ifdef PBR_SKINNED
    highp vec4 animation_data;
#endif // PBR_SKINNED
#endif // PBR_INSTANCED
};

#endif
