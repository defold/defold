/**
 * OpenAL cross platform audio library
 * Copyright (C) 1999-2000 by authors.
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
#include "AL/alc.h"
#include "AL/alext.h"
#include "alError.h"
#include "alSource.h"
#include "alAuxEffectSlot.h"


static const ALchar alVendor[] = "OpenAL Community";
static const ALchar alVersion[] = "1.1 ALSOFT "ALSOFT_VERSION;
static const ALchar alRenderer[] = "OpenAL Soft";

// Error Messages
static const ALchar alNoError[] = "No Error";
static const ALchar alErrInvalidName[] = "Invalid Name";
static const ALchar alErrInvalidEnum[] = "Invalid Enum";
static const ALchar alErrInvalidValue[] = "Invalid Value";
static const ALchar alErrInvalidOp[] = "Invalid Operation";
static const ALchar alErrOutOfMemory[] = "Out of Memory";

AL_API ALvoid AL_APIENTRY alEnable(ALenum capability)
{
    ALCcontext *Context;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        switch(capability)
        {
            case AL_SOURCE_DISTANCE_MODEL:
                Context->SourceDistanceModel = AL_TRUE;
                Context->UpdateSources = AL_TRUE;
                break;

            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alDisable(ALenum capability)
{
    ALCcontext *Context;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        switch(capability)
        {
            case AL_SOURCE_DISTANCE_MODEL:
                Context->SourceDistanceModel = AL_FALSE;
                Context->UpdateSources = AL_TRUE;
                break;

            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALboolean AL_APIENTRY alIsEnabled(ALenum capability)
{
    ALCcontext *Context;
    ALboolean value=AL_FALSE;

    Context = GetContextRef();
    if(!Context) return AL_FALSE;

    al_try
    {
        switch(capability)
        {
            case AL_SOURCE_DISTANCE_MODEL:
                value = Context->SourceDistanceModel;
                break;

            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);

    return value;
}

AL_API ALboolean AL_APIENTRY alGetBoolean(ALenum pname)
{
    ALCcontext *Context;
    ALboolean value=AL_FALSE;

    Context = GetContextRef();
    if(!Context) return AL_FALSE;

    al_try
    {
        switch(pname)
        {
            case AL_DOPPLER_FACTOR:
                if(Context->DopplerFactor != 0.0f)
                    value = AL_TRUE;
                break;

            case AL_DOPPLER_VELOCITY:
                if(Context->DopplerVelocity != 0.0f)
                    value = AL_TRUE;
                break;

            case AL_DISTANCE_MODEL:
                if(Context->DistanceModel == AL_INVERSE_DISTANCE_CLAMPED)
                    value = AL_TRUE;
                break;

            case AL_SPEED_OF_SOUND:
                if(Context->SpeedOfSound != 0.0f)
                    value = AL_TRUE;
                break;

            case AL_DEFERRED_UPDATES_SOFT:
                value = Context->DeferUpdates;
                break;

            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);

    return value;
}

AL_API ALdouble AL_APIENTRY alGetDouble(ALenum pname)
{
    ALCcontext *Context;
    ALdouble value = 0.0;

    Context = GetContextRef();
    if(!Context) return 0.0;

    al_try
    {
        switch(pname)
        {
            case AL_DOPPLER_FACTOR:
                value = (ALdouble)Context->DopplerFactor;
                break;

            case AL_DOPPLER_VELOCITY:
                value = (ALdouble)Context->DopplerVelocity;
                break;

            case AL_DISTANCE_MODEL:
                value = (ALdouble)Context->DistanceModel;
                break;

            case AL_SPEED_OF_SOUND:
                value = (ALdouble)Context->SpeedOfSound;
                break;

            case AL_DEFERRED_UPDATES_SOFT:
                value = (ALdouble)Context->DeferUpdates;
                break;

            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);

    return value;
}

AL_API ALfloat AL_APIENTRY alGetFloat(ALenum pname)
{
    ALCcontext *Context;
    ALfloat value = 0.0f;

    Context = GetContextRef();
    if(!Context) return 0.0f;

    al_try
    {
        switch(pname)
        {
            case AL_DOPPLER_FACTOR:
                value = Context->DopplerFactor;
                break;

            case AL_DOPPLER_VELOCITY:
                value = Context->DopplerVelocity;
                break;

            case AL_DISTANCE_MODEL:
                value = (ALfloat)Context->DistanceModel;
                break;

            case AL_SPEED_OF_SOUND:
                value = Context->SpeedOfSound;
                break;

            case AL_DEFERRED_UPDATES_SOFT:
                value = (ALfloat)Context->DeferUpdates;
                break;

            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);

    return value;
}

AL_API ALint AL_APIENTRY alGetInteger(ALenum pname)
{
    ALCcontext *Context;
    ALint value = 0;

    Context = GetContextRef();
    if(!Context) return 0;

    al_try
    {
        switch(pname)
        {
            case AL_DOPPLER_FACTOR:
                value = (ALint)Context->DopplerFactor;
                break;

            case AL_DOPPLER_VELOCITY:
                value = (ALint)Context->DopplerVelocity;
                break;

            case AL_DISTANCE_MODEL:
                value = (ALint)Context->DistanceModel;
                break;

            case AL_SPEED_OF_SOUND:
                value = (ALint)Context->SpeedOfSound;
                break;

            case AL_DEFERRED_UPDATES_SOFT:
                value = (ALint)Context->DeferUpdates;
                break;

            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);

    return value;
}

AL_API ALvoid AL_APIENTRY alGetBooleanv(ALenum pname, ALboolean *values)
{
    ALCcontext *Context;

    if(values)
    {
        switch(pname)
        {
            case AL_DOPPLER_FACTOR:
            case AL_DOPPLER_VELOCITY:
            case AL_DISTANCE_MODEL:
            case AL_SPEED_OF_SOUND:
            case AL_DEFERRED_UPDATES_SOFT:
                values[0] = alGetBoolean(pname);
                return;
        }
    }

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, values);
        switch(pname)
        {
            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alGetDoublev(ALenum pname, ALdouble *values)
{
    ALCcontext *Context;

    if(values)
    {
        switch(pname)
        {
            case AL_DOPPLER_FACTOR:
            case AL_DOPPLER_VELOCITY:
            case AL_DISTANCE_MODEL:
            case AL_SPEED_OF_SOUND:
            case AL_DEFERRED_UPDATES_SOFT:
                values[0] = alGetDouble(pname);
                return;
        }
    }

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, values);
        switch(pname)
        {
            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alGetFloatv(ALenum pname, ALfloat *values)
{
    ALCcontext *Context;

    if(values)
    {
        switch(pname)
        {
            case AL_DOPPLER_FACTOR:
            case AL_DOPPLER_VELOCITY:
            case AL_DISTANCE_MODEL:
            case AL_SPEED_OF_SOUND:
            case AL_DEFERRED_UPDATES_SOFT:
                values[0] = alGetFloat(pname);
                return;
        }
    }

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, values);
        switch(pname)
        {
            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alGetIntegerv(ALenum pname, ALint *values)
{
    ALCcontext *Context;

    if(values)
    {
        switch(pname)
        {
            case AL_DOPPLER_FACTOR:
            case AL_DOPPLER_VELOCITY:
            case AL_DISTANCE_MODEL:
            case AL_SPEED_OF_SOUND:
            case AL_DEFERRED_UPDATES_SOFT:
                values[0] = alGetInteger(pname);
                return;
        }
    }

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, values);
        switch(pname)
        {
            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API const ALchar* AL_APIENTRY alGetString(ALenum pname)
{
    const ALchar *value = NULL;
    ALCcontext *Context;

    Context = GetContextRef();
    if(!Context) return NULL;

    al_try
    {
        switch(pname)
        {
            case AL_VENDOR:
                value = alVendor;
                break;

            case AL_VERSION:
                value = alVersion;
                break;

            case AL_RENDERER:
                value = alRenderer;
                break;

            case AL_EXTENSIONS:
                value = Context->ExtensionList;
                break;

            case AL_NO_ERROR:
                value = alNoError;
                break;

            case AL_INVALID_NAME:
                value = alErrInvalidName;
                break;

            case AL_INVALID_ENUM:
                value = alErrInvalidEnum;
                break;

            case AL_INVALID_VALUE:
                value = alErrInvalidValue;
                break;

            case AL_INVALID_OPERATION:
                value = alErrInvalidOp;
                break;

            case AL_OUT_OF_MEMORY:
                value = alErrOutOfMemory;
                break;

            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);

    return value;
}

AL_API ALvoid AL_APIENTRY alDopplerFactor(ALfloat value)
{
    ALCcontext *Context;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, value >= 0.0f && isfinite(value));

        Context->DopplerFactor = value;
        Context->UpdateSources = AL_TRUE;
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alDopplerVelocity(ALfloat value)
{
    ALCcontext *Context;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, value >= 0.0f && isfinite(value));

        Context->DopplerVelocity = value;
        Context->UpdateSources = AL_TRUE;
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alSpeedOfSound(ALfloat value)
{
    ALCcontext *Context;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, value > 0.0f && isfinite(value));

        Context->SpeedOfSound = value;
        Context->UpdateSources = AL_TRUE;
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alDistanceModel(ALenum value)
{
    ALCcontext *Context;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, value == AL_NONE ||
                             value == AL_INVERSE_DISTANCE ||
                             value == AL_INVERSE_DISTANCE_CLAMPED ||
                             value == AL_LINEAR_DISTANCE ||
                             value == AL_LINEAR_DISTANCE_CLAMPED ||
                             value == AL_EXPONENT_DISTANCE ||
                             value == AL_EXPONENT_DISTANCE_CLAMPED);

        Context->DistanceModel = value;
        if(!Context->SourceDistanceModel)
            Context->UpdateSources = AL_TRUE;
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alDeferUpdatesSOFT(void)
{
    ALCcontext *Context;

    Context = GetContextRef();
    if(!Context) return;

    if(!Context->DeferUpdates)
    {
        ALboolean UpdateSources;
        ALsource **src, **src_end;
        ALeffectslot **slot, **slot_end;
        FPUCtl oldMode;

        SetMixerFPUMode(&oldMode);

        LockContext(Context);
        Context->DeferUpdates = AL_TRUE;

        /* Make sure all pending updates are performed */
        UpdateSources = ExchangeInt(&Context->UpdateSources, AL_FALSE);

        src = Context->ActiveSources;
        src_end = src + Context->ActiveSourceCount;
        while(src != src_end)
        {
            if((*src)->state != AL_PLAYING)
            {
                Context->ActiveSourceCount--;
                *src = *(--src_end);
                continue;
            }

            if(ExchangeInt(&(*src)->NeedsUpdate, AL_FALSE) || UpdateSources)
                ALsource_Update(*src, Context);

            src++;
        }

        slot = Context->ActiveEffectSlots;
        slot_end = slot + Context->ActiveEffectSlotCount;
        while(slot != slot_end)
        {
            if(ExchangeInt(&(*slot)->NeedsUpdate, AL_FALSE))
                VCALL((*slot)->EffectState,update,(Context->Device, *slot));
            slot++;
        }

        UnlockContext(Context);
        RestoreFPUMode(&oldMode);
    }

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alProcessUpdatesSOFT(void)
{
    ALCcontext *Context;

    Context = GetContextRef();
    if(!Context) return;

    if(ExchangeInt(&Context->DeferUpdates, AL_FALSE))
    {
        ALsizei pos;

        LockContext(Context);
        LockUIntMapRead(&Context->SourceMap);
        for(pos = 0;pos < Context->SourceMap.size;pos++)
        {
            ALsource *Source = Context->SourceMap.array[pos].value;
            ALenum new_state;

            if((Source->state == AL_PLAYING || Source->state == AL_PAUSED) &&
               Source->Offset >= 0.0)
                ApplyOffset(Source);

            new_state = ExchangeInt(&Source->new_state, AL_NONE);
            if(new_state)
                SetSourceState(Source, Context, new_state);
        }
        UnlockUIntMapRead(&Context->SourceMap);
        UnlockContext(Context);
    }

    ALCcontext_DecRef(Context);
}
