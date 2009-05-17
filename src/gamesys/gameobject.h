#ifndef GAMEOBJECT_H
#define GAMEOBJECT_H

#include "Python.h"
#include <vectormath/cpp/vectormath_aos.h>

#include "resource.h"

using namespace Vectormath::Aos;

namespace GameObject
{
    typedef struct Instance* HInstance;

    HInstance              New(Resource::HFactory factory, const char* prototype_name);
    void                   Delete(Resource::HFactory factory, HInstance instance);

    Resource::FactoryError RegisterResourceTypes(Resource::HFactory factory);


/*    struct Script
    {

    };*/
}

#endif // GAMEOBJECT_H
