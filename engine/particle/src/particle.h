#ifndef DM_PARTICLE_H
#define DM_PARTICLE_H

#include <vectormath/cpp/vectormath_aos.h>
#include <dlib/configfile.h>
#include <dlib/hash.h>
#include <ddf/ddf.h>
#include "particle/particle_ddf.h"

/*extern "C"
{
#include <lua/lua.h>
#include <lua/lauxlib.h>
}*/

/**
 * System to handle particle systems
 */
namespace dmParticle
{
    using namespace Vectormath::Aos;

    /**
     * Context handle
     */
    typedef struct Context* HParticleContext;
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
    const HParticleContext INVALID_CONTEXT = 0;
    /**
     * Invalid prototype handle
     */
    const HPrototype INVALID_PROTOTYPE = 0;
    /**
     * Invalid instance handle
     */
    const HInstance INVALID_INSTANCE = 0;

    /// Config key to use for tweaking maximum number of instances in a context.
    extern const char* MAX_INSTANCE_COUNT_KEY;
    /// Config key to use for tweaking the total maximum number of particles in a context.
    extern const char* MAX_PARTICLE_COUNT_KEY;

    /**
     * Render constants supplied to the render callback.
     */
    struct RenderConstant
    {
        dmhash_t m_NameHash;
        Vector4 m_Value;
    };

    /**
     * Callback to handle rendering of emitters
     */
    typedef void (*RenderEmitterCallback)(void* usercontext, void* material, void* texture, const Vectormath::Aos::Matrix4& world_transform, dmParticleDDF::BlendMode blend_mode, uint32_t vertex_index, uint32_t vertex_count, RenderConstant* constants, uint32_t constant_count);
    /**
     * Callback to handle rendering of lines for debug purposes
     */
    typedef void (*RenderLineCallback)(void* usercontext, const Vectormath::Aos::Point3& start, const Vectormath::Aos::Point3& end, const Vectormath::Aos::Vector4& color);

    enum AnimPlayback
    {
        ANIM_PLAYBACK_NONE = 0,
        ANIM_PLAYBACK_ONCE_FORWARD = 1,
        ANIM_PLAYBACK_ONCE_BACKWARD = 2,
        ANIM_PLAYBACK_LOOP_FORWARD = 3,
        ANIM_PLAYBACK_LOOP_BACKWARD = 4,
        ANIM_PLAYBACK_LOOP_PINGPONG = 5,
        ANIM_PLAYBACK_ONCE_PINGPONG = 6,
    };

    struct AnimationData
    {
        AnimationData();

        void* m_Texture;
        float* m_TexCoords;
        float* m_TexDims;
        AnimPlayback m_Playback;
        uint32_t m_TileWidth;
        uint32_t m_TileHeight;
        uint32_t m_StartTile;
        uint32_t m_EndTile;
        uint32_t m_FPS;
        uint32_t m_HFlip;
        uint32_t m_VFlip;
        /// Clients are responsible for setting this to the size of the AnimationData (e.g. in ParticleLibrary.java)
        uint32_t m_StructSize;
    };

    enum FetchAnimationResult
    {
        FETCH_ANIMATION_OK = 0,
        FETCH_ANIMATION_NOT_FOUND = -1,
        FETCH_ANIMATION_UNKNOWN_ERROR = -1000
    };

    enum EmitterState
    {
        EMITTER_STATE_SLEEPING = 0,
        EMITTER_STATE_PRESPAWN = 1,
        EMITTER_STATE_SPAWNING = 2,
        EMITTER_STATE_POSTSPAWN = 3,
    };

    enum ParticleVertexFormat
    {
        PARTICLE_GO = 0,
        PARTICLE_GUI = 1,
    };

    struct EmitterRenderData
    {
    	EmitterRenderData()
    	{
    		memset(this, 0x0, sizeof(EmitterRenderData));
    	}

