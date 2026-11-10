#ifndef DEFOLD_PBR_VERTEX
#define DEFOLD_PBR_VERTEX

#include "/builtins/materials/gltf_common.glsl"

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
    // Reuse one blended skin matrix for the position and tangent frame.
    mat4 skin = get_skin_matrix();
    local_position = skin * local_position;
    linear_transform = linear_transform * mat3(skin);

    // Normals need the inverse transpose when bones have nonuniform scale.
    // The cofactor matrix gives that direction without inverse(); normalization
    // removes its determinant magnitude, and skin_sign corrects reflections.
    mat3 skin_linear = mat3(skin);
    mat3 skin_cofactor = mat3(cross(skin_linear[1], skin_linear[2]),
                             cross(skin_linear[2], skin_linear[0]),
                             cross(skin_linear[0], skin_linear[1]));
    float skin_sign = dot(skin_linear[0], skin_cofactor[0]) < 0.0 ? -1.0 : 1.0;
    local_normal = skin_cofactor * normal * skin_sign;
#endif

    vec4 view_position = model_view * local_position;
    vec3 view_normal = pbr_normalize(mat3(mtx_normal) * local_normal, vec3(0.0, 0.0, 1.0));

    // Tangents follow the position transform, then must be made perpendicular
    // to the transformed normal. Check validity before supplying a fallback.
    vec3 view_tangent = linear_transform * tangent.xyz;
    view_tangent -= view_normal * dot(view_normal, view_tangent);
    bool has_tangent = dot(view_tangent, view_tangent) > PBR_EPSILON * PBR_EPSILON
                    && abs(tangent.w) > PBR_EPSILON;
    view_tangent = pbr_normalize(view_tangent, vec3(1.0, 0.0, 0.0));

    // tangent.w stores the mesh's UV handedness. A negative determinant adds
    // another reflection, so carry both signs into the interpolated bitangent.
    float transform_determinant = dot(linear_transform[0], cross(linear_transform[1], linear_transform[2]));
    float transform_sign = transform_determinant < 0.0 ? -1.0 : 1.0;

    var_position = view_position;
    var_normal = view_normal;
    var_tangent = view_tangent;
    var_bitangent = cross(view_normal, view_tangent) * tangent.w * transform_sign;
    var_has_tangent = has_tangent ? 1.0 : 0.0;
    var_texcoord0 = texcoord0;
    var_color = color;
    var_view = mtx_view;
    gl_Position = mtx_proj * view_position;
}

#endif
