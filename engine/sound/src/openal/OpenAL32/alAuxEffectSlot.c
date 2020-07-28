/**
 * OpenAL cross platform audio library
 * Copyright (C) 1999-2007 by authors.
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
#include <math.h>

#include "AL/al.h"
#include "AL/alc.h"
#include "alMain.h"
#include "alAuxEffectSlot.h"
#include "alThunk.h"
#include "alError.h"
#include "alSource.h"


static ALenum AddEffectSlotArray(ALCcontext *Context, ALsizei count, const ALuint *slots);
static ALvoid RemoveEffectSlotArray(ALCcontext *Context, ALeffectslot *slot);


static UIntMap EffectStateFactoryMap;
static inline ALeffectStateFactory *getFactoryByType(ALenum type)
{
    ALeffectStateFactory* (*getFactory)(void) = LookupUIntMapKey(&EffectStateFactoryMap, type);
    if(getFactory != NULL)
        return getFactory();
    return NULL;
}


AL_API ALvoid AL_APIENTRY alGenAuxiliaryEffectSlots(ALsizei n, ALuint *effectslots)
{
    ALCcontext *Context;
    ALsizei    cur = 0;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        ALenum err;

        CHECK_VALUE(Context, n >= 0);
        for(cur = 0;cur < n;cur++)
        {
            ALeffectslot *slot = al_calloc(16, sizeof(ALeffectslot));
            err = AL_OUT_OF_MEMORY;
            if(!slot || (err=InitEffectSlot(slot)) != AL_NO_ERROR)
            {
                al_free(slot);
                alDeleteAuxiliaryEffectSlots(cur, effectslots);
                al_throwerr(Context, err);
                break;
            }

            err = NewThunkEntry(&slot->id);
            if(err == AL_NO_ERROR)
                err = InsertUIntMapEntry(&Context->EffectSlotMap, slot->id, slot);
            if(err != AL_NO_ERROR)
            {
                FreeThunkEntry(slot->id);
                DELETE_OBJ(slot->EffectState);
                al_free(slot);

                alDeleteAuxiliaryEffectSlots(cur, effectslots);
                al_throwerr(Context, err);
            }

            effectslots[cur] = slot->id;
        }
        err = AddEffectSlotArray(Context, n, effectslots);
        if(err != AL_NO_ERROR)
        {
            alDeleteAuxiliaryEffectSlots(cur, effectslots);
            al_throwerr(Context, err);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alDeleteAuxiliaryEffectSlots(ALsizei n, const ALuint *effectslots)
{
    ALCcontext *Context;
    ALeffectslot *slot;
    ALsizei i;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, n >= 0);
        for(i = 0;i < n;i++)
        {
            if((slot=LookupEffectSlot(Context, effectslots[i])) == NULL)
                al_throwerr(Context, AL_INVALID_NAME);
            if(slot->ref != 0)
                al_throwerr(Context, AL_INVALID_OPERATION);
        }

        // All effectslots are valid
        for(i = 0;i < n;i++)
        {
            if((slot=RemoveEffectSlot(Context, effectslots[i])) == NULL)
                continue;
            FreeThunkEntry(slot->id);

            RemoveEffectSlotArray(Context, slot);
            DELETE_OBJ(slot->EffectState);

            memset(slot, 0, sizeof(*slot));
            al_free(slot);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALboolean AL_APIENTRY alIsAuxiliaryEffectSlot(ALuint effectslot)
{
    ALCcontext *Context;
    ALboolean  result;

    Context = GetContextRef();
    if(!Context) return AL_FALSE;

    result = (LookupEffectSlot(Context, effectslot) ? AL_TRUE : AL_FALSE);

    ALCcontext_DecRef(Context);

    return result;
}

AL_API ALvoid AL_APIENTRY alAuxiliaryEffectSloti(ALuint effectslot, ALenum param, ALint value)
{
    ALCcontext *Context;
    ALeffectslot *Slot;
    ALeffect *effect = NULL;
    ALenum err;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        ALCdevice *device = Context->Device;
        if((Slot=LookupEffectSlot(Context, effectslot)) == NULL)
            al_throwerr(Context, AL_INVALID_NAME);
        switch(param)
        {
        case AL_EFFECTSLOT_EFFECT:
            CHECK_VALUE(Context, value == 0 || (effect=LookupEffect(device, value)) != NULL);

            err = InitializeEffect(device, Slot, effect);
            if(err != AL_NO_ERROR)
                al_throwerr(Context, err);
            Context->UpdateSources = AL_TRUE;
            break;

        case AL_EFFECTSLOT_AUXILIARY_SEND_AUTO:
            CHECK_VALUE(Context, value == AL_TRUE || value == AL_FALSE);

            Slot->AuxSendAuto = value;
            Context->UpdateSources = AL_TRUE;
            break;

        default:
            al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alAuxiliaryEffectSlotiv(ALuint effectslot, ALenum param, const ALint *values)
{
    ALCcontext *Context;

    switch(param)
    {
        case AL_EFFECTSLOT_EFFECT:
        case AL_EFFECTSLOT_AUXILIARY_SEND_AUTO:
            alAuxiliaryEffectSloti(effectslot, param, values[0]);
            return;
    }

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        if(LookupEffectSlot(Context, effectslot) == NULL)
            al_throwerr(Context, AL_INVALID_NAME);
        switch(param)
        {
        default:
            al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alAuxiliaryEffectSlotf(ALuint effectslot, ALenum param, ALfloat value)
{
    ALCcontext *Context;
    ALeffectslot *Slot;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        if((Slot=LookupEffectSlot(Context, effectslot)) == NULL)
            al_throwerr(Context, AL_INVALID_NAME);
        switch(param)
        {
        case AL_EFFECTSLOT_GAIN:
            CHECK_VALUE(Context, value >= 0.0f && value <= 1.0f);

            Slot->Gain = value;
            Slot->NeedsUpdate = AL_TRUE;
            break;

        default:
            al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alAuxiliaryEffectSlotfv(ALuint effectslot, ALenum param, const ALfloat *values)
{
    ALCcontext *Context;

    switch(param)
    {
        case AL_EFFECTSLOT_GAIN:
            alAuxiliaryEffectSlotf(effectslot, param, values[0]);
            return;
    }

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        if(LookupEffectSlot(Context, effectslot) == NULL)
            al_throwerr(Context, AL_INVALID_NAME);
        switch(param)
        {
        default:
            al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alGetAuxiliaryEffectSloti(ALuint effectslot, ALenum param, ALint *value)
{
    ALCcontext *Context;
    ALeffectslot *Slot;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        if((Slot=LookupEffectSlot(Context, effectslot)) == NULL)
            al_throwerr(Context, AL_INVALID_NAME);
        switch(param)
        {
        case AL_EFFECTSLOT_AUXILIARY_SEND_AUTO:
            *value = Slot->AuxSendAuto;
            break;

        default:
            al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alGetAuxiliaryEffectSlotiv(ALuint effectslot, ALenum param, ALint *values)
{
    ALCcontext *Context;

    switch(param)
    {
        case AL_EFFECTSLOT_EFFECT:
        case AL_EFFECTSLOT_AUXILIARY_SEND_AUTO:
            alGetAuxiliaryEffectSloti(effectslot, param, values);
            return;
    }

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        if(LookupEffectSlot(Context, effectslot) == NULL)
            al_throwerr(Context, AL_INVALID_NAME);
        switch(param)
        {
        default:
            al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alGetAuxiliaryEffectSlotf(ALuint effectslot, ALenum param, ALfloat *value)
{
    ALCcontext *Context;
    ALeffectslot *Slot;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        if((Slot=LookupEffectSlot(Context, effectslot)) == NULL)
            al_throwerr(Context, AL_INVALID_NAME);
        switch(param)
        {
        case AL_EFFECTSLOT_GAIN:
            *value = Slot->Gain;
            break;

        default:
            al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alGetAuxiliaryEffectSlotfv(ALuint effectslot, ALenum param, ALfloat *values)
{
    ALCcontext *Context;

    switch(param)
    {
        case AL_EFFECTSLOT_GAIN:
            alGetAuxiliaryEffectSlotf(effectslot, param, values);
            return;
    }

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        if(LookupEffectSlot(Context, effectslot) == NULL)
            al_throwerr(Context, AL_INVALID_NAME);
        switch(param)
        {
        default:
            al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}


static ALvoid RemoveEffectSlotArray(ALCcontext *Context, ALeffectslot *slot)
{
    ALeffectslot **slotlist, **slotlistend;

    LockContext(Context);
    slotlist = Context->ActiveEffectSlots;
    slotlistend = slotlist + Context->ActiveEffectSlotCount;
    while(slotlist != slotlistend)
    {
        if(*slotlist == slot)
        {
            *slotlist = *(--slotlistend);
            Context->ActiveEffectSlotCount--;
            break;
        }
        slotlist++;
    }
    UnlockContext(Context);
}

static ALenum AddEffectSlotArray(ALCcontext *Context, ALsizei count, const ALuint *slots)
{
    ALsizei i;

    LockContext(Context);
    if(count > Context->MaxActiveEffectSlots-Context->ActiveEffectSlotCount)
    {
        ALsizei newcount;
        void *temp = NULL;

        newcount = Context->MaxActiveEffectSlots ? (Context->MaxActiveEffectSlots<<1) : 1;
        if(newcount > Context->MaxActiveEffectSlots)
            temp = realloc(Context->ActiveEffectSlots,
                           newcount * sizeof(*Context->ActiveEffectSlots));
        if(!temp)
        {
            UnlockContext(Context);
            return AL_OUT_OF_MEMORY;
        }
        Context->ActiveEffectSlots = temp;
        Context->MaxActiveEffectSlots = newcount;
    }
    for(i = 0;i < count;i++)
    {
        ALeffectslot *slot = LookupEffectSlot(Context, slots[i]);
        assert(slot != NULL);
        Context->ActiveEffectSlots[Context->ActiveEffectSlotCount++] = slot;
    }
    UnlockContext(Context);
    return AL_NO_ERROR;
}


void InitEffectFactoryMap(void)
{
    InitUIntMap(&EffectStateFactoryMap, ~0);

    InsertUIntMapEntry(&EffectStateFactoryMap, AL_EFFECT_NULL, ALnullStateFactory_getFactory);
    InsertUIntMapEntry(&EffectStateFactoryMap, AL_EFFECT_EAXREVERB, ALreverbStateFactory_getFactory);
    InsertUIntMapEntry(&EffectStateFactoryMap, AL_EFFECT_REVERB, ALreverbStateFactory_getFactory);
    InsertUIntMapEntry(&EffectStateFactoryMap, AL_EFFECT_CHORUS, ALchorusStateFactory_getFactory);
    InsertUIntMapEntry(&EffectStateFactoryMap, AL_EFFECT_DISTORTION, ALdistortionStateFactory_getFactory);
    InsertUIntMapEntry(&EffectStateFactoryMap, AL_EFFECT_ECHO, ALechoStateFactory_getFactory);
    InsertUIntMapEntry(&EffectStateFactoryMap, AL_EFFECT_EQUALIZER, ALequalizerStateFactory_getFactory);
    InsertUIntMapEntry(&EffectStateFactoryMap, AL_EFFECT_FLANGER, ALflangerStateFactory_getFactory);
    InsertUIntMapEntry(&EffectStateFactoryMap, AL_EFFECT_RING_MODULATOR, ALmodulatorStateFactory_getFactory);
    InsertUIntMapEntry(&EffectStateFactoryMap, AL_EFFECT_DEDICATED_DIALOGUE, ALdedicatedStateFactory_getFactory);
    InsertUIntMapEntry(&EffectStateFactoryMap, AL_EFFECT_DEDICATED_LOW_FREQUENCY_EFFECT, ALdedicatedStateFactory_getFactory);
}

void DeinitEffectFactoryMap(void)
{
    ResetUIntMap(&EffectStateFactoryMap);
}


ALenum InitializeEffect(ALCdevice *Device, ALeffectslot *EffectSlot, ALeffect *effect)
{
    ALenum newtype = (effect ? effect->type : AL_EFFECT_NULL);
    ALeffectStateFactory *factory;

    if(newtype != EffectSlot->EffectType)
    {
        ALeffectState *State;
        FPUCtl oldMode;

        factory = getFactoryByType(newtype);
        if(!factory)
        {
            ERR("Failed to find factory for effect type 0x%04x\n", newtype);
            return AL_INVALID_ENUM;
        }
        State = VCALL_NOARGS(factory,create);
        if(!State)
            return AL_OUT_OF_MEMORY;

        SetMixerFPUMode(&oldMode);

        ALCdevice_Lock(Device);
        if(VCALL(State,deviceUpdate,(Device)) == AL_FALSE)
        {
            ALCdevice_Unlock(Device);
            RestoreFPUMode(&oldMode);
            DELETE_OBJ(State);
            return AL_OUT_OF_MEMORY;
        }

        State = ExchangePtr((XchgPtr*)&EffectSlot->EffectState, State);
        if(!effect)
        {
            memset(&EffectSlot->EffectProps, 0, sizeof(EffectSlot->EffectProps));
            EffectSlot->EffectType = AL_EFFECT_NULL;
        }
        else
        {
            memcpy(&EffectSlot->EffectProps, &effect->Props, sizeof(effect->Props));
            EffectSlot->EffectType = effect->type;
        }

        /* FIXME: This should be done asynchronously, but since the EffectState
         * object was changed, it needs an update before its Process method can
         * be called. */
        EffectSlot->NeedsUpdate = AL_FALSE;
        VCALL(EffectSlot->EffectState,update,(Device, EffectSlot));
        ALCdevice_Unlock(Device);

        RestoreFPUMode(&oldMode);

        DELETE_OBJ(State);
        State = NULL;
    }
    else
    {
        if(effect)
        {
            ALCdevice_Lock(Device);
            memcpy(&EffectSlot->EffectProps, &effect->Props, sizeof(effect->Props));
            ALCdevice_Unlock(Device);
            EffectSlot->NeedsUpdate = AL_TRUE;
        }
    }

    return AL_NO_ERROR;
}


