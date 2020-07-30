#ifndef _AL_LISTENER_H_
#define _AL_LISTENER_H_

#include "alMain.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ALlistener {
    volatile ALfloat Position[3];
    volatile ALfloat Velocity[3];
    volatile ALfloat Forward[3];
    volatile ALfloat Up[3];
    volatile ALfloat Gain;
    volatile ALfloat MetersPerUnit;

    struct {
        ALfloat Matrix[4][4];
        ALfloat Velocity[3];
    } Params;
} ALlistener;

#ifdef __cplusplus
}
#endif

#endif
