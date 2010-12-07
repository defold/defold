#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include <stdint.h>
#include <vectormath/cpp/vectormath_aos.h>

#include <dlib/message.h>

#include <ddf/ddf.h>

#include <resource/resource.h>

namespace dmGameObject
{
    using namespace Vectormath::Aos;

    /// Instance handle
    typedef struct Instance* HInstance;

    /// Script handle
    typedef struct Script* HScript;

    /// Instance handle
    typedef struct ScriptInstance* HScriptInstance;

    /// Component register
    typedef struct Register* HRegister;

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
        RESULT_COMPONENT_NOT_FOUND = -5,    //!< RESULT_COMPONENT_NOT_FOUND
        RESULT_MAXIMUM_HIEARCHICAL_DEPTH = -6, //!< RESULT_MAXIMUM_HIEARCHICAL_DEPTH
        RESULT_INVALID_OPERATION = -7,      //!< RESULT_INVALID_OPERATION
        RESULT_RESOURCE_TYPE_NOT_FOUND = -8,    //!< RESULT_COMPONENT_TYPE_NOT_FOUND
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
     * Create result enum
     */
    enum UpdateResult
    {
        UPDATE_RESULT_OK = 0,              //!< UPDATE_RESULT_OK
        UPDATE_RESULT_UNKNOWN_ERROR = -1000,//!< UPDATE_RESULT_UNKNOWN_ERROR
    };

    /**
     * Input result enum
     */
    enum InputResult
    {
        INPUT_RESULT_IGNORED = 0,           //!< INPUT_RESULT_IGNORED
        INPUT_RESULT_CONSUMED = 1,          //!< INPUT_RESULT_CONSUMED
        INPUT_RESULT_UNKNOWN_ERROR = -1000  //!< INPUT_RESULT_UNKNOWN_ERROR
    };

    /**
     * Update context
     */
    struct UpdateContext
    {
        /// Time step
        float m_DT;
    };

    extern const uint32_t UNNAMED_IDENTIFIER;

    // TODO: Configurable?
    const uint32_t INSTANCE_MESSAGE_MAX = 256;

    /**
     * Game script message data
     */
    struct InstanceMessageData
    {
        InstanceMessageData();

        /// Subject
        HInstance m_Instance;

        /// Subject instance component index. Set to 0xff to broadcast to all components. Set to 0 to target scripts.
        uint8_t   m_Component;
        uint8_t   m_Pad[3];

        /// Message id
        uint32_t  m_MessageId;

        /// Pay-load DDF descriptor. NULL if not present
        const dmDDF::Descriptor* m_DDFDescriptor;
        /// Pay-load size
        uint32_t                 m_BufferSize;
        /// Pay-load
        uint8_t                  m_Buffer[0];
    };

    /**
     * Container of input related information.
     */
    struct InputAction
    {
        /// Action id, hashed action name
        uint32_t m_ActionId;
        /// Value of the input [0,1]
        float m_Value;
        /// If the input was 0 last update
        uint16_t m_Pressed;
        /// If the input turned from above 0 to 0 this update
        uint16_t m_Released;
        /// If the input was held enough for the value to be repeated this update
        uint16_t m_Repeated;
    };

    /**
     * Component world create function
     * @param config Configuration of the world
     * @param context Context for the component type
     * @param world Out-parameter of the pointer in which to store the created world
     * @return CREATE_RESULT_OK on success
     */
    typedef CreateResult (*ComponentNewWorld)(void* context, void** world);

