#ifndef DM_ENGINE_H
#define DM_ENGINE_H

#include <stdint.h>

#include <dlib/message.h>

#include <resource/resource.h>

namespace dmEngine
{
    typedef struct Engine* HEngine;

    HEngine New();
    void Delete(HEngine);

    bool Init(HEngine engine, int argc, char *argv[]);
    void Reload(HEngine engine);
    int32_t Run(HEngine engine);
    void Exit(HEngine engine, int32_t code);

    void LoadCollection(HEngine engine, const char* collection_name);
    void UnloadCollection(HEngine engine, const char* collection_name);
    void ActivateCollection(HEngine engine, const char* collection_name);
};

#endif // DM_ENGINE_H
