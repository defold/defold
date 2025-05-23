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
    int pose_matrix_index = bone_index * 4 + int(animation_data.x);
    vec2 cache_size       = animation_data.zw;
    return mat4(
        texture(pose_matrix_cache, get_bone_uv(cache_size, pose_matrix_index + 0)),
        texture(pose_matrix_cache, get_bone_uv(cache_size, pose_matrix_index + 1)),
        texture(pose_matrix_cache, get_bone_uv(cache_size, pose_matrix_index + 2)),
        texture(pose_matrix_cache, get_bone_uv(cache_size, pose_matrix_index + 3))
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
        for (int i = 0; i < 4; i++)
        {
            int bone_index    = int(bone_indices[i]);
            mat4 bone_matrix  = get_bone_matrix(bone_index);
            skinned_position += bone_weights[i] * (bone_matrix * vec4(position.xyz, 1.0));
        }
    }
    else
    {
        skinned_position = position;
    }
    return skinned_position;
#endif
}

#endif
