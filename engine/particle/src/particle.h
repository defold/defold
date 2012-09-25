#ifndef DM_PARTICLE_H
#define DM_PARTICLE_H

#include <vectormath/cpp/vectormath_aos.h>
#include <dlib/configfile.h>
#include <ddf/ddf.h>
#include "particle/particle_ddf.h"

using namespace Vectormath::Aos;

/**
 * System to handle particle systems
 */
namespace dmParticle
{
    /**
     * Context handle
     */
    typedef struct Context* HContext;
    /**
     * Prototype handle
     */
    typedef struct Prototype* HPrototype;
    /**
     * Instance handle
     */
    typedef uint32_t HInstance;

    /**
     * Invalid context handle
     */
    const HContext INVALID_CONTEXT = 0;
    /**
     * Invalid instance handle
     */
    const HInstance INVALID_INSTANCE = 0;

    /// Config key to use for tweaking maximum number of instances in a context.
    extern const char* MAX_INSTANCE_COUNT_KEY;
    /// Config key to use for tweaking the total maximum number of particles in a context.
    extern const char* MAX_PARTICLE_COUNT_KEY;

    /**
     * Callback to handle rendering of instances
     */
    typedef void (*RenderInstanceCallback)(void* usercontext, void* material, void* texture, uint32_t vertex_index, uint32_t vertex_count);
    /**
     * Callback to handle rendering of lines for debug purposes
     */
    typedef void (*RenderLineCallback)(void* usercontext, Vectormath::Aos::Point3 start, Vectormath::Aos::Point3 end, Vectormath::Aos::Vector4 color);

#define DM_PARTICLE_PROTO(ret, name,  ...) \
    \
    ret name(__VA_ARGS__);\
    extern "C" DM_DLLEXPORT ret Particle_##name(__VA_ARGS__);

    DM_PARTICLE_PROTO(HContext, Foo, int x);

    /**
     * Create a context.
     * @param render_context Context for use when rendering.
     * @param max_instance_count Max number of instances
     * @param max_particle_count Max number of particles
     * @return Context handle, or INVALID_CONTEXT when out of memory.
     */
    DM_PARTICLE_PROTO(HContext, CreateContext, uint32_t max_instance_count, uint32_t max_particle_count);
    /**
     * Destroy a context.
     * @param context Context to destroy. This will also destroy any remaining instances.
     */
    DM_PARTICLE_PROTO(void, DestroyContext, HContext context);

    /**
     * Create an instance from the supplied path and fetch resources using the supplied factory.
     * @param context Context in which to create the instance, must be valid.
     * @param prototype Prototype of the instance to be created
     * @return Instance handle, or INVALID_INSTANCE when the resource is broken or the context is full.
     */
    DM_PARTICLE_PROTO(HInstance, CreateInstance, HContext context, HPrototype prototype);
    /**
     * Destroy instance in the specified context.
     * @param context Context handle, must be valid.
     * @param instance Instance to destroy, must not be valid.
     */
    DM_PARTICLE_PROTO(void, DestroyInstance, HContext context, HInstance instance);
    /**
     * Start the specified instance, which means it will start spawning particles.
     * @param context Context in which the instance exists.
     * @param instance Instance to start, must not be valid.
     */
    DM_PARTICLE_PROTO(void, StartInstance, HContext context, HInstance instance);
    /**
     * Stop the specified instance, which means it will stop spawning particles.
     * Any spawned particles will still be simulated until they die.
     * @param context Context in which the instance exists.
     * @param instance Instance to start, must not be valid.
     */
    DM_PARTICLE_PROTO(void, StopInstance, HContext context, HInstance instance);
    /**
     * Restart the specified instance, which means it will start spawning particles again.
     * Any already living particles will remain after this call.
     * @param context Context in which the instance exists.
     * @param instance Instance to restart, must not be valid.
     */
    DM_PARTICLE_PROTO(void, RestartInstance, HContext context, HInstance instance);
    /**
     * Create and start a new instance. Once the instance is sleeping (@see IsSleeping), the particle system will automatically destroy it.
     * Instances with looping emitters can not be created this way since they would never be destroyed in that case.
     * @param context Context in which the instance exists.
     * @param prototype Prototype of the instance to be created.
     * @param path Path of the instance resource.
     * @param position Position of the created instance.
     * @param rotation Rotation of the created instance.
     */
    DM_PARTICLE_PROTO(void, FireAndForget, HContext context, HPrototype prototype, Point3 position, Quat rotation);

    /**
     * Set the position of the specified instance.
     * @param context Context in which the instance exists.
     * @param instance Instance to set position for.
     * @param position Position in world space.
     */
    DM_PARTICLE_PROTO(void, SetPosition, HContext context, HInstance instance, Point3 position);
    /**
     * Set the position of the specified instance.
     * @param context Context in which the instance exists.
     * @param instance Instance to set rotation for.
     * @param rotation Rotation in world space.
     */
    DM_PARTICLE_PROTO(void, SetRotation, HContext context, HInstance instance, Quat rotation);

    /**
     * Returns if the specified instance is spawning particles or not.
     * A looping instance is always spawning particles, other instances are spawning until their duration has elapsed.
     */
    DM_PARTICLE_PROTO(bool, IsSpawning, HContext context, HInstance instances);
    /**
     * Returns if the specified instance is spawning particles or not.
     * A looping instance is never sleeping, other instances are sleeping when they are not spawning and have no remaining living particles.
     */
    DM_PARTICLE_PROTO(bool, IsSleeping, HContext context, HInstance instance);

    /**
     * Update the instances within the specified context.
     * @param context Context of the instances to update.
     * @param dt Time step.
     * @param vertex_buffer Vertex buffer into which to store the particle vertex data. If this is 0x0, no rendering will occur.
     * @param vertex_buffer_size Size in bytes of the supplied vertex buffer.
     * @param out_vertex_buffer_size How many bytes was actually written to the vertex buffer, 0x0 is allowed.
     */
    DM_PARTICLE_PROTO(void, Update, HContext context, float dt, float* vertex_buffer, uint32_t vertex_buffer_size, uint32_t* out_vertex_buffer_size);

    /**
     * Render the instances within the specified context.
     * @param context Context of the instances to render.
     */
    DM_PARTICLE_PROTO(void, Render, HContext context, void* user_context, RenderInstanceCallback render_instance_callback);
    /**
     * Debug render the status of the instances within the specified context.
     * @param context Context of the instances to render.
     * @param RenderLine Function pointer to use to render the lines.
     */
    DM_PARTICLE_PROTO(void, DebugRender, HContext context, void* user_context, RenderLineCallback render_line_callback);

    extern "C"
    {
        // Currently custom for java bindings. Might be changed with the new interface
        HPrototype Particle_NewPrototype(HContext context, void* instance_data, uint32_t instance_data_size);
        void Particle_DeletePrototype(HContext context, HPrototype prototype);
    }

#undef DM_PARTICLE_PROTO

}

#endif // DM_PARTICLE_H
