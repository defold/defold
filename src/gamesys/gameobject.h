#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include <ddf/ddf.h>
#include <vectormath/cpp/vectormath_aos.h>

#include "resource.h"
//#include "gameobject_script.h"

using namespace Vectormath::Aos;

namespace dmGameObject
{
    /// Instance handle
    typedef struct Instance* HInstance;

    /// Instance handle
    typedef struct ScriptInstance* HScriptInstance;

    /// Collection handle
    typedef struct Collection* HCollection;

    /**
     * Result enum
     */
    enum Result
    {
        RESULT_OK = 0,                      //!< RESULT_OK
        RESULT_OUT_OF_RESOURCES = -1,       //!< RESULT_OUT_OF_RESOURCES
        RESULT_ALREADY_REGISTERED = -2,     //!< RESULT_ALREADY_REGISTERED
        RESULT_IDENTIFIER_IN_USE = -3,      //!< RESULT_IDENTIFIER_IN_USE
        RESULT_IDENTIFIER_ALREADY_SET = -4, //!< RESULT_IDENTIFIER_ALREADY_SET
        RESULT_UNKNOWN_ERROR = -1000,       //!< RESULT_UNKNOWN_ERROR
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

    extern const uint32_t UNNAMED_IDENTIFIER;

    #define DMGAMEOBJECT_SCRIPT_EVENT_NAME "script_event"
    #define DMGAMEOBJECT_SCRIPT_EVENT_SOCKET_NAME "script"
    #define DMGAMEOBJECT_SCRIPT_REPLY_EVENT_SOCKET_NAME "script_reply"

    /**
     * Game script event data
     */
    struct ScriptEventData
    {
        /// Senderscript instance
        HScriptInstance          m_ScriptInstance;
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
     * @param max_instances Max instances in this collection
     * @return HCollection
     */
    HCollection NewCollection(uint32_t max_instances);

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
     * @param update_context Update context
     * @return New gameobject instance. NULL if any error occured
     */
    HInstance New(HCollection collection, dmResource::HFactory factory, const char* prototype_name, const UpdateContext* update_context);

    /**
     * Delete gameobject instance
     * @param collection Gameobject collection
     * @param factory Resource factory. Must be identical to factory used with New
     * @param instance Gameobject instance
     */
    void Delete(HCollection collection, dmResource::HFactory factory, HInstance instance);

    /**
     * Set instance identifier. Must be unique within the collection.
     * @param collection Collection
     * @param instance Instance
     * @param identifier Identifier
     * @return RESULT_OK on success
     */
    Result SetIdentifier(HCollection collection, HInstance instance, const char* identifier);

    /**
     * Get instance identifier
     * @param instance Instance
     * @return Identifer. dmGameObject::UNNAMED_IDENTIFIER if not set.
     */
    uint32_t GetIdentifier(HInstance instance);

    /**
     * Call update function. Does *NOT* dispatch script events
     * @param collection Gameobject collection
     * @param instance Gameobject instance
     * @param update_context Update context
     * @return True on success
     */
    bool Update(HCollection collection, HInstance instance, const UpdateContext* update_context);

    /**
     * Update all gameobjects and its components and dispatches all event to script
     * @param collection Gameobject collection
     * @param update_context Update context
     * @return True on success
     */
    bool Update(HCollection collection, const UpdateContext* update_context);

    /**
     * Set gameobject instance position
     * @param instance Gameobject instance
     * @param position New Position
     */
    void SetPosition(HInstance instance, Point3 position);

    /**
     * Get gameobject instance position
     * @param instance Gameobject instance
     * @return Position
     */
    Point3 GetPosition(HInstance instance);

    /**
     * Set gameobject instance rotation
     * @param instance Gameobject instance
     * @param position New Position
     */
    void SetRotation(HInstance instance, Quat rotation);

    /**
     * Get gameobject instance rotation
     * @param instance Gameobject instance
     * @return Position
     */
    Quat GetRotation(HInstance instance);

    /**
     * Register all resource types in resource factory
     * @param factory Resource factory
     * @return dmResource::FactoryResult
     */
    dmResource::FactoryResult RegisterResourceTypes(dmResource::HFactory factory);
}

#endif // GAMEOBJECT_H
