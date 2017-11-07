#ifndef DM_ENGINE_H
#define DM_ENGINE_H

#include <stdint.h>

#include <dlib/message.h>

#include <resource/resource.h>

namespace dmEngine
{
    typedef struct Engine* HEngine;

    typedef void (*PreRun)(HEngine engine, void* context);
    typedef void (*PostRun)(HEngine engine, void* context);

    int Launch(int argc, char *argv[], PreRun pre_run, PostRun post_run, void* context);

    uint16_t GetHttpPort(HEngine engine);
    uint32_t GetFrameCount(HEngine engine);
};

#endif // DM_ENGINE_H
