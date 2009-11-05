#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include "Python.h"
#include <vectormath/cpp/vectormath_aos.h>

#include "resource.h"

using namespace Vectormath::Aos;

namespace GameObject
{
    typedef struct Instance* HInstance;
    typedef struct Collection* HCollection;

    enum Result
    {
        RESULT_OK = 0,
        RESULT_OUT_OF_RESOURCES = 1,
        RESULT_ALREADY_REGISTERED = 2,
        RESULT_UNKNOWN_ERROR = 1000,
    };

    enum CreateResult
    {
        CREATE_RESULT_OK = 0,
        CREATE_RESULT_UNKNOWN_ERROR = 1000,
    };

    struct UpdateContext
    {
        float m_DT;
    };

    typedef CreateResult (*ComponentCreate)(HCollection collection,
                                            HInstance instance,
                                            void* resource,
                                            void* context,
                                            uintptr_t* user_data);

    typedef CreateResult (*ComponentDestroy)(HCollection collection,
                                             HInstance instance,
                                             void* context,
                                             uintptr_t* user_data);

    typedef void (*ComponentsUpdate)(HCollection collection,
                                     const UpdateContext* update_context,
                                     void* context);

    /**
     * Initialize system
     */
    void         Initialize();

    /**
     * Finalize system
     */
    void         Finalize();

    /**
     * Creates a new gameobject collection
     * @return HCollection
     */
    HCollection  NewCollection();

    /**
     * Deletes a gameobject collection
     * @param collection
     */
    void         DeleteCollection(HCollection collection, Resource::HFactory factory);

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
    Result       RegisterComponentType(HCollection collection,
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
    HInstance    New(HCollection collection, Resource::HFactory factory, const char* prototype_name);

    /**
     * Delete gameobject instance
     * @param collection Gameobject collection
     * @param factory Resource factory. Must be identical to factory used with New
     * @param instance Gameobject instance
     */
    void         Delete(HCollection collection, Resource::HFactory factory, HInstance instance);

    /**
     * Call update function
     * @param collection Gameobject collection
     * @param instance Gameobject instance
     * @return True on success
     */
    bool         Update(HCollection collection, HInstance instance);

    /**
     * Update all gameobjects and its components
     * @param collection Gameobject collection
     * @param update_context Update context
     * @return True on success
     */
    bool         Update(HCollection collection, const UpdateContext* update_context);

    /**
     * Set gameobject instance position
     * @param collection Gameobject collection
     * @param instance Gameobject instance
     * @param position New Position
     */
    void         SetPosition(HCollection collection, HInstance instance, Point3 position);

    /**
     * Get gameobject instance position
     * @param collection Gameobject collection
     * @param instance Gameobject instance
     * @return Position
     */
    Point3       GetPosition(HCollection collection, HInstance instance);

    /**
     * Set gameobject instance rotation
     * @param collection Gameobject collection
     * @param instance Gameobject instance
     * @param position New Position
     */
    void         SetRotation(HCollection collection, HInstance instance, Quat rotation);

    /**
     * Get gameobject instance rotation
     * @param collection Gameobject collection
     * @param instance Gameobject instance
     * @return Position
     */
    Quat         GetRotation(HCollection collection, HInstance instance);

    /**
     * Register all resource types in resource factory
     * @param factory Resource factory
     * @return Resource::FactoryResult
     */
    Resource::FactoryResult RegisterResourceTypes(Resource::HFactory factory);
}

#endif // GAMEOBJECT_H
