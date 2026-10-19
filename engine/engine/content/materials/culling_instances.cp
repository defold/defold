#version 430
layout(local_size_x = 128, local_size_y = 1, local_size_z = 1) in;
layout(std430, binding = 0) readonly buffer Bounds { uint bounds_words[]; };
layout(std430, binding = 1) readonly buffer Worlds { float world_words[]; };
layout(std430, binding = 2) readonly buffer Slots { uint slots[]; };
layout(std430, binding = 3) writeonly buffer Instances { uint instance_words[]; };
layout(std430, binding = 4) buffer Arguments { uint arguments[]; };
layout(std140, binding = 5) uniform Constants
{
    vec4 frustum_planes[6];
    vec4 culling_params; // candidates, planes (zero disables culling), stride, world offset
    vec4 instance_params; // normal word offset (negative when absent), visible-ID output
    mat4 view;
};
vec3 read_vec3(uint offset)
{
    return vec3(uintBitsToFloat(bounds_words[offset]), uintBitsToFloat(bounds_words[offset + 1]), uintBitsToFloat(bounds_words[offset + 2]));
}
bool visible(uint offset, uint type, mat4 world)
{
    vec3 center = read_vec3(offset + 1);
    vec3 extents = read_vec3(offset + 4);
    vec3 edge0 = vec3(0.0);
    vec3 edge1 = vec3(0.0);
    vec3 edge2 = vec3(0.0);
    if (type == 3)
    {
        center = (world * vec4(center, 1.0)).xyz;
        // Half-edge vectors preserve negative/nonuniform scale and shear.
        edge0 = world[0].xyz * extents.x;
        edge1 = world[1].xyz * extents.y;
        edge2 = world[2].xyz * extents.z;
    }
    for (uint i = 0; i < uint(culling_params.y); ++i)
    {
        vec4 plane = frustum_planes[i];
        float distance = dot(plane.xyz, center) + plane.w;
        if (type == 1)
        {
            float radius_squared = uintBitsToFloat(bounds_words[offset + 4]);
            if (distance < 0.0 && distance * distance > radius_squared) return false;
        }
        else
        {
            float radius = type == 2 ? dot(abs(plane.xyz), extents)
                : abs(dot(plane.xyz, edge0)) + abs(dot(plane.xyz, edge1)) + abs(dot(plane.xyz, edge2));
            if (distance + radius < 0.0) return false;
        }
    }
    return true;
}
void main()
{
    uint index = gl_GlobalInvocationID.x;
    if (index >= uint(culling_params.x)) return;
    uint slot = slots[index];
    uint offset = slot * 8;
    uint type = bounds_words[offset];
    if (type == 0) return;
    // The same persistent transform supplies OBB culling and instance output.
    mat4 world;
    for (uint column = 0; column < 4; ++column)
        for (uint row = 0; row < 4; ++row)
            world[column][row] = world_words[slot * 16 + column * 4 + row];
    if (!visible(offset, type, world)) return;
    // Opaque batches may compact in any order; blending uses the CPU path.
    uint destination = atomicAdd(arguments[1], 1) * uint(culling_params.z);
    if (instance_params.y > 0.0)
    {
        instance_words[destination] = slot;
        return;
    }
    if (culling_params.w >= 0.0)
        for (uint i = 0; i < 16; ++i)
            instance_words[destination + uint(culling_params.w) + i] = floatBitsToUint(world[i / 4][i % 4]);
    if (instance_params.x >= 0.0)
    {
        mat4 normal = transpose(inverse(view * world));
        for (uint i = 0; i < 16; ++i)
            instance_words[destination + uint(instance_params.x) + i] = floatBitsToUint(normal[i / 4][i % 4]);
    }
}