    /**
     * Component world destroy function
     * @param config Configuration of the world
     * @param context Context for the component type
     * @param world The pointer to the world to destroy
     * @return CREATE_RESULT_OK on success
     */
    typedef CreateResult (*ComponentDeleteWorld)(void* context, void* world);

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
                                            void* world,
                                            void* context,
                                            uintptr_t* user_data);

    /**
     * Component init function
     * @param collection Collection handle
     * @param instance Game object instance
     * @param resource Component resource
     * @param context User context
     * @param user_data User data storage pointer
     * @return CREATE_RESULT_OK on success
     */
    typedef CreateResult (*ComponentInit)(HCollection collection,
                                            HInstance instance,
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
                                             void* world,
                                             void* context,
                                             uintptr_t* user_data);

    /**
     * Component update function. Updates all component of this type for all game objects
     * @param collection Collection handle
     * @param update_context Update context
     * @param context User context
     */
    typedef UpdateResult (*ComponentsUpdate)(HCollection collection,
                                     const UpdateContext* update_context,
                                     void* world,
                                     void* context);

    /**
     * Component on-message function. Called when message is sent to this component
     * @param instance Instance handle
     * @param context User context
     * @param user_data User data storage pointer
     */
    typedef UpdateResult (*ComponentOnMessage)(HInstance instance,
                                     const InstanceMessageData* message_data,
                                     void* context,
                                     uintptr_t* user_data);

    /**
     * Component on-input function. Called when input is sent to this component
     * @param instance Instance handle
     * @param context User context
     * @param user_data User data storage pointer
     * @return How the component handled the input
     */
    typedef InputResult (*ComponentOnInput)(HInstance instance,
                                     const InputAction* input_action,
                                     void* context,
                                     uintptr_t* user_data);

    /**
     * Collection of component registration data.
     */
    struct ComponentType
    {
        ComponentType();

        uint32_t                m_ResourceType;
        const char*             m_Name;
        void*                   m_Context;
        ComponentNewWorld       m_NewWorldFunction;
        ComponentDeleteWorld    m_DeleteWorldFunction;
        ComponentCreate         m_CreateFunction;
        ComponentInit           m_InitFunction;
        ComponentDestroy        m_DestroyFunction;
        ComponentsUpdate        m_UpdateFunction;
        ComponentOnMessage      m_OnMessageFunction;
        ComponentOnInput        m_OnInputFunction;
        uint32_t                m_InstanceHasUserData : 1;
        uint32_t                m_Reserved : 31;
        uint16_t                m_UpdateOrderPrio;
    };

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
     * Create a new component type register
     * @param dispatch_callback Message dispatch function that will be called when collections belonging to this register are updated.
     * @param dispatch_userdata User-defined data that will be passed to the dispatch_callback.
     * @return Register handle
     */
    HRegister NewRegister(dmMessage::DispatchCallback dispatch_callback, void* dispatch_userdata);

    /**
     * Delete a component type register
     * @param regist Register to delete
     */
    void DeleteRegister(HRegister regist);

    /**
     * Creates a new gameobject collection
     * @param factory Resource factory. Must be valid during the life-time of the collection
     * @param regist Register
     * @param max_instances Max instances in this collection
     * @return HCollection
     */
    HCollection NewCollection(dmResource::HFactory factory, HRegister regist, uint32_t max_instances);

    /**
     * Deletes a gameobject collection
     * @param collection
     */
    void DeleteCollection(HCollection collection);

    /**
     * Retrieve the world in the collection connected to the supplied resource type
     * @param collection Collection handle
     * @param resource_type Resource type
     * @return A pointer to the world or 0x0 if the resource type could not be found
     */
    void* FindWorld(HCollection collection, uint32_t resource_type);

    /**
     * Register a new component type
     * @param regist Gameobject register
     * @param type Collection of component type registration data
     * @return RESULT_OK on success
     */
    Result RegisterComponentType(HRegister regist, const ComponentType& type);

    /**
     * Set update order priority. Zero is highest priority.
     * @param resource_type Resource type
     * @param prio Priority
     * @return RESULT_OK on success
     */
    Result SetUpdateOrderPrio(HRegister regist, uint32_t resource_type, uint16_t prio);

    /**
     * Create a new gameobject instance
     * @note Calling this function during update is not permitted. Use #Spawn instead for deferred creation
     * @param collection Gameobject collection
     * @param prototype_name Prototype file name
     * @return New gameobject instance. NULL if any error occured
     */
    HInstance New(HCollection collection, const char* prototype_name);

    /**
     * Spawns a new gameobject instance. The actual creation is performed after the update is completed.
     * @param collection Gameobject collection
     * @param prototype_name Prototype file name
     * @param position Position of the spawed object
     * @param rotation Rotation of the spawned object
     */
    void Spawn(HCollection collection, const char* prototype_name, const Point3& position, const Quat& rotation);

    /**
     * Delete gameobject instance
     * @param collection Gameobject collection
     * @param instance Gameobject instance
     */
    void Delete(HCollection collection, HInstance instance);

    /**
     * Delete all gameobject instances in the collection
     * @param collection Gameobject collection
     */
    void DeleteAll(HCollection collection);

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
     * @return Identifier. dmGameObject::UNNAMED_IDENTIFIER if not set.
     */
    uint32_t GetIdentifier(HInstance instance);

    /**
     * Get absolute identifier relative to #instance. The returned identifier is the
     * representation of the qualified name, i.e. the path from root-collection to the sub-collection which the #instance belongs to.
     * Example: if #instance is part of a sub-collection in the root-collection named "sub" and id == "a" the returned identifier represents the path "sub.a"
     * @param instance Instance to absolute identifier to
     * @param id Identifier relative to #instance
     * @return Absolute identifier
     */
    uint32_t GetAbsoluteIdentifier(HInstance instance, const char* id);

    /**
     * Get instance from identifier
     * @param collection Collection
     * @param identifier Identifier
     * @return Instance. NULL if instance isn't found.
     */
    HInstance GetInstanceFromIdentifier(HCollection collection, uint32_t identifier);

    /**
     * Post named message
     * @param reg Handle to the register which defines the socket of the message.
     * @param message_id Hash of the message, the original name should be lowercase with underscore as separator
     * @return RESULT_OK on success
     */
    Result PostNamedMessage(HRegister reg, uint32_t message_id);

    /**
     * Post ddf message
     * @param reg Handle to the register which defines the socket of the message.
     * @param ddf_desc Descripor of the ddf message to send
     * @param ddf_data The actual ddf message to send
     * @return RESULT_OK on success
     */
    Result PostDDFMessage(HRegister reg, const dmDDF::Descriptor* ddf_desc, char* ddf_data);

    /**
     * Post named message to instance
     * @param instance Instance
     * @param component_name Component name. NULL broadcasts the message to every component.
     * @param message_id Hash of the message, the original name should be lowercase with underscore as separator
     * @return RESULT_OK on success
     */
    Result PostNamedMessageTo(HInstance instance, const char* component_name, uint32_t message_id, char* buffer, uint32_t buffer_size);

    /**
     * Post ddf message to instance
     * @param instance Instance
     * @param component_name Component name. NULL broadcasts the message to every component.
     * @param ddf_desc Descripor of the ddf message to send
     * @param ddf_data The actual ddf message to send
     * @return RESULT_OK on success
     */
    Result PostDDFMessageTo(HInstance instance, const char* component_name, const dmDDF::Descriptor* ddf_desc, char* ddf_data);

    /**
     * Set integer property in instance script
     * @param instance Instnce
     * @param key Key
     * @param value Value
     */
    void SetScriptIntProperty(HInstance instance, const char* key, int32_t value);

    /**
     * Set float property in instance script
     * @param instance Instnce
     * @param key Key
     * @param value Value
     */
    void SetScriptFloatProperty(HInstance instance, const char* key, float value);

    /**
     * Set string property in instance script
     * @param instance Instnce
     * @param key Key
     * @param value Value
     */
    void SetScriptStringProperty(HInstance instance, const char* key, const char* value);

    /**
     * Initializes a game object instance in the supplied collection.
     * @param collection Game object collection
     */
    bool Init(HCollection collection, HInstance instance);

    /**
     * Initializes all game object instances in the supplied collection.
     * @param collection Game object collection
     */
    bool Init(HCollection collection);

    /**
     * Update all gameobjects and its components and dispatches all message to script.
     * The order is to update each component type, one at a time, over all collections each iteration.
     * @param collections Array of game object collections to be updated, all must share the same registry.
     * @param update_contexts Array of update contexts, one for each collection.
     * @param collection_count Number of collections in the array.
     * @return True on success
     */
    bool Update(HCollection* collections, const UpdateContext* update_contexts, uint32_t collection_count);

    /**
     * Performs clean up of the collection after update, such as deleting all instances scheduled for delete.
     * @param collections Array of game object collections
     * @param collection_count Number of collections
     * @return True on success
     */
    bool PostUpdate(HCollection* collections, uint32_t collection_count);

    /**
     * Dispatches input actions to the input focus stacks in the supplied array of game object collections.
     * @param collections Array of game object collections
     * @param collection_count Number of collections
     * @param input_actions Array of input actions to dispatch
     * @param input_action_count Number of input actions
     */
    UpdateResult DispatchInput(HCollection* collections, uint32_t collection_count, InputAction* input_actions, uint32_t input_action_count);

    void AcquireInputFocus(HCollection collection, HInstance instance);
    void ReleaseInputFocus(HCollection collection, HInstance instance);

    /**
     * Retrieve a collection from the specified instance
     * @param instance Game object instance
     * @return The collection the specified instance belongs to
     */
    HCollection GetCollection(HInstance instance);

    /**
     * Retrieve a factory from the specified collection
     * @param collection Game object collection
     * @return The resource factory bound to the specified collection
     */
    dmResource::HFactory GetFactory(HCollection collection);

    /**
     * Retrieve a register from the specified collection
     * @param collection Game object collection
     * @return The register bound to the specified collection
     */
    HRegister GetRegister(HCollection collection);

    /**
     * Retrieve the message socket id for the specified register.
     * @param reg Register handle
     * @return The message socket id of the specified collection
     */
    dmMessage::HSocket GetMessageSocket(HRegister reg);

    /**
     * Retrieve the reply message socket id for the specified register.
     * @param reg Register handle
     * @return The reply message socket id of the specified collection
     */
    dmMessage::HSocket GetReplyMessageSocket(HRegister reg);

    /**
     * Retrieve the designated message id for game object messages for the specified register.
     * @param reg Register handle
     * @return The message id of the specified collection
     */
    uint32_t GetMessageId(HRegister reg);

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
     * Get gameobject instance world position
     * @param instance Gameobject instance
     * @return World position
     */
    Point3 GetWorldPosition(HInstance instance);

    /**
     * Get gameobject instance world rotation
     * @param instance Gameobject instance
     * @return World rotation
     */
    Quat GetWorldRotation(HInstance instance);

    /**
     * Set parent instance to child
     * @note Instances must belong to the same collection
     * @param child Child instance
     * @param parent Parent instance
     * @return RESULT_OK on success. RESULT_MAXIMUM_HIEARCHICAL_DEPTH if parent at maximal level
     */
    Result SetParent(HInstance child, HInstance parent);

    /**
     * Get parent instance if exists
     * @param instance Gameobject instance
     * @return Parent instance. NULL if passed instance is rooted
     */
    HInstance GetParent(HInstance instance);

    /**
     * Get instance hierarchical depth
     * @param instance Gameobject instance
     * @return Hierarchical depth
     */
    uint32_t GetDepth(HInstance instance);

    /**
     * Get child count
     * @note O(n) operation. Should only be used for debugging purposes.
     * @param instance Gameobject instance
     * @return Child count
     */
    uint32_t GetChildCount(HInstance instance);

    /**
     * Test if "child" is a direct parent of "parent"
     * @param child Child Gamebject
     * @param parent Parent Gameobject
     * @return True if child of
     */
    bool IsChildOf(HInstance child, HInstance parent);

    /**
     * Register all resource types in resource factory
     * @param factory Resource factory
     * @param regist Register
     * @return dmResource::FactoryResult
     */
    dmResource::FactoryResult RegisterResourceTypes(dmResource::HFactory factory, HRegister regist);

    /**
     * Register all component types in collection
     * @param factory Resource factory
     * @param regist Register
     * @return Result
     */
    Result RegisterComponentTypes(dmResource::HFactory factory, HRegister regist);
}

#endif // GAMEOBJECT_H
