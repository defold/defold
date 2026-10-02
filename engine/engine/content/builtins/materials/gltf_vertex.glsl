#ifndef DEFOLD_PBR_VERTEX
#define DEFOLD_PBR_VERTEX

#include "/builtins/materials/gltf_common.glsl"

#ifdef PBR_SKINNED

// glTF-only helper: fetch each bone once for position, normal and tangent.
mat4 get_pbr_skin_matrix()
{
#ifndef EDITOR
    if (animation_data.y > 0.0)
    {
        return get_bone_matrix(int(bone_indices.x)) * bone_weights.x
             + get_bone_matrix(int(bone_indices.y)) * bone_weights.y
             + get_bone_matrix(int(bone_indices.z)) * bone_weights.z
             + get_bone_matrix(int(bone_indices.w)) * bone_weights.w;
    }
#endif
    return mat4(1.0);
}
#endif

void main()
{
#ifdef PBR_INSTANCED
    mat4 model_view = mtx_view * mtx_world;
#else
    mat4 model_view = mtx_worldview;
#endif
    vec4 local_position = vec4(position.xyz, 1.0);
    mat3 linear_transform = mat3(model_view);
    vec3 local_normal = normal;
#ifdef PBR_SKINNED
    mat4 skin = get_pbr_skin_matrix();
    local_position = skin * local_position;
    linear_transform = linear_transform * mat3(skin);
    // Cofactor matrix also handles nonuniform bone scale without inverse().
    mat3 skin_linear = mat3(skin);
    mat3 skin_cofactor = mat3(cross(skin_linear[1], skin_linear[2]),
                             cross(skin_linear[2], skin_linear[0]),
                             cross(skin_linear[0], skin_linear[1]));
    float skin_sign = dot(skin_linear[0], skin_cofactor[0]) < 0.0 ? -1.0 : 1.0;
    local_normal = skin_cofactor * normal * skin_sign;
#endif
    vec4 p = model_view * local_position;
    vec3 n = pbr_normalize(mat3(mtx_normal) * local_normal, vec3(0.0, 0.0, 1.0));
    vec3 t = linear_transform * tangent.xyz;
    t -= n * dot(n, t);
    bool has_tangent = dot(t, t) > PBR_EPSILON * PBR_EPSILON && abs(tangent.w) > PBR_EPSILON;
    t = pbr_normalize(t, vec3(1.0, 0.0, 0.0));
    float transform_sign = dot(linear_transform[0], cross(linear_transform[1], linear_transform[2])) < 0.0 ? -1.0 : 1.0;
    var_position = p;
    var_normal = n;
    var_tangent = t;
    var_bitangent = cross(n, t) * tangent.w * transform_sign;
    var_has_tangent = has_tangent ? 1.0 : 0.0;
    var_texcoord0 = texcoord0;
    var_color = color;
    var_view = mtx_view;
    gl_Position = mtx_proj * p;
}

#endif
