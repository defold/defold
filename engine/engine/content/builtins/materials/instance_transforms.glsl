#ifndef DEFOLD_INSTANCE_TRANSFORMS
#define DEFOLD_INSTANCE_TRANSFORMS

// Define DM_USE_VISIBLE_INSTANCES to opt in to the visible-ID variant.
// Include after #version 430 or later. The material must mark this attribute as
// an instanced world matrix. Override its name before including this file.
#ifndef DM_INSTANCE_WORLD_ATTRIBUTE
#define DM_INSTANCE_WORLD_ATTRIBUTE mtx_world
#endif

#if defined(DM_USE_VISIBLE_INSTANCES) && defined(DM_GPU_VISIBLE_INSTANCES)
// Descriptor set 3 is reserved for the renderer's persistent instance inputs.
layout(std430, set = 3, binding = 0) readonly buffer _dm_visible_worlds
{
    mat4 _dm_instance_worlds[];
};
layout(std430, set = 3, binding = 1) readonly buffer _dm_visible_ids
{
    uint _dm_instance_ids[];
};
#else
in highp mat4 DM_INSTANCE_WORLD_ATTRIBUTE;
#endif

mat4 dm_instance_world()
{
#if defined(DM_USE_VISIBLE_INSTANCES) && defined(DM_GPU_VISIBLE_INSTANCES)
    uint visible_id = _dm_instance_ids[gl_InstanceIndex];
    return _dm_instance_worlds[visible_id];
#else
    return DM_INSTANCE_WORLD_ATTRIBUTE;
#endif
}

#endif
