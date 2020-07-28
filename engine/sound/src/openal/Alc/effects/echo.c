/**
 * OpenAL cross platform audio library
 * Copyright (C) 2009 by Chris Robinson.
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

#include <math.h>
#include <stdlib.h>

#include "alMain.h"
#include "alFilter.h"
#include "alAuxEffectSlot.h"
#include "alError.h"
#include "alu.h"


typedef struct ALechoStateFactory {
    DERIVE_FROM_TYPE(ALeffectStateFactory);
} ALechoStateFactory;

static ALechoStateFactory EchoFactory;


typedef struct ALechoState {
    DERIVE_FROM_TYPE(ALeffectState);

    ALfloat *SampleBuffer;
    ALuint BufferLength;

    // The echo is two tap. The delay is the number of samples from before the
    // current offset
    struct {
        ALuint delay;
    } Tap[2];
    ALuint Offset;
    /* The panning gains for the two taps */
    ALfloat Gain[2][MaxChannels];

    ALfloat FeedGain;

    ALfilterState Filter;
} ALechoState;

static ALvoid ALechoState_Destruct(ALechoState *state)
{
    free(state->SampleBuffer);
    state->SampleBuffer = NULL;
}

static ALboolean ALechoState_deviceUpdate(ALechoState *state, ALCdevice *Device)
{
    ALuint maxlen, i;

    // Use the next power of 2 for the buffer length, so the tap offsets can be
    // wrapped using a mask instead of a modulo
    maxlen  = fastf2u(AL_ECHO_MAX_DELAY * Device->Frequency) + 1;
    maxlen += fastf2u(AL_ECHO_MAX_LRDELAY * Device->Frequency) + 1;
    maxlen  = NextPowerOf2(maxlen);

    if(maxlen != state->BufferLength)
    {
        void *temp;

        temp = realloc(state->SampleBuffer, maxlen * sizeof(ALfloat));
        if(!temp) return AL_FALSE;
        state->SampleBuffer = temp;
        state->BufferLength = maxlen;
    }
    for(i = 0;i < state->BufferLength;i++)
        state->SampleBuffer[i] = 0.0f;

    return AL_TRUE;
}

static ALvoid ALechoState_update(ALechoState *state, ALCdevice *Device, const ALeffectslot *Slot)
{
    ALuint frequency = Device->Frequency;
    ALfloat lrpan, gain;
    ALfloat dirGain;
    ALuint i;

    state->Tap[0].delay = fastf2u(Slot->EffectProps.Echo.Delay * frequency) + 1;
    state->Tap[1].delay = fastf2u(Slot->EffectProps.Echo.LRDelay * frequency);
    state->Tap[1].delay += state->Tap[0].delay;

    lrpan = Slot->EffectProps.Echo.Spread;

    state->FeedGain = Slot->EffectProps.Echo.Feedback;

    ALfilterState_setParams(&state->Filter, ALfilterType_HighShelf,
                            1.0f - Slot->EffectProps.Echo.Damping,
                            (ALfloat)LOWPASSFREQREF/frequency, 0.0f);

    gain = Slot->Gain;
    for(i = 0;i < MaxChannels;i++)
    {
        state->Gain[0][i] = 0.0f;
        state->Gain[1][i] = 0.0f;
    }

    dirGain = fabsf(lrpan);

    /* First tap panning */
    ComputeAngleGains(Device, atan2f(-lrpan, 0.0f), (1.0f-dirGain)*F_PI, gain, state->Gain[0]);

    /* Second tap panning */
    ComputeAngleGains(Device, atan2f(+lrpan, 0.0f), (1.0f-dirGain)*F_PI, gain, state->Gain[1]);
}

static ALvoid ALechoState_process(ALechoState *state, ALuint SamplesToDo, const ALfloat *restrict SamplesIn, ALfloat (*restrict SamplesOut)[BUFFERSIZE])
{
    const ALuint mask = state->BufferLength-1;
    const ALuint tap1 = state->Tap[0].delay;
    const ALuint tap2 = state->Tap[1].delay;
    ALuint offset = state->Offset;
    ALfloat smp;
    ALuint base;
    ALuint i, k;

    for(base = 0;base < SamplesToDo;)
    {
        ALfloat temps[64][2];
        ALuint td = minu(SamplesToDo-base, 64);

        for(i = 0;i < td;i++)
        {
            /* First tap */
            temps[i][0] = state->SampleBuffer[(offset-tap1) & mask];
            /* Second tap */
            temps[i][1] = state->SampleBuffer[(offset-tap2) & mask];

            // Apply damping and feedback gain to the second tap, and mix in the
            // new sample
            smp = ALfilterState_processSingle(&state->Filter, temps[i][1]+SamplesIn[i+base]);
            state->SampleBuffer[offset&mask] = smp * state->FeedGain;
            offset++;
        }

        for(k = 0;k < MaxChannels;k++)
        {
            ALfloat gain = state->Gain[0][k];
            if(gain > 0.00001f)
            {
                for(i = 0;i < td;i++)
                    SamplesOut[k][i+base] += temps[i][0] * gain;
            }

            gain = state->Gain[1][k];
            if(gain > 0.00001f)
            {
                for(i = 0;i < td;i++)
                    SamplesOut[k][i+base] += temps[i][1] * gain;
            }
        }

        base += td;
    }

    state->Offset = offset;
}