        Matrix4                     m_Transform;
        void*                       m_Material; // dmRender::HMaterial
        dmParticleDDF::BlendMode    m_BlendMode;
        void*                       m_Texture; // dmGraphics::HTexture
        RenderConstant*             m_RenderConstants;
        uint32_t                    m_RenderConstantsSize;
        HInstance                   m_Instance; // Particle instance handle
        uint32_t                    m_EmitterIndex;
        uint32_t                    m_MixedHash;
        uint32_t                    m_MixedHashNoMaterial;
    };

    /**
    * Callback for emitter state changed
    */
    typedef void (*EmitterStateChanged)(uint32_t num_awake_emitters, dmhash_t emitter_id, EmitterState emitter_state, void* user_data);

    /**
    * Data for emitter state change callback
    */
    struct EmitterStateChangedData
    {
        EmitterStateChangedData()
        {
            m_UserData = 0x0;
        }

        EmitterStateChanged m_StateChangedCallback;
        void* m_UserData;
    };

    /**
     * Callback to fetch the animation from a tile source
     */
    typedef FetchAnimationResult (*FetchAnimationCallback)(void* tile_source, dmhash_t animation, AnimationData* out_data);

    /**
     * Particle statistics
     */
    struct Stats
    {
        Stats()
        {
            m_StructSize = sizeof(*this);
        }

        uint32_t m_Particles;
        uint32_t m_MaxParticles;
        uint32_t m_StructSize;
    };

    /**
     * Particle instance statistics
     */
    struct InstanceStats
    {
        InstanceStats()
        {
            m_StructSize = sizeof(*this);
        }

        float m_Time;
        uint32_t m_StructSize;
    };

    /**
     * Particle vertex format
     */
    struct Vertex
    {
        // Offset 0
        float m_X, m_Y, m_Z;
        // Offset 12
        uint8_t m_Red, m_Green, m_Blue, m_Alpha;
        // Offset 16
        uint16_t m_U, m_V;
        // Offset 20
    };

    /**
     * Particle gui vertex format (must match dmGui::ParticleGuiVertex)
     */
    struct ParticleGuiVertex
    {
        // Offset 0
        float    m_Position[3];
        // Offset 12
        float    m_UV[2];
        // Offset 20
        uint32_t m_Color;
        // Offset 24
    };

    // For tests
    Vector3 GetPosition(HParticleContext context, HInstance instance);

    // For profiling
    uint32_t GetPrototypeResourceSize(HPrototype prototype);

#define DM_PARTICLE_PROTO(ret, name,  ...) \
    \
    ret name(__VA_ARGS__);\
    extern "C" DM_DLLEXPORT ret Particle_##name(__VA_ARGS__)

    /**
     * Create a context.
     * @param max_instance_count Max number of instances
     * @param max_particle_count Max number of particles
     * @return Context handle, or INVALID_CONTEXT when out of memory.
     */
    DM_PARTICLE_PROTO(HParticleContext, CreateContext, uint32_t max_instance_count, uint32_t max_particle_count);
    /**
     * Destroy a context.
     * @param context Context to destroy. This will also destroy any remaining instances.
     */
    DM_PARTICLE_PROTO(void, DestroyContext, HParticleContext context);

    /**
     * Retrieve max particle count for the context.
     * @param context Context to update.
     * @return Max number of particles
     */
    DM_PARTICLE_PROTO(uint32_t, GetContextMaxParticleCount, HParticleContext context);
    /**
     * Set new max particle count for the context.
     * @param context Context to update.
     * @param max_particle_count Max number of particles
     */
    DM_PARTICLE_PROTO(void, SetContextMaxParticleCount, HParticleContext context, uint32_t max_particle_count);

