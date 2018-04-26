#include "gameobjet_private.h"

    /** Iterate over a registry to get all collections
     */
    void IterateCollections(HRegister register, FCollectionIterator callback, void* user_ctx);

    /** Iterate over a collection to get all game objects
     */
    void IterateGameObjects(HCollection collection, FGameObjectIterator callback, void* user_ctx);

    /** Iterate over a gameobject to get all components
     */
    void IterateComponents(HInstance instance, FGameComponentIterator callback, void* user_ctx);