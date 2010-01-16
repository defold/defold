#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include <ddf/ddf.h>
#include <vectormath/cpp/vectormath_aos.h>

#include "resource.h"

using namespace Vectormath::Aos;

namespace dmGameObject
{
    /// Instance handle
    typedef struct Instance* HInstance;

    /// Collection handle
    typedef struct Collection* HCollection;

    /**
     * Result enum
     */
    enum Result
    {
        RESULT_OK = 0,                //!< RESULT_OK
        RESULT_OUT_OF_RESOURCES = -1,  //!< RESULT_OUT_OF_RESOURCES
        RESULT_ALREADY_REGISTERED = -2,//!< RESULT_ALREADY_REGISTERED
        RESULT_UNKNOWN_ERROR = -1000,  //!< RESULT_UNKNOWN_ERROR
    };

    /**
     * Create result enum
     */
    enum CreateResult
    {
        CREATE_RESULT_OK = 0,              //!< CREATE_RESULT_OK
        CREATE_RESULT_UNKNOWN_ERROR = -1000,//!< CREATE_RESULT_UNKNOWN_ERROR
    };

    /**
     * Update context
     */
    struct UpdateContext
    {
        /// DDF descriptor for global data
        dmDDF::Descriptor* m_DDFGlobalDataDescriptor;
        /// Global read-only data
        void*              m_GlobalData;
        /// Time step
        float              m_DT;
    };

    #define DMGAMEOBJECT_SCRIPT_EVENT_NAME "script_event"
    #define DMGAMEOBJECT_SCRIPT_EVENT_SOCKET_NAME "script"

    /**
     * Game script event data
     */
    struct ScriptEventData
    {
        /// Sender instance
        HInstance                m_Sender;
        /// Pay-load DDF descriptor
        const dmDDF::Descriptor* m_DDFDescriptor;
        /// Pay-load (DDF)
        uint8_t                  m_DDFData[0];
    };

    /**
     * Component create function
     * @param collection Collection handle
     * @param instance Game object instance
     * @param resource Component resource
     * @param context User context
     * @param user_data User data storage pointer
     * @return CREATE_RESULT_OK on success
     */
    typedef CreateResult (*ComponentCreate)(HCollection collection,
                                            HInstance instance,
                                            void* resource,
                                            void* context,
                                            uintptr_t* user_data);

    /**
     * Component destroy function
     * @param collection Collection handle
     * @param instance Game object instance
     * @param context User context
     * @param user_data User data storage pointer
     * @return CREATE_RESULT_OK on success
     */
    typedef CreateResult (*ComponentDestroy)(HCollection collection,
                                             HInstance instance,
                                             void* context,
                                             uintptr_t* user_data);

    /**
     * Component update function. Updates all component of this type for all game objects
     * @param collection Collection handle
     * @param update_context Update context
     * @param context User context
     */
    typedef void (*ComponentsUpdate)(HCollection collection,
                                     const UpdateContext* update_context,
                                     void* context);

    /**
     * Initialize system
     */
    void Initialize();

    /**
     * Finalize system
     */
    void Finalize();

    /**
     * Register DDF type
     * @param descriptor Descriptor
     * @return RESULT_OK on success
     */
    Result RegisterDDFType(const dmDDF::Descriptor* descriptor);

    /**
     * Creates a new gameobject collection
     * @return HCollection
     */
    HCollection NewCollection();

    /**
     * Deletes a gameobject collection
     * @param collection
     */
    void DeleteCollection(HCollection collection, dmResource::HFactory factory);

    /**
     * Register a new component type
     * @param collection Gameobject collection
     * @param resource_type Resource type, resource factory type
     * @param context User context
     * @param create_function Create function call-back
     * @param destroy_function Destroy function call-back
     * @param components_update Components update call-back. NULL if not required.
     * @param component_instance_has_user_data True if the component instance needs user data
     * @return RESULT_OK on success
     */
    Result RegisterComponentType(HCollection collection,
                                 uint32_t resource_type,
                                 void* context,
                                 ComponentCreate create_function,
                                 ComponentDestroy destroy_function,
                                 ComponentsUpdate components_update,
                                 bool component_instance_has_user_data);

    /**
     * Create a new gameobject instane
     * @param collection Gameobject collection
     * @param factory Resource factory
     * @param prototype_name Prototype file name
     * @return New gameobject instance. NULL if any error occured
     */
    HInstance New(HCollection collection, dmResource::HFactory factory, const char* prototype_name);

    /**
     * Delete gameobject instance
     * @param collection Gameobject collection
     * @param factory Resource factory. Must be identical to factory used with New
     * @param instance Gameobject instance
     */
    void Delete(HCollection collection, dmResource::HFactory factory, HInstance instance);

    /**
     * Call update function
     * @param collection Gameobject collection
     * @param instance Gameobject instance
     * @param update_context Update context
     * @return True on success
     */
    bool Update(HCollection collection, HInstance instance, const UpdateContext* update_context);

    /**
     * Update all gameobjects and its components
     * @param collection Gameobject collection
     * @param update_context Update context
     * @return True on success
     */
    bool Update(HCollection collection, const UpdateContext* update_context);

    /**
     * Set gameobject instance position
     * @param collection Gameobject collection
     * @param instance Gameobject instance
     * @param position New Position
     */
    void SetPosition(HCollection collection, HInstance instance, Point3 position);

    /**
     * Get gameobject instance position
     * @param collection Gameobject collection
     * @param instance Gameobject instance
     * @return Position
     */
    Point3 GetPosition(HCollection collection, HInstance instance);

    /**
     * Set gameobject instance rotation
     * @param collection Gameobject collection
     * @param instance Gameobject instance
     * @param position New Position
     */
    void SetRotation(HCollection collection, HInstance instance, Quat rotation);

    /**
     * Get gameobject instance rotation
     * @param collection Gameobject collection
     * @param instance Gameobject instance
     * @return Position
     */
    Quat GetRotation(HCollection collection, HInstance instance);

    /**
     * Register all resource types in resource factory
     * @param factory Resource factory
     * @return dmResource::FactoryResult
     */
    dmResource::FactoryResult RegisterResourceTypes(dmResource::HFactory factory);
}

#endif // GAMEOBJECT_H
