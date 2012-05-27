#ifndef DM_ENGINE_H
#define DM_ENGINE_H

#include <stdint.h>

#include <dlib/message.h>

#include <resource/resource.h>

// Embedded resources
extern unsigned char DEBUG_VPC[];
extern uint32_t DEBUG_VPC_SIZE;
extern unsigned char DEBUG_FPC[];
extern uint32_t DEBUG_FPC_SIZE;
extern unsigned char BUILTINS_ARC[];
extern uint32_t BUILTINS_ARC_SIZE;

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
