#include "config.h"

#include <stdlib.h>

#include "AL/al.h"
#include "AL/alc.h"
#include "alMain.h"
#include "alAuxEffectSlot.h"
#include "alError.h"


typedef struct ALnullStateFactory {
    DERIVE_FROM_TYPE(ALeffectStateFactory);
} ALnullStateFactory;

typedef struct ALnullState {
    DERIVE_FROM_TYPE(ALeffectState);
} ALnullState;

static ALnullStateFactory NullFactory;

/* This destructs (not free!) the effect state. It's called only when the
 * effect slot is no longer used.
 */
static ALvoid ALnullState_Destruct(ALnullState *state)
{
    (void)state;
}

/* This updates the device-dependant effect state. This is called on
 * initialization and any time the device parameters (eg. playback frequency,
 * format) have been changed.
 */
static ALboolean ALnullState_deviceUpdate(ALnullState *state, ALCdevice *device)
{
    return AL_TRUE;
    (void)state;
    (void)device;
}

/* This updates the effect state. This is called any time the effect is
 * (re)loaded into a slot.
 */
static ALvoid ALnullState_update(ALnullState *state, ALCdevice *device, const ALeffectslot *slot)
{
    (void)state;
    (void)device;
    (void)slot;
}

/* This processes the effect state, for the given number of samples from the
 * input to the output buffer. The result should be added to the output buffer,
 * not replace it.
 */
static ALvoid ALnullState_process(ALnullState *state, ALuint samplesToDo, const ALfloat *restrict samplesIn, ALfloat (*restrict samplesOut)[BUFFERSIZE])
{
    (void)state;
    (void)samplesToDo;
    (void)samplesIn;
    (void)samplesOut;
}

/* This frees the memory used by the object, after it has been destructed. */
static void ALnullState_Delete(ALnullState *state)
{
    free(state);
}

/* Define the forwards and the ALeffectState vtable for this type. */
DEFINE_ALEFFECTSTATE_VTABLE(ALnullState);


/* Creates ALeffectState objects of the appropriate type. */
ALeffectState *ALnullStateFactory_create(ALnullStateFactory *factory)
{
    ALnullState *state;
    (void)factory;

    state = calloc(1, sizeof(*state));
    if(!state) return NULL;
    /* Set vtables for inherited types. */
    SET_VTABLE2(ALnullState, ALeffectState, state);

    return STATIC_CAST(ALeffectState, state);
}

/* Define the ALeffectStateFactory vtable for this type. */
DEFINE_ALEFFECTSTATEFACTORY_VTABLE(ALnullStateFactory);


static void init_none_factory(void)
{
    SET_VTABLE2(ALnullStateFactory, ALeffectStateFactory, &NullFactory);
}

ALeffectStateFactory *ALnullStateFactory_getFactory(void)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, init_none_factory);
    return STATIC_CAST(ALeffectStateFactory, &NullFactory);
}


void ALnull_setParami(ALeffect *effect, ALCcontext *context, ALenum param, ALint val)
{
    (void)effect;
    (void)val;
    switch(param)
    {
        default:
            SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM);
    }
}
void ALnull_setParamiv(ALeffect *effect, ALCcontext *context, ALenum param, const ALint *vals)
{
    (void)effect;
    (void)vals;
    switch(param)
    {
        default:
            SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM);
    }
}
void ALnull_setParamf(ALeffect *effect, ALCcontext *context, ALenum param, ALfloat val)
{
    (void)effect;
    (void)val;
    switch(param)
    {
        default:
            SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM);
    }
}
void ALnull_setParamfv(ALeffect *effect, ALCcontext *context, ALenum param, const ALfloat *vals)
{
    (void)effect;
    (void)vals;
    switch(param)
    {
        default:
            SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM);
    }
}

void ALnull_getParami(ALeffect *effect, ALCcontext *context, ALenum param, ALint *val)
{
    (void)effect;
    (void)val;
    switch(param)
    {
        default:
            SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM);
    }
}
void ALnull_getParamiv(ALeffect *effect, ALCcontext *context, ALenum param, ALint *vals)
{
    (void)effect;
    (void)vals;
    switch(param)
    {
        default:
            SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM);
    }
}
void ALnull_getParamf(ALeffect *effect, ALCcontext *context, ALenum param, ALfloat *val)
{
    (void)effect;
    (void)val;
    switch(param)
    {
        default:
            SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM);
    }
}
void ALnull_getParamfv(ALeffect *effect, ALCcontext *context, ALenum param, ALfloat *vals)
{
    (void)effect;
    (void)vals;
    switch(param)
    {
        default:
            SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM);
    }
}

DEFINE_ALEFFECT_VTABLE(ALnull);
