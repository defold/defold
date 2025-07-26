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

vec4 get_skinned_position(vec4 local_position)
{
#ifdef EDITOR
    // Editor does not support skinned mesh previews yet
    return local_position;
#else
    vec4 skinned_position = vec4(0.0);
    if (animation_data.y > 0.0) {
        vec4 base_pos = vec4(position.xyz, 1.0);
        // for (int i = 0; i < 4; i++)
        // {
        //     int bone_index    = int(bone_indices[i]);
        //     mat4 bone_matrix  = get_bone_matrix(bone_index);
        //     skinned_position += bone_weights[i] * (bone_matrix * base_pos);
        // }
        // Manually unrolled loop for compatibility with WebGL1 and OpenGL ES 2.0.
        int  bone_index0   = int(bone_indices.x);
        mat4 bone_matrix0  = get_bone_matrix(bone_index0);
        skinned_position  += bone_weights.x * (bone_matrix0 * base_pos);

        int  bone_index1   = int(bone_indices.y);
        mat4 bone_matrix1  = get_bone_matrix(bone_index1);
        skinned_position  += bone_weights.y * (bone_matrix1 * base_pos);

        int  bone_index2   = int(bone_indices.z);
        mat4 bone_matrix2  = get_bone_matrix(bone_index2);
        skinned_position  += bone_weights.z * (bone_matrix2 * base_pos);

        int  bone_index3   = int(bone_indices.w);
        mat4 bone_matrix3  = get_bone_matrix(bone_index3);
        skinned_position  += bone_weights.w * (bone_matrix3 * base_pos);
    }
    else
    {
        skinned_position = position;
    }
    return skinned_position;
#endif
}

#endif
