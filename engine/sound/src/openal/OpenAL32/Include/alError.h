#ifndef _AL_ERROR_H_
#define _AL_ERROR_H_

#include "alMain.h"

#ifdef __cplusplus
extern "C" {
#endif

extern ALboolean TrapALError;

ALvoid alSetError(ALCcontext *Context, ALenum errorCode);

#define SET_ERROR_AND_RETURN(ctx, err)  do {      \
    alSetError((ctx), (err));                     \
    return;                                       \
} while(0)

#define SET_AND_RETURN_ERROR(ctx, err)  do {      \
    alSetError((ctx), (err));                     \
    return (err);                                 \
} while(0)

#ifdef __cplusplus
}
#endif

#endif