    /**
     * Create an instance from the supplied path and fetch resources using the supplied factory.
     * @param context Context in which to create the instance, must be valid.
     * @param prototype Prototype of the instance to be created
     * @param Pointer to data for emitter state change callback
     * @param fetch_animation_callback Callback to set animation data.
     * @return Instance handle, or INVALID_INSTANCE when the resource is broken or the context is full.
     */
    DM_PARTICLE_PROTO(HInstance, CreateInstance, HParticleContext context, HPrototype prototype, EmitterStateChangedData* data);
    /**
     * Destroy instance in the specified context.
     * @param context Context handle, must be valid.
     * @param instance Instance to destroy, can be invalid.
     */
    DM_PARTICLE_PROTO(void, DestroyInstance, HParticleContext context, HInstance instance);
    /**
     * Reload instance in the specified context based on its prototype.
     * @param context Context handle, must be valid.
     * @param instance Instance to reload, can be invalid.
     * @param replay if emitters should be replayed
     */
    DM_PARTICLE_PROTO(void, ReloadInstance, HParticleContext context, HInstance instance, bool replay);
    /**
     * Start the specified instance, which means it will start spawning particles.
     * @param context Context in which the instance exists.
     * @param instance Instance to start, can be invalid.
     */
    DM_PARTICLE_PROTO(void, StartInstance, HParticleContext context, HInstance instance);
    /**
     * Stop the specified instance, which means it will stop spawning particles.
     * Any spawned particles will still be simulated until they die.
     * @param context Context in which the instance exists.
     * @param instance Instance to start, can be invalid.
     */
    DM_PARTICLE_PROTO(void, StopInstance, HParticleContext context, HInstance instance);
    /**
     * Retire the specified instance, which means it will stop spawning particles at the closest convenient time.
     * In practice this means that looping emitters will continue for the current life cycle and then stop, like a once emitter.
     * Any spawned particles will still be simulated until they die.
     * @param context Context in which the instance exists.
     * @param instance Instance to start, can be invalid.
     */
    DM_PARTICLE_PROTO(void, RetireInstance, HParticleContext context, HInstance instance);
    /**
     * Reset the specified instance, which means its state will be like when first created.
     * Any already living particles will be annihilated.
     * @param context Context in which the instance exists.
     * @param instance Instance to reset, can be invalid.
     */
    DM_PARTICLE_PROTO(void, ResetInstance, HParticleContext context, HInstance instance);

    /**
     * Set the position of the specified instance.
     * @param context Context in which the instance exists.
     * @param instance Instance to set position for.
     * @param position Position in world space.
     */
    DM_PARTICLE_PROTO(void, SetPosition, HParticleContext context, HInstance instance, const Point3& position);
    /**
     * Set the rotation of the specified instance.
     * @param context Context in which the instance exists.
     * @param instance Instance to set rotation for.
     * @param rotation Rotation in world space.
     */
    DM_PARTICLE_PROTO(void, SetRotation, HParticleContext context, HInstance instance, const Quat& rotation);
    /**
     * Set the scale of the specified instance.
     * @param context Context in which the instance exists.
     * @param instance Instance to set scale for.
     * @param scale Scale in world space.
     */
    DM_PARTICLE_PROTO(void, SetScale, HParticleContext context, HInstance instance, float scale);
    /**
     * Set if the scale should be used along Z or not.
     * @param context Context in which the instance exists.
     * @param instance Instance to set the property for.
     * @param scale_along_z Whether the scale should be used along Z.
     */
    DM_PARTICLE_PROTO(void, SetScaleAlongZ, HParticleContext context, HInstance instance, bool scale_along_z);
    /**
     * Returns if the specified instance is spawning particles or not.
     * Instances are sleeping when they are not spawning and have no remaining living particles.
     */
    DM_PARTICLE_PROTO(bool, IsSleeping, HParticleContext context, HInstance instance);

    /**
     * Update the instances within the specified context.
     * @param context Context of the instances to update.
     * @param dt Time step.
     */
    DM_PARTICLE_PROTO(void, Update, HParticleContext context, float dt, FetchAnimationCallback fetch_animation_callback);

    /**
     * Gets the vertex count for rendering the emitter at a given emitter index
     * @param context Particle context
     * @param instance Particle instance handle
     * @param emitter_index Emitter index for which to get the vertex count
     * @return vertex count needed to render the emitter
     */
    DM_PARTICLE_PROTO(uint32_t, GetEmitterVertexCount, HParticleContext context, HInstance instance, uint32_t emitter_index);

