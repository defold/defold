#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include "Python.h"
#include <vectormath/cpp/vectormath_aos.h>

#include "resource.h"

using namespace Vectormath::Aos;

namespace GameObject
{
    typedef struct Instance* HInstance;

    /**
     * Initialize system
     */
    void         Initialize();

    /**
     * Finalize system
     */
    void         Finalize();

    /**
     * Create a new gameobject instane
     * @param factory Resource factory
     * @param prototype_name Prototype file name
     * @return New gameobject instance. NULL if any error occured
     */
    HInstance    New(Resource::HFactory factory, const char* prototype_name);

    /**
     * Delete gameobject instance
     * @param factory Resource factory. Must be identical to factory used with New
     * @param instance Gameobject instance
     */
    void         Delete(Resource::HFactory factory, HInstance instance);

    /**
     * Call update function
     * @param instance Gameobject instance
     * @return True on success
     */
    bool         Update(HInstance instance);

    /**
     * Register all resource types in resource factory
     * @param factory Resource factory
     * @return Resource::FactoryError
     */
    Resource::FactoryError RegisterResourceTypes(Resource::HFactory factory);
}

#endif // GAMEOBJECT_H
