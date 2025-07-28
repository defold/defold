#ifndef DEFOLD_SKINNING_GLSL
#define DEFOLD_SKINNING_GLSL

uniform sampler2D pose_matrix_cache;

vec2 get_bone_uv(vec2 cache_size, int index)
{
    int x = int(mod(index, int(cache_size.x)));
    int y = int(float(index) / cache_size.x);
    // Make sure we sample from the center of the pixel
    float u = (float(x) + 0.5) / cache_size.x;
    float v = (float(y) + 0.5) / cache_size.y;
    return vec2(u, v);
}

// Retrieve a bone matrix from the pose matrix cache
mat4 get_bone_matrix(int bone_index)
{
    int pose_matrix_index = bone_index * 3 + int(animation_data.x);
    vec2 cache_size       = animation_data.zw;

    // Read the 3 columns from the texture (we store only first 3 columns)
    vec4 col0 = texture(pose_matrix_cache, get_bone_uv(cache_size, pose_matrix_index + 0));
    vec4 col1 = texture(pose_matrix_cache, get_bone_uv(cache_size, pose_matrix_index + 1));
    vec4 col2 = texture(pose_matrix_cache, get_bone_uv(cache_size, pose_matrix_index + 2));

    // Reconstruct the 4x4 matrix by using the translation stored in W components
    return mat4(
        vec4(col0.xyz, 0.0),  // First column: rotation/scale
        vec4(col1.xyz, 0.0),  // Second column: rotation/scale
        vec4(col2.xyz, 0.0),  // Third column: rotation/scale
        vec4(col0.w, col1.w, col2.w, 1.0)  // Fourth column: translation + (0,0,0,1)
    );
}

vec4 apply_skin(mat4 bone_matrix, float weight, vec4 base_pos) {
    return weight * (bone_matrix * base_pos);
}

vec4 get_skinned_position(vec4 local_position)
{
#ifdef EDITOR
    // Editor does not support skinned mesh previews yet
    return local_position;
#else
    vec4 skinned_position = vec4(0.0);
    if (animation_data.y > 0.0) {
        vec4 base_pos = vec4(position.xyz, 1.0);
        // Manually unrolled loop for compatibility with WebGL1 and OpenGL ES 2.0.
        skinned_position += apply_skin(get_bone_matrix(int(bone_indices.x)), bone_weights.x, base_pos);
        skinned_position += apply_skin(get_bone_matrix(int(bone_indices.y)), bone_weights.y, base_pos);
        skinned_position += apply_skin(get_bone_matrix(int(bone_indices.z)), bone_weights.z, base_pos);
        skinned_position += apply_skin(get_bone_matrix(int(bone_indices.w)), bone_weights.w, base_pos);
    }
    else
    {
        skinned_position = position;
    }
    return skinned_position;
#endif
}
#endif