    /**
     * Generates vertex data for an emitter
     * @param context Particle context
     * @param dt Time step.
     * @param instance Particle instance handle
     * @param emitter_index Emitter index for which to generate vertex data for
     * @param vertex_buffer Vertex buffer into which to store the particle vertex data. If this is 0x0, no data will be generated.
     * @param vertex_buffer_size Size in bytes of the supplied vertex buffer.
     * @param out_vertex_buffer_size Size in bytes of the total data written to vertex buffer.
     * @param vertex_format Which vertex format to use
     */
    DM_PARTICLE_PROTO(void, GenerateVertexData, HParticleContext context, float dt, HInstance instance, uint32_t emitter_index, const Vector4& color, void* vertex_buffer, uint32_t vertex_buffer_size, uint32_t* out_vertex_buffer_size, ParticleVertexFormat vertex_format);

    /**
     * Debug render the status of the instances within the specified context.
     * @param context Context of the instances to render.
     * @param RenderLine Function pointer to use to render the lines.
     */
    DM_PARTICLE_PROTO(void, DebugRender, HParticleContext context, void* user_context, RenderLineCallback render_line_callback);

    /**
     * Create a new prototype from ddf data.
     * @param buffer Buffer of ddf data
     * @param buffer_size Size of ddf data
     * @return prototype handle
     */
    DM_PARTICLE_PROTO(HPrototype, NewPrototype, const void* buffer, uint32_t buffer_size);

    /**
     * Create a new prototype from ddf message data.
     * Ownership of message data is transferred to the particle system.
     * @param message pointer to ParticleDDF message
     * @return prototype handle
     */
    DM_PARTICLE_PROTO(HPrototype, NewPrototypeFromDDF, dmParticleDDF::ParticleFX* message);

    /**
     * Delete a prototype
     * @param prototype Prototype to delete
     */
    DM_PARTICLE_PROTO(void, DeletePrototype, HPrototype prototype);

    /**
     * Reload a prototype. This does not update any dependant instances which should be recreated.
     * @param prototype Prototype to reload
     * @param buffer Buffer of ddf data
     * @param buffer_size Size of ddf data
     */
    DM_PARTICLE_PROTO(bool, ReloadPrototype, HPrototype prototype, const void* buffer, uint32_t buffer_size);

    /**
     * Retrieve number of emitters in the supplied prototype
     * @param prototype Prototype
     */
    DM_PARTICLE_PROTO(uint32_t, GetEmitterCount, HPrototype prototype);

    /**
     * Retrieve number of emitters on the supplied instance
     * @param context Context of the instance
     * @param instance The instance of which to get emitter count from
     * @return The number of emitters on the instance
     */
    DM_PARTICLE_PROTO(uint32_t, GetInstanceEmitterCount, HParticleContext context, HInstance instance);

    /**
     * Render the specified emitter
     * @param context Context of the emitter to render.
     * @param instance Instance of the emitter to render.
     * @param emitter_index Index of the emitter to render.
     * @param user_context Context to pass to the callback.
     * @param render_instance_callback Callback function that will be called with emitter data for actual rendering
     * @see Render
     */
    DM_PARTICLE_PROTO(void, RenderEmitter, HParticleContext context, HInstance instance, uint32_t emitter_index, void* user_context, RenderEmitterCallback render_instance_callback);