static void ALechoState_Delete(ALechoState *state)
{
    free(state);
}

DEFINE_ALEFFECTSTATE_VTABLE(ALechoState);


ALeffectState *ALechoStateFactory_create(ALechoStateFactory *factory)
{
    ALechoState *state;
    (void)factory;

    state = malloc(sizeof(*state));
    if(!state) return NULL;
    SET_VTABLE2(ALechoState, ALeffectState, state);

    state->BufferLength = 0;
    state->SampleBuffer = NULL;

    state->Tap[0].delay = 0;
    state->Tap[1].delay = 0;
    state->Offset = 0;

    ALfilterState_clear(&state->Filter);

    return STATIC_CAST(ALeffectState, state);
}

DEFINE_ALEFFECTSTATEFACTORY_VTABLE(ALechoStateFactory);


static void init_echo_factory(void)
{
    SET_VTABLE2(ALechoStateFactory, ALeffectStateFactory, &EchoFactory);
}

ALeffectStateFactory *ALechoStateFactory_getFactory(void)
{
    static pthread_once_t once = PTHREAD_ONCE_INIT;
    pthread_once(&once, init_echo_factory);
    return STATIC_CAST(ALeffectStateFactory, &EchoFactory);
}


void ALecho_setParami(ALeffect *effect, ALCcontext *context, ALenum param, ALint val)
{ SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM); (void)effect;(void)param;(void)val; }
void ALecho_setParamiv(ALeffect *effect, ALCcontext *context, ALenum param, const ALint *vals)
{
    ALecho_setParami(effect, context, param, vals[0]);
}
void ALecho_setParamf(ALeffect *effect, ALCcontext *context, ALenum param, ALfloat val)
{
    ALeffectProps *props = &effect->Props;
    switch(param)
    {
        case AL_ECHO_DELAY:
            if(!(val >= AL_ECHO_MIN_DELAY && val <= AL_ECHO_MAX_DELAY))
                SET_ERROR_AND_RETURN(context, AL_INVALID_VALUE);
            props->Echo.Delay = val;
            break;

        case AL_ECHO_LRDELAY:
            if(!(val >= AL_ECHO_MIN_LRDELAY && val <= AL_ECHO_MAX_LRDELAY))
                SET_ERROR_AND_RETURN(context, AL_INVALID_VALUE);
            props->Echo.LRDelay = val;
            break;

        case AL_ECHO_DAMPING:
            if(!(val >= AL_ECHO_MIN_DAMPING && val <= AL_ECHO_MAX_DAMPING))
                SET_ERROR_AND_RETURN(context, AL_INVALID_VALUE);
            props->Echo.Damping = val;
            break;

        case AL_ECHO_FEEDBACK:
            if(!(val >= AL_ECHO_MIN_FEEDBACK && val <= AL_ECHO_MAX_FEEDBACK))
                SET_ERROR_AND_RETURN(context, AL_INVALID_VALUE);
            props->Echo.Feedback = val;
            break;

        case AL_ECHO_SPREAD:
            if(!(val >= AL_ECHO_MIN_SPREAD && val <= AL_ECHO_MAX_SPREAD))
                SET_ERROR_AND_RETURN(context, AL_INVALID_VALUE);
            props->Echo.Spread = val;
            break;

        default:
            SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM);
    }
}
void ALecho_setParamfv(ALeffect *effect, ALCcontext *context, ALenum param, const ALfloat *vals)
{
    ALecho_setParamf(effect, context, param, vals[0]);
}

void ALecho_getParami(ALeffect *effect, ALCcontext *context, ALenum param, ALint *val)
{ SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM); (void)effect;(void)param;(void)val; }
void ALecho_getParamiv(ALeffect *effect, ALCcontext *context, ALenum param, ALint *vals)
{
    ALecho_getParami(effect, context, param, vals);
}
void ALecho_getParamf(ALeffect *effect, ALCcontext *context, ALenum param, ALfloat *val)
{
    const ALeffectProps *props = &effect->Props;
    switch(param)
    {
        case AL_ECHO_DELAY:
            *val = props->Echo.Delay;
            break;

        case AL_ECHO_LRDELAY:
            *val = props->Echo.LRDelay;
            break;

        case AL_ECHO_DAMPING:
            *val = props->Echo.Damping;
            break;

        case AL_ECHO_FEEDBACK:
            *val = props->Echo.Feedback;
            break;

        case AL_ECHO_SPREAD:
            *val = props->Echo.Spread;
            break;

        default:
            SET_ERROR_AND_RETURN(context, AL_INVALID_ENUM);
    }
}
void ALecho_getParamfv(ALeffect *effect, ALCcontext *context, ALenum param, ALfloat *vals)
{
    ALecho_getParamf(effect, context, param, vals);
}

DEFINE_ALEFFECT_VTABLE(ALecho);
