/**
 * OpenAL cross platform audio library
 * Copyright (C) 2011 by Chris Robinson.
 * This library is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU Library General Public
 *  License as published by the Free Software Foundation; either
 *  version 2 of the License, or (at your option) any later version.
 *
 * This library is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 *  Library General Public License for more details.
 *
 * You should have received a copy of the GNU Library General Public
 *  License along with this library; if not, write to the
 *  Free Software Foundation, Inc., 59 Temple Place - Suite 330,
 *  Boston, MA  02111-1307, USA.
 * Or go to http://www.gnu.org/copyleft/lgpl.html
 */

#include "config.h"

#include <stdlib.h>

#include "alMain.h"
#include "alFilter.h"
#include "alAuxEffectSlot.h"
#include "alError.h"
#include "alu.h"


typedef struct ALdedicatedStateFactory {
    DERIVE_FROM_TYPE(ALeffectStateFactory);
} ALdedicatedStateFactory;

static ALdedicatedStateFactory DedicatedFactory;


typedef struct ALdedicatedState {
    DERIVE_FROM_TYPE(ALeffectState);

    ALfloat gains[MaxChannels];
} ALdedicatedState;


static ALvoid ALdedicatedState_Destruct(ALdedicatedState *state)
{
    (void)state;
}

static ALboolean ALdedicatedState_deviceUpdate(ALdedicatedState *state, ALCdevice *Device)
{
    return AL_TRUE;
    (void)state;
    (void)Device;
}

static ALvoid ALdedicatedState_update(ALdedicatedState *state, ALCdevice *device, const ALeffectslot *Slot)
{
    ALfloat Gain;
    ALsizei s;

    Gain = Slot->Gain * Slot->EffectProps.Dedicated.Gain;
    for(s = 0;s < MaxChannels;s++)
        state->gains[s] = 0.0f;

    if(Slot->EffectType == AL_EFFECT_DEDICATED_DIALOGUE)
        ComputeAngleGains(device, atan2f(0.0f, 1.0f), 0.0f, Gain, state->gains);
    else if(Slot->EffectType == AL_EFFECT_DEDICATED_LOW_FREQUENCY_EFFECT)
        state->gains[LFE] = Gain;
}

static ALvoid ALdedicatedState_process(ALdedicatedState *state, ALuint SamplesToDo, const ALfloat *restrict SamplesIn, ALfloat (*restrict SamplesOut)[BUFFERSIZE])
{
    const ALfloat *gains = state->gains;
    ALuint i, c;

    for(c = 0;c < MaxChannels;c++)
    {
        if(!(gains[c] > 0.00001f))
            continue;

        for(i = 0;i < SamplesToDo;i++)
            SamplesOut[c][i] = SamplesIn[i] * gains[c];
    }
}

static void ALdedicatedState_Delete(ALdedicatedState *state)
{
    free(state);
}

DEFINE_ALEFFECTSTATE_VTABLE(ALdedicatedState);


ALeffectState *ALdedicatedStateFactory_create(ALdedicatedStateFactory *factory)
{
    ALdedicatedState *state;
    ALsizei s;
    (void)factory;

    state = malloc(sizeof(*state));
    if(!state) return NULL;
    SET_VTABLE2(ALdedicatedState, ALeffectState, state);

    for(s = 0;s < MaxChannels;s++)
        state->gains[s] = 0.0f;

    return STATIC_CAST(ALeffectState, state);
}

DEFINE_ALEFFECTSTATEFACTORY_VTABLE(ALdedicatedStateFactory);


static void init_dedicated_factory(void)
{
    SET_VTABLE2(ALdedicatedStateFactory, ALeffectStateFactory, &DedicatedFactory);
}

ALeffectStateFactory *ALdedicatedStateFactory_getFactory(void)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, init_dedicated_factory);
    return STATIC_CAST(ALeffectStateFactory, &DedicatedFactory);
}


void ALdedicated_setParami(ALeffect *effect, ALCcontext *context, ALenum param, ALint val)
{ SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM); (void)effect;(void)param;(void)val; }
void ALdedicated_setParamiv(ALeffect *effect, ALCcontext *context, ALenum param, const ALint *vals)
{
    ALdedicated_setParami(effect, context, param, vals[0]);
}
void ALdedicated_setParamf(ALeffect *effect, ALCcontext *context, ALenum param, ALfloat val)
{
    ALeffectProps *props = &effect->Props;
    switch(param)
    {
        case AL_DEDICATED_GAIN:
            if(!(val >= 0.0f && isfinite(val)))
                SET_ERROR_AND_RETURN(context, AL_INVALID_VALUE);
            props->Dedicated.Gain = val;
            break;

        default:
            SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM);
    }
}
void ALdedicated_setParamfv(ALeffect *effect, ALCcontext *context, ALenum param, const ALfloat *vals)
{
    ALdedicated_setParamf(effect, context, param, vals[0]);
}

void ALdedicated_getParami(ALeffect *effect, ALCcontext *context, ALenum param, ALint *val)
{ SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM); (void)effect;(void)param;(void)val; }
void ALdedicated_getParamiv(ALeffect *effect, ALCcontext *context, ALenum param, ALint *vals)
{
    ALdedicated_getParami(effect, context, param, vals);
}
void ALdedicated_getParamf(ALeffect *effect, ALCcontext *context, ALenum param, ALfloat *val)
{
    const ALeffectProps *props = &effect->Props;
    switch(param)
    {
        case AL_DEDICATED_GAIN:
            *val = props->Dedicated.Gain;
            break;

        default:
            SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM);
    }
}
void ALdedicated_getParamfv(ALeffect *effect, ALCcontext *context, ALenum param, ALfloat *vals)
{
    ALdedicated_getParamf(effect, context, param, vals);
}

DEFINE_ALEFFECT_VTABLE(ALdedicated);