    /**
     * Retrieve data needed to render emitter
     * @param context Context of the instance
     * @param instance Instance which holds the emitter
     * @param emitter_index The index of the emitter for which to get render data for
     * @param data Out data for emitter render data.
    */
    DM_PARTICLE_PROTO(void, GetEmitterRenderData, HParticleContext context, HInstance instance, uint32_t emitter_index, EmitterRenderData** data);
    /**
     * Retrieve material path from the emitter in the supplied prototype
     * @param prototype Prototype
     * @param emitter_index Index of the emitter in question
     */
    DM_PARTICLE_PROTO(const char*, GetMaterialPath, HPrototype prototype, uint32_t emitter_index);
    /**
     * Retrieve tile source path from the emitter in the supplied prototype
     * @param prototype Prototype
     * @param emitter_index Index of the emitter in question
     */
    DM_PARTICLE_PROTO(const char*, GetTileSourcePath, HPrototype prototype, uint32_t emitter_index);
    /**
     * Retrieve material from the emitter in the supplied prototype
     * @param prototype Prototype
     * @param emitter_index Index of the emitter in question
     * @return pointer to the material
     */
    DM_PARTICLE_PROTO(void*, GetMaterial, HPrototype prototype, uint32_t emitter_index);
    /**
     * Retrieve tile source from the emitter in the supplied prototype
     * @param prototype Prototype
     * @param emitter_index Index of the emitter in question
     * @return pointer to the tile source
     */
    DM_PARTICLE_PROTO(void*, GetTileSource, HPrototype prototype, uint32_t emitter_index);
    /**
     * Set material in the emitter in the supplied prototype
     * @param prototype Prototype
     * @param emitter_index Index of the emitter in question
     * @param material Material to set
     */
    DM_PARTICLE_PROTO(void, SetMaterial, HPrototype prototype, uint32_t emitter_index, void* material);
    /**
     * Set tile source in the emitter in the supplied prototype
     * @param prototype Prototype
     * @param emitter_index Index of the emitter in question
     * @param tile_source Tile source to set
     */
    DM_PARTICLE_PROTO(void, SetTileSource, HPrototype prototype, uint32_t emitter_index, void* tile_source);

    /**
     * Set a render constant for the emitter with the specified id
     * @param context Particle context
     * @param instance Instance containing the emitter
     * @param emitter_id Id of the emitter
     * @param name_hash Hashed name of the constant to set
     * @param value Value to set the constant to
     */
    DM_PARTICLE_PROTO(void, SetRenderConstant, HParticleContext context, HInstance instance, dmhash_t emitter_id, dmhash_t name_hash, Vector4 value);
    /**
     * Reset a render constant for the emitter with the specified id
     * @param context Particle context
     * @param instance Instance containing the emitter
     * @param emitter_id Id of the emitter
     * @param name_hash Hashed name of the constant to reset
     */
    DM_PARTICLE_PROTO(void, ResetRenderConstant, HParticleContext context, HInstance instance, dmhash_t emitter_id, dmhash_t name_hash);

    /**
     * Get statistics
     * @param context Particle context
     * @param stats Pointer to stats structure
     */
    DM_PARTICLE_PROTO(void, GetStats, HParticleContext context, Stats* stats);

    /**
     * Get instance statistics
     * @param context Particle context
     * @param instance Instance to get stats for
     * @param stats Pointer to instance stats structure
     */
    DM_PARTICLE_PROTO(void, GetInstanceStats, HParticleContext context, HInstance instance, InstanceStats* stats);

    /**
     * Get required vertex buffer size
     * @param particle_count number of particles in vertex buffer
     * @param vertex_format Which vertex format to use
     * @return Required vertex buffer size in bytes
     */
    DM_PARTICLE_PROTO(uint32_t, GetVertexBufferSize, uint32_t particle_count, ParticleVertexFormat vertex_format);

    /**
     * Get the required vertex buffer size for a context
     * @param context Particle context
     * @param vertex_format Which vertex format to use
     * @return Required vertex buffer size in bytes
     */
    DM_PARTICLE_PROTO(uint32_t, GetMaxVertexBufferSize, HParticleContext context, ParticleVertexFormat vertex_format);

    /**
     * Rehash all emitters on an instance
     * @param context Particle context
     * @param instance Instance containing the emitters to be rehashed
     */
    DM_PARTICLE_PROTO(void, ReHash, HParticleContext context, HInstance instance);

    /**
     * Wrapper for dmHashString64
     * @param value string to hash
     * @return hashed string
     */
    extern "C" DM_DLLEXPORT dmhash_t Particle_Hash(const char* value);

#undef DM_PARTICLE_PROTO

}

#endif // DM_PARTICLE_H