ALenum InitEffectSlot(ALeffectslot *slot)
{
    ALeffectStateFactory *factory;
    ALint i, c;

    slot->EffectType = AL_EFFECT_NULL;

    factory = getFactoryByType(AL_EFFECT_NULL);
    if(!(slot->EffectState=VCALL_NOARGS(factory,create)))
        return AL_OUT_OF_MEMORY;

    slot->Gain = 1.0;
    slot->AuxSendAuto = AL_TRUE;
    slot->NeedsUpdate = AL_FALSE;
    for(c = 0;c < 1;c++)
    {
        for(i = 0;i < BUFFERSIZE;i++)
            slot->WetBuffer[c][i] = 0.0f;
        slot->ClickRemoval[c] = 0.0f;
        slot->PendingClicks[c] = 0.0f;
    }
    slot->ref = 0;

    return AL_NO_ERROR;
}

ALvoid ReleaseALAuxiliaryEffectSlots(ALCcontext *Context)
{
    ALsizei pos;
    for(pos = 0;pos < Context->EffectSlotMap.size;pos++)
    {
        ALeffectslot *temp = Context->EffectSlotMap.array[pos].value;
        Context->EffectSlotMap.array[pos].value = NULL;

        DELETE_OBJ(temp->EffectState);

        FreeThunkEntry(temp->id);
        memset(temp, 0, sizeof(ALeffectslot));
        al_free(temp);
    }
}
