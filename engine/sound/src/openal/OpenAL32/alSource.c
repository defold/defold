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
#include <float.h>

#include "AL/al.h"
#include "AL/alc.h"
#include "alMain.h"
#include "alError.h"
#include "alSource.h"
#include "alBuffer.h"
#include "alThunk.h"
#include "alAuxEffectSlot.h"


enum Resampler DefaultResampler = LinearResampler;
const ALsizei ResamplerPadding[ResamplerMax] = {
    0, /* Point */
    1, /* Linear */
    2, /* Cubic */
};
const ALsizei ResamplerPrePadding[ResamplerMax] = {
    0, /* Point */
    0, /* Linear */
    1, /* Cubic */
};


static ALvoid InitSourceParams(ALsource *Source);
static ALint64 GetSourceOffset(const ALsource *Source);
static ALdouble GetSourceSecOffset(const ALsource *Source);
static ALvoid GetSourceOffsets(const ALsource *Source, ALenum name, ALdouble *offsets, ALdouble updateLen);
static ALint GetSampleOffset(ALsource *Source);

typedef enum SrcFloatProp {
    sfPitch = AL_PITCH,
    sfGain = AL_GAIN,
    sfMinGain = AL_MIN_GAIN,
    sfMaxGain = AL_MAX_GAIN,
    sfMaxDistance = AL_MAX_DISTANCE,
    sfRolloffFactor = AL_ROLLOFF_FACTOR,
    sfDopplerFactor = AL_DOPPLER_FACTOR,
    sfConeOuterGain = AL_CONE_OUTER_GAIN,
    sfSecOffset = AL_SEC_OFFSET,
    sfSampleOffset = AL_SAMPLE_OFFSET,
    sfByteOffset = AL_BYTE_OFFSET,
    sfConeInnerAngle = AL_CONE_INNER_ANGLE,
    sfConeOuterAngle = AL_CONE_OUTER_ANGLE,
    sfRefDistance = AL_REFERENCE_DISTANCE,

    sfPosition = AL_POSITION,
    sfVelocity = AL_VELOCITY,
    sfDirection = AL_DIRECTION,

    sfSourceRelative = AL_SOURCE_RELATIVE,
    sfLooping = AL_LOOPING,
    sfBuffer = AL_BUFFER,
    sfSourceState = AL_SOURCE_STATE,
    sfBuffersQueued = AL_BUFFERS_QUEUED,
    sfBuffersProcessed = AL_BUFFERS_PROCESSED,
    sfSourceType = AL_SOURCE_TYPE,

    /* ALC_EXT_EFX */
    sfConeOuterGainHF = AL_CONE_OUTER_GAINHF,
    sfAirAbsorptionFactor = AL_AIR_ABSORPTION_FACTOR,
    sfRoomRolloffFactor =  AL_ROOM_ROLLOFF_FACTOR,
    sfDirectFilterGainHFAuto = AL_DIRECT_FILTER_GAINHF_AUTO,
    sfAuxSendFilterGainAuto = AL_AUXILIARY_SEND_FILTER_GAIN_AUTO,
    sfAuxSendFilterGainHFAuto = AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO,

    /* AL_SOFT_direct_channels */
    sfDirectChannelsSOFT = AL_DIRECT_CHANNELS_SOFT,

    /* AL_EXT_source_distance_model */
    sfDistanceModel = AL_DISTANCE_MODEL,

    /* AL_SOFT_buffer_sub_data / AL_SOFT_buffer_samples */
    sfSampleRWOffsetsSOFT = AL_SAMPLE_RW_OFFSETS_SOFT,
    sfByteRWOffsetsSOFT = AL_BYTE_RW_OFFSETS_SOFT,

    /* AL_SOFT_source_latency */
    sfSecOffsetLatencySOFT = AL_SEC_OFFSET_LATENCY_SOFT,
} SrcFloatProp;

typedef enum SrcIntProp {
    siMaxDistance = AL_MAX_DISTANCE,
    siRolloffFactor = AL_ROLLOFF_FACTOR,
    siRefDistance = AL_REFERENCE_DISTANCE,
    siSourceRelative = AL_SOURCE_RELATIVE,
    siConeInnerAngle = AL_CONE_INNER_ANGLE,
    siConeOuterAngle = AL_CONE_OUTER_ANGLE,
    siLooping = AL_LOOPING,
    siBuffer = AL_BUFFER,
    siSourceState = AL_SOURCE_STATE,
    siBuffersQueued = AL_BUFFERS_QUEUED,
    siBuffersProcessed = AL_BUFFERS_PROCESSED,
    siSourceType = AL_SOURCE_TYPE,
    siSecOffset = AL_SEC_OFFSET,
    siSampleOffset = AL_SAMPLE_OFFSET,
    siByteOffset = AL_BYTE_OFFSET,
    siDopplerFactor = AL_DOPPLER_FACTOR,
    siPosition = AL_POSITION,
    siVelocity = AL_VELOCITY,
    siDirection = AL_DIRECTION,

    /* ALC_EXT_EFX */
    siDirectFilterGainHFAuto = AL_DIRECT_FILTER_GAINHF_AUTO,
    siAuxSendFilterGainAutio = AL_AUXILIARY_SEND_FILTER_GAIN_AUTO,
    siAuxSendFilterGainHFAuto = AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO,
    siDirectFilter = AL_DIRECT_FILTER,
    siAuxSendFilter = AL_AUXILIARY_SEND_FILTER,

    /* AL_SOFT_direct_channels */
    siDirectChannelsSOFT = AL_DIRECT_CHANNELS_SOFT,

    /* AL_EXT_source_distance_model */
    siDistanceModel = AL_DISTANCE_MODEL,

    /* AL_SOFT_buffer_sub_data / AL_SOFT_buffer_samples */
    siSampleRWOffsetsSOFT = AL_SAMPLE_RW_OFFSETS_SOFT,
    siByteRWOffsetsSOFT = AL_BYTE_RW_OFFSETS_SOFT,

    /* AL_SOFT_source_latency */
    siSampleOffsetLatencySOFT = AL_SAMPLE_OFFSET_LATENCY_SOFT,
} SrcIntProp;

static ALenum SetSourcefv(ALsource *Source, ALCcontext *Context, SrcFloatProp prop, const ALfloat *values);
static ALenum SetSourceiv(ALsource *Source, ALCcontext *Context, SrcIntProp prop, const ALint *values);
static ALenum SetSourcei64v(ALsource *Source, ALCcontext *Context, SrcIntProp prop, const ALint64SOFT *values);

static ALenum GetSourcedv(const ALsource *Source, ALCcontext *Context, SrcFloatProp prop, ALdouble *values);
static ALenum GetSourceiv(const ALsource *Source, ALCcontext *Context, SrcIntProp prop, ALint *values);
static ALenum GetSourcei64v(const ALsource *Source, ALCcontext *Context, SrcIntProp prop, ALint64 *values);

static ALint FloatValsByProp(ALenum prop)
{
    if(prop != (ALenum)((SrcFloatProp)prop))
        return 0;
    switch((SrcFloatProp)prop)
    {
        case sfPitch:
        case sfGain:
        case sfMinGain:
        case sfMaxGain:
        case sfMaxDistance:
        case sfRolloffFactor:
        case sfDopplerFactor:
        case sfConeOuterGain:
        case sfSecOffset:
        case sfSampleOffset:
        case sfByteOffset:
        case sfConeInnerAngle:
        case sfConeOuterAngle:
        case sfRefDistance:
        case sfConeOuterGainHF:
        case sfAirAbsorptionFactor:
        case sfRoomRolloffFactor:
        case sfDirectFilterGainHFAuto:
        case sfAuxSendFilterGainAuto:
        case sfAuxSendFilterGainHFAuto:
        case sfDirectChannelsSOFT:
        case sfDistanceModel:
        case sfSourceRelative:
        case sfLooping:
        case sfBuffer:
        case sfSourceState:
        case sfBuffersQueued:
        case sfBuffersProcessed:
        case sfSourceType:
            return 1;

        case sfSampleRWOffsetsSOFT:
        case sfByteRWOffsetsSOFT:
            return 2;

        case sfPosition:
        case sfVelocity:
        case sfDirection:
            return 3;

        case sfSecOffsetLatencySOFT:
            break; /* Double only */
    }
    return 0;
}
static ALint DoubleValsByProp(ALenum prop)
{
    if(prop != (ALenum)((SrcFloatProp)prop))
        return 0;
    switch((SrcFloatProp)prop)
    {
        case sfPitch:
        case sfGain:
        case sfMinGain:
        case sfMaxGain:
        case sfMaxDistance:
        case sfRolloffFactor:
        case sfDopplerFactor:
        case sfConeOuterGain:
        case sfSecOffset:
        case sfSampleOffset:
        case sfByteOffset:
        case sfConeInnerAngle:
        case sfConeOuterAngle:
        case sfRefDistance:
        case sfConeOuterGainHF:
        case sfAirAbsorptionFactor:
        case sfRoomRolloffFactor:
        case sfDirectFilterGainHFAuto:
        case sfAuxSendFilterGainAuto:
        case sfAuxSendFilterGainHFAuto:
        case sfDirectChannelsSOFT:
        case sfDistanceModel:
        case sfSourceRelative:
        case sfLooping:
        case sfBuffer:
        case sfSourceState:
        case sfBuffersQueued:
        case sfBuffersProcessed:
        case sfSourceType:
            return 1;

        case sfSampleRWOffsetsSOFT:
        case sfByteRWOffsetsSOFT:
        case sfSecOffsetLatencySOFT:
            return 2;

        case sfPosition:
        case sfVelocity:
        case sfDirection:
            return 3;
    }
    return 0;
}

static ALint IntValsByProp(ALenum prop)
{
    if(prop != (ALenum)((SrcIntProp)prop))
        return 0;
    switch((SrcIntProp)prop)
    {
        case siMaxDistance:
        case siRolloffFactor:
        case siRefDistance:
        case siSourceRelative:
        case siConeInnerAngle:
        case siConeOuterAngle:
        case siLooping:
        case siBuffer:
        case siSourceState:
        case siBuffersQueued:
        case siBuffersProcessed:
        case siSourceType:
        case siSecOffset:
        case siSampleOffset:
        case siByteOffset:
        case siDopplerFactor:
        case siDirectFilterGainHFAuto:
        case siAuxSendFilterGainAutio:
        case siAuxSendFilterGainHFAuto:
        case siDirectFilter:
        case siDirectChannelsSOFT:
        case siDistanceModel:
            return 1;

        case siSampleRWOffsetsSOFT:
        case siByteRWOffsetsSOFT:
            return 2;

        case siPosition:
        case siVelocity:
        case siDirection:
        case siAuxSendFilter:
            return 3;

        case siSampleOffsetLatencySOFT:
            break; /* i64 only */
    }
    return 0;
}
static ALint Int64ValsByProp(ALenum prop)
{
    if(prop != (ALenum)((SrcIntProp)prop))
        return 0;
    switch((SrcIntProp)prop)
    {
        case siMaxDistance:
        case siRolloffFactor:
        case siRefDistance:
        case siSourceRelative:
        case siConeInnerAngle:
        case siConeOuterAngle:
        case siLooping:
        case siBuffer:
        case siSourceState:
        case siBuffersQueued:
        case siBuffersProcessed:
        case siSourceType:
        case siSecOffset:
        case siSampleOffset:
        case siByteOffset:
        case siDopplerFactor:
        case siDirectFilterGainHFAuto:
        case siAuxSendFilterGainAutio:
        case siAuxSendFilterGainHFAuto:
        case siDirectFilter:
        case siDirectChannelsSOFT:
        case siDistanceModel:
            return 1;

        case siSampleRWOffsetsSOFT:
        case siByteRWOffsetsSOFT:
        case siSampleOffsetLatencySOFT:
            return 2;

        case siPosition:
        case siVelocity:
        case siDirection:
        case siAuxSendFilter:
            return 3;
    }
    return 0;
}


#define RETERR(x) do {                                                        \
    alSetError(Context, (x));                                                 \
    return (x);                                                               \
} while(0)

#define CHECKVAL(x) do {                                                      \
    if(!(x))                                                                  \
        RETERR(AL_INVALID_VALUE);                                             \
} while(0)

static ALenum SetSourcefv(ALsource *Source, ALCcontext *Context, SrcFloatProp prop, const ALfloat *values)
{
    ALint ival;

    switch(prop)
    {
        case AL_PITCH:
            CHECKVAL(*values >= 0.0f);

            Source->Pitch = *values;
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_CONE_INNER_ANGLE:
            CHECKVAL(*values >= 0.0f && *values <= 360.0f);

            Source->InnerAngle = *values;
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_CONE_OUTER_ANGLE:
            CHECKVAL(*values >= 0.0f && *values <= 360.0f);

            Source->OuterAngle = *values;
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_GAIN:
            CHECKVAL(*values >= 0.0f);

            Source->Gain = *values;
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_MAX_DISTANCE:
            CHECKVAL(*values >= 0.0f);

            Source->MaxDistance = *values;
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_ROLLOFF_FACTOR:
            CHECKVAL(*values >= 0.0f);

            Source->RollOffFactor = *values;
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_REFERENCE_DISTANCE:
            CHECKVAL(*values >= 0.0f);

            Source->RefDistance = *values;
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_MIN_GAIN:
            CHECKVAL(*values >= 0.0f && *values <= 1.0f);

            Source->MinGain = *values;
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_MAX_GAIN:
            CHECKVAL(*values >= 0.0f && *values <= 1.0f);

            Source->MaxGain = *values;
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_CONE_OUTER_GAIN:
            CHECKVAL(*values >= 0.0f && *values <= 1.0f);

            Source->OuterGain = *values;
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_CONE_OUTER_GAINHF:
            CHECKVAL(*values >= 0.0f && *values <= 1.0f);

            Source->OuterGainHF = *values;
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_AIR_ABSORPTION_FACTOR:
            CHECKVAL(*values >= 0.0f && *values <= 10.0f);

            Source->AirAbsorptionFactor = *values;
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_ROOM_ROLLOFF_FACTOR:
            CHECKVAL(*values >= 0.0f && *values <= 10.0f);

            Source->RoomRolloffFactor = *values;
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_DOPPLER_FACTOR:
            CHECKVAL(*values >= 0.0f && *values <= 1.0f);

            Source->DopplerFactor = *values;
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_SEC_OFFSET:
        case AL_SAMPLE_OFFSET:
        case AL_BYTE_OFFSET:
            CHECKVAL(*values >= 0.0f);

            LockContext(Context);
            Source->OffsetType = prop;
            Source->Offset = *values;

            if((Source->state == AL_PLAYING || Source->state == AL_PAUSED) &&
               !Context->DeferUpdates)
            {
                if(ApplyOffset(Source) == AL_FALSE)
                {
                    UnlockContext(Context);
                    RETERR(AL_INVALID_VALUE);
                }
            }
            UnlockContext(Context);
            return AL_NO_ERROR;


        case AL_SEC_OFFSET_LATENCY_SOFT:
            /* Query only */
            RETERR(AL_INVALID_OPERATION);


        case AL_POSITION:
            CHECKVAL(isfinite(values[0]) && isfinite(values[1]) && isfinite(values[2]));

            LockContext(Context);
            Source->Position[0] = values[0];
            Source->Position[1] = values[1];
            Source->Position[2] = values[2];
            UnlockContext(Context);
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_VELOCITY:
            CHECKVAL(isfinite(values[0]) && isfinite(values[1]) && isfinite(values[2]));

            LockContext(Context);
            Source->Velocity[0] = values[0];
            Source->Velocity[1] = values[1];
            Source->Velocity[2] = values[2];
            UnlockContext(Context);
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_DIRECTION:
            CHECKVAL(isfinite(values[0]) && isfinite(values[1]) && isfinite(values[2]));

            LockContext(Context);
            Source->Orientation[0] = values[0];
            Source->Orientation[1] = values[1];
            Source->Orientation[2] = values[2];
            UnlockContext(Context);
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;


        case sfSampleRWOffsetsSOFT:
        case sfByteRWOffsetsSOFT:
            RETERR(AL_INVALID_OPERATION);


        case sfSourceRelative:
        case sfLooping:
        case sfSourceState:
        case sfSourceType:
        case sfDistanceModel:
        case sfDirectFilterGainHFAuto:
        case sfAuxSendFilterGainAuto:
        case sfAuxSendFilterGainHFAuto:
        case sfDirectChannelsSOFT:
            ival = (ALint)values[0];
            return SetSourceiv(Source, Context, (SrcIntProp)prop, &ival);

        case sfBuffer:
        case sfBuffersQueued:
        case sfBuffersProcessed:
            ival = (ALint)((ALuint)values[0]);
            return SetSourceiv(Source, Context, (SrcIntProp)prop, &ival);
    }

    ERR("Unexpected property: 0x%04x\n", prop);
    RETERR(AL_INVALID_ENUM);
}

static ALenum SetSourceiv(ALsource *Source, ALCcontext *Context, SrcIntProp prop, const ALint *values)
{
    ALCdevice *device = Context->Device;
    ALbuffer  *buffer = NULL;
    ALfilter  *filter = NULL;
    ALeffectslot *slot = NULL;
    ALbufferlistitem *oldlist;
    ALfloat fvals[3];

    switch(prop)
    {
        case AL_SOURCE_RELATIVE:
            CHECKVAL(*values == AL_FALSE || *values == AL_TRUE);

            Source->HeadRelative = (ALboolean)*values;
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_LOOPING:
            CHECKVAL(*values == AL_FALSE || *values == AL_TRUE);

            Source->Looping = (ALboolean)*values;
            return AL_NO_ERROR;

        case AL_BUFFER:
            CHECKVAL(*values == 0 || (buffer=LookupBuffer(device, *values)) != NULL);

            LockContext(Context);
            if(!(Source->state == AL_STOPPED || Source->state == AL_INITIAL))
            {
                UnlockContext(Context);
                RETERR(AL_INVALID_OPERATION);
            }

            Source->BuffersInQueue = 0;
            Source->BuffersPlayed = 0;

            if(buffer != NULL)
            {
                ALbufferlistitem *BufferListItem;

                /* Source is now Static */
                Source->SourceType = AL_STATIC;

                /* Add the selected buffer to a one-item queue */
                BufferListItem = malloc(sizeof(ALbufferlistitem));
                BufferListItem->buffer = buffer;
                BufferListItem->next = NULL;
                BufferListItem->prev = NULL;
                IncrementRef(&buffer->ref);

                oldlist = ExchangePtr((XchgPtr*)&Source->queue, BufferListItem);
                Source->BuffersInQueue = 1;

                ReadLock(&buffer->lock);
                Source->NumChannels = ChannelsFromFmt(buffer->FmtChannels);
                Source->SampleSize  = BytesFromFmt(buffer->FmtType);
                ReadUnlock(&buffer->lock);
                if(buffer->FmtChannels == FmtMono)
                    Source->Update = CalcSourceParams;
                else
                    Source->Update = CalcNonAttnSourceParams;
                Source->NeedsUpdate = AL_TRUE;
            }
            else
            {
                /* Source is now Undetermined */
                Source->SourceType = AL_UNDETERMINED;
                oldlist = ExchangePtr((XchgPtr*)&Source->queue, NULL);
            }

            /* Delete all elements in the previous queue */
            while(oldlist != NULL)
            {
                ALbufferlistitem *temp = oldlist;
                oldlist = temp->next;

                if(temp->buffer)
                    DecrementRef(&temp->buffer->ref);
                free(temp);
            }
            UnlockContext(Context);
            return AL_NO_ERROR;

        case siSourceState:
        case siSourceType:
        case siBuffersQueued:
        case siBuffersProcessed:
            /* Query only */
            RETERR(AL_INVALID_OPERATION);

        case AL_SEC_OFFSET:
        case AL_SAMPLE_OFFSET:
        case AL_BYTE_OFFSET:
            CHECKVAL(*values >= 0);

            LockContext(Context);
            Source->OffsetType = prop;
            Source->Offset = *values;

            if((Source->state == AL_PLAYING || Source->state == AL_PAUSED) &&
                !Context->DeferUpdates)
            {
                if(ApplyOffset(Source) == AL_FALSE)
                {
                    UnlockContext(Context);
                    RETERR(AL_INVALID_VALUE);
                }
            }
            UnlockContext(Context);
            return AL_NO_ERROR;


        case siSampleRWOffsetsSOFT:
        case siByteRWOffsetsSOFT:
            /* Query only */
            RETERR(AL_INVALID_OPERATION);


        case AL_DIRECT_FILTER:
            CHECKVAL(*values == 0 || (filter=LookupFilter(device, *values)) != NULL);

            LockContext(Context);
            if(!filter)
            {
                Source->DirectGain = 1.0f;
                Source->DirectGainHF = 1.0f;
            }
            else
            {
                Source->DirectGain = filter->Gain;
                Source->DirectGainHF = filter->GainHF;
            }
            UnlockContext(Context);
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_DIRECT_FILTER_GAINHF_AUTO:
            CHECKVAL(*values == AL_FALSE || *values == AL_TRUE);

            Source->DryGainHFAuto = *values;
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_AUXILIARY_SEND_FILTER_GAIN_AUTO:
            CHECKVAL(*values == AL_FALSE || *values == AL_TRUE);

            Source->WetGainAuto = *values;
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO:
            CHECKVAL(*values == AL_FALSE || *values == AL_TRUE);

            Source->WetGainHFAuto = *values;
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_DIRECT_CHANNELS_SOFT:
            CHECKVAL(*values == AL_FALSE || *values == AL_TRUE);

            Source->DirectChannels = *values;
            Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;

        case AL_DISTANCE_MODEL:
            CHECKVAL(*values == AL_NONE ||
                     *values == AL_INVERSE_DISTANCE ||
                     *values == AL_INVERSE_DISTANCE_CLAMPED ||
                     *values == AL_LINEAR_DISTANCE ||
                     *values == AL_LINEAR_DISTANCE_CLAMPED ||
                     *values == AL_EXPONENT_DISTANCE ||
                     *values == AL_EXPONENT_DISTANCE_CLAMPED);

            Source->DistanceModel = *values;
            if(Context->SourceDistanceModel)
                Source->NeedsUpdate = AL_TRUE;
            return AL_NO_ERROR;


        case AL_AUXILIARY_SEND_FILTER:
            LockContext(Context);
            if(!((ALuint)values[1] < device->NumAuxSends &&
                 (values[0] == 0 || (slot=LookupEffectSlot(Context, values[0])) != NULL) &&
                 (values[2] == 0 || (filter=LookupFilter(device, values[2])) != NULL)))
            {
                UnlockContext(Context);
                RETERR(AL_INVALID_VALUE);
            }

            /* Add refcount on the new slot, and release the previous slot */
            if(slot) IncrementRef(&slot->ref);
            slot = ExchangePtr((XchgPtr*)&Source->Send[values[1]].Slot, slot);
            if(slot) DecrementRef(&slot->ref);

            if(!filter)
            {
                /* Disable filter */
                Source->Send[values[1]].Gain = 1.0f;
                Source->Send[values[1]].GainHF = 1.0f;
            }
            else
            {
                Source->Send[values[1]].Gain = filter->Gain;
                Source->Send[values[1]].GainHF = filter->GainHF;
            }
            Source->NeedsUpdate = AL_TRUE;
            UnlockContext(Context);
            return AL_NO_ERROR;


        case AL_MAX_DISTANCE:
        case AL_ROLLOFF_FACTOR:
        case AL_CONE_INNER_ANGLE:
        case AL_CONE_OUTER_ANGLE:
        case AL_REFERENCE_DISTANCE:
        case siDopplerFactor:
            fvals[0] = (ALfloat)*values;
            return SetSourcefv(Source, Context, (int)prop, fvals);

        case AL_POSITION:
        case AL_VELOCITY:
        case AL_DIRECTION:
            fvals[0] = (ALfloat)values[0];
            fvals[1] = (ALfloat)values[1];
            fvals[2] = (ALfloat)values[2];
            return SetSourcefv(Source, Context, (int)prop, fvals);

        case siSampleOffsetLatencySOFT:
            /* i64 only */
            break;
    }

    ERR("Unexpected property: 0x%04x\n", prop);
    RETERR(AL_INVALID_ENUM);
}

static ALenum SetSourcei64v(ALsource *Source, ALCcontext *Context, SrcIntProp prop, const ALint64SOFT *values)
{
    ALfloat fvals[3];
    ALint   ivals[3];

    switch(prop)
    {
        case siSampleRWOffsetsSOFT:
        case siByteRWOffsetsSOFT:
        case siSampleOffsetLatencySOFT:
            /* Query only */
            RETERR(AL_INVALID_OPERATION);


        /* 1x int */
        case AL_SOURCE_RELATIVE:
        case AL_LOOPING:
        case AL_SOURCE_STATE:
        case AL_BYTE_OFFSET:
        case AL_SAMPLE_OFFSET:
        case siSourceType:
        case siBuffersQueued:
        case siBuffersProcessed:
        case AL_DIRECT_FILTER_GAINHF_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAIN_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO:
        case AL_DIRECT_CHANNELS_SOFT:
        case AL_DISTANCE_MODEL:
            CHECKVAL(*values <= INT_MAX && *values >= INT_MIN);

            ivals[0] = (ALint)*values;
            return SetSourceiv(Source, Context, (int)prop, ivals);

        /* 1x uint */
        case AL_BUFFER:
        case AL_DIRECT_FILTER:
            CHECKVAL(*values <= UINT_MAX && *values >= 0);

            ivals[0] = (ALuint)*values;
            return SetSourceiv(Source, Context, (int)prop, ivals);

        /* 3x uint */
        case AL_AUXILIARY_SEND_FILTER:
            CHECKVAL(values[0] <= UINT_MAX && values[0] >= 0 &&
                     values[1] <= UINT_MAX && values[1] >= 0 &&
                     values[2] <= UINT_MAX && values[2] >= 0);

            ivals[0] = (ALuint)values[0];
            ivals[1] = (ALuint)values[1];
            ivals[2] = (ALuint)values[2];
            return SetSourceiv(Source, Context, (int)prop, ivals);

        /* 1x float */
        case AL_MAX_DISTANCE:
        case AL_ROLLOFF_FACTOR:
        case AL_CONE_INNER_ANGLE:
        case AL_CONE_OUTER_ANGLE:
        case AL_REFERENCE_DISTANCE:
        case AL_SEC_OFFSET:
        case siDopplerFactor:
            fvals[0] = (ALfloat)*values;
            return SetSourcefv(Source, Context, (int)prop, fvals);

        /* 3x float */
        case AL_POSITION:
        case AL_VELOCITY:
        case AL_DIRECTION:
            fvals[0] = (ALfloat)values[0];
            fvals[1] = (ALfloat)values[1];
            fvals[2] = (ALfloat)values[2];
            return SetSourcefv(Source, Context, (int)prop, fvals);
    }

    ERR("Unexpected property: 0x%04x\n", prop);
    RETERR(AL_INVALID_ENUM);
}

#undef CHECKVAL


static ALenum GetSourcedv(const ALsource *Source, ALCcontext *Context, SrcFloatProp prop, ALdouble *values)
{
    ALdouble offsets[2];
    ALdouble updateLen;
    ALint    ivals[3];
    ALenum   err;

    switch(prop)
    {
        case AL_GAIN:
            *values = Source->Gain;
            return AL_NO_ERROR;

        case AL_PITCH:
            *values = Source->Pitch;
            return AL_NO_ERROR;

        case AL_MAX_DISTANCE:
            *values = Source->MaxDistance;
            return AL_NO_ERROR;

        case AL_ROLLOFF_FACTOR:
            *values = Source->RollOffFactor;
            return AL_NO_ERROR;

        case AL_REFERENCE_DISTANCE:
            *values = Source->RefDistance;
            return AL_NO_ERROR;

        case AL_CONE_INNER_ANGLE:
            *values = Source->InnerAngle;
            return AL_NO_ERROR;

        case AL_CONE_OUTER_ANGLE:
            *values = Source->OuterAngle;
            return AL_NO_ERROR;

        case AL_MIN_GAIN:
            *values = Source->MinGain;
            return AL_NO_ERROR;

        case AL_MAX_GAIN:
            *values = Source->MaxGain;
            return AL_NO_ERROR;

        case AL_CONE_OUTER_GAIN:
            *values = Source->OuterGain;
            return AL_NO_ERROR;

        case AL_SEC_OFFSET:
        case AL_SAMPLE_OFFSET:
        case AL_BYTE_OFFSET:
            LockContext(Context);
            updateLen = (ALdouble)Context->Device->UpdateSize /
                        Context->Device->Frequency;
            GetSourceOffsets(Source, prop, offsets, updateLen);
            UnlockContext(Context);
            *values = offsets[0];
            return AL_NO_ERROR;

        case AL_CONE_OUTER_GAINHF:
            *values = Source->OuterGainHF;
            return AL_NO_ERROR;

        case AL_AIR_ABSORPTION_FACTOR:
            *values = Source->AirAbsorptionFactor;
            return AL_NO_ERROR;

        case AL_ROOM_ROLLOFF_FACTOR:
            *values = Source->RoomRolloffFactor;
            return AL_NO_ERROR;

        case AL_DOPPLER_FACTOR:
            *values = Source->DopplerFactor;
            return AL_NO_ERROR;

        case AL_SAMPLE_RW_OFFSETS_SOFT:
        case AL_BYTE_RW_OFFSETS_SOFT:
            LockContext(Context);
            updateLen = (ALdouble)Context->Device->UpdateSize /
                        Context->Device->Frequency;
            GetSourceOffsets(Source, prop, values, updateLen);
            UnlockContext(Context);
            return AL_NO_ERROR;

        case AL_SEC_OFFSET_LATENCY_SOFT:
            LockContext(Context);
            values[0] = GetSourceSecOffset(Source);
            values[1] = (ALdouble)ALCdevice_GetLatency(Context->Device) /
                        1000000000.0;
            UnlockContext(Context);
            return AL_NO_ERROR;

        case AL_POSITION:
            LockContext(Context);
            values[0] = Source->Position[0];
            values[1] = Source->Position[1];
            values[2] = Source->Position[2];
            UnlockContext(Context);
            return AL_NO_ERROR;

        case AL_VELOCITY:
            LockContext(Context);
            values[0] = Source->Velocity[0];
            values[1] = Source->Velocity[1];
            values[2] = Source->Velocity[2];
            UnlockContext(Context);
            return AL_NO_ERROR;

        case AL_DIRECTION:
            LockContext(Context);
            values[0] = Source->Orientation[0];
            values[1] = Source->Orientation[1];
            values[2] = Source->Orientation[2];
            UnlockContext(Context);
            return AL_NO_ERROR;

        case AL_SOURCE_RELATIVE:
        case AL_LOOPING:
        case AL_BUFFER:
        case AL_SOURCE_STATE:
        case AL_BUFFERS_QUEUED:
        case AL_BUFFERS_PROCESSED:
        case AL_SOURCE_TYPE:
        case AL_DIRECT_FILTER_GAINHF_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAIN_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO:
        case AL_DIRECT_CHANNELS_SOFT:
        case AL_DISTANCE_MODEL:
            if((err=GetSourceiv(Source, Context, (int)prop, ivals)) == AL_NO_ERROR)
                *values = (ALdouble)ivals[0];
            return err;
    }

    ERR("Unexpected property: 0x%04x\n", prop);
    RETERR(AL_INVALID_ENUM);
}

static ALenum GetSourceiv(const ALsource *Source, ALCcontext *Context, SrcIntProp prop, ALint *values)
{
    ALbufferlistitem *BufferList;
    ALdouble dvals[3];
    ALenum   err;

    switch(prop)
    {
        case AL_SOURCE_RELATIVE:
            *values = Source->HeadRelative;
            return AL_NO_ERROR;

        case AL_LOOPING:
            *values = Source->Looping;
            return AL_NO_ERROR;

        case AL_BUFFER:
            LockContext(Context);
            BufferList = Source->queue;
            if(Source->SourceType != AL_STATIC)
            {
                ALuint i = Source->BuffersPlayed;
                while(i > 0)
                {
                    BufferList = BufferList->next;
                    i--;
                }
            }
            *values = ((BufferList && BufferList->buffer) ?
                       BufferList->buffer->id : 0);
            UnlockContext(Context);
            return AL_NO_ERROR;

        case AL_SOURCE_STATE:
            *values = Source->state;
            return AL_NO_ERROR;

        case AL_BUFFERS_QUEUED:
            *values = Source->BuffersInQueue;
            return AL_NO_ERROR;

        case AL_BUFFERS_PROCESSED:
            LockContext(Context);
            if(Source->Looping || Source->SourceType != AL_STREAMING)
            {
                /* Buffers on a looping source are in a perpetual state of
                 * PENDING, so don't report any as PROCESSED */
                *values = 0;
            }
            else
                *values = Source->BuffersPlayed;
            UnlockContext(Context);
            return AL_NO_ERROR;

        case AL_SOURCE_TYPE:
            *values = Source->SourceType;
            return AL_NO_ERROR;

        case AL_DIRECT_FILTER_GAINHF_AUTO:
            *values = Source->DryGainHFAuto;
            return AL_NO_ERROR;

        case AL_AUXILIARY_SEND_FILTER_GAIN_AUTO:
            *values = Source->WetGainAuto;
            return AL_NO_ERROR;

        case AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO:
            *values = Source->WetGainHFAuto;
            return AL_NO_ERROR;

        case AL_DIRECT_CHANNELS_SOFT:
            *values = Source->DirectChannels;
            return AL_NO_ERROR;

        case AL_DISTANCE_MODEL:
            *values = Source->DistanceModel;
            return AL_NO_ERROR;

        case AL_MAX_DISTANCE:
        case AL_ROLLOFF_FACTOR:
        case AL_REFERENCE_DISTANCE:
        case AL_CONE_INNER_ANGLE:
        case AL_CONE_OUTER_ANGLE:
        case AL_SEC_OFFSET:
        case AL_SAMPLE_OFFSET:
        case AL_BYTE_OFFSET:
        case AL_DOPPLER_FACTOR:
            if((err=GetSourcedv(Source, Context, (int)prop, dvals)) == AL_NO_ERROR)
                *values = (ALint)dvals[0];
            return err;

        case AL_SAMPLE_RW_OFFSETS_SOFT:
        case AL_BYTE_RW_OFFSETS_SOFT:
            if((err=GetSourcedv(Source, Context, (int)prop, dvals)) == AL_NO_ERROR)
            {
                values[0] = (ALint)dvals[0];
                values[1] = (ALint)dvals[1];
            }
            return err;

        case AL_POSITION:
        case AL_VELOCITY:
        case AL_DIRECTION:
            if((err=GetSourcedv(Source, Context, (int)prop, dvals)) == AL_NO_ERROR)
            {
                values[0] = (ALint)dvals[0];
                values[1] = (ALint)dvals[1];
                values[2] = (ALint)dvals[2];
            }
            return err;

        case siSampleOffsetLatencySOFT:
            /* i64 only */
            break;

        case siDirectFilter:
        case siAuxSendFilter:
            /* ??? */
            break;
    }

    ERR("Unexpected property: 0x%04x\n", prop);
    RETERR(AL_INVALID_ENUM);
}

static ALenum GetSourcei64v(const ALsource *Source, ALCcontext *Context, SrcIntProp prop, ALint64 *values)
{
    ALdouble dvals[3];
    ALint    ivals[3];
    ALenum   err;

    switch(prop)
    {
        case AL_SAMPLE_OFFSET_LATENCY_SOFT:
            LockContext(Context);
            values[0] = GetSourceOffset(Source);
            values[1] = ALCdevice_GetLatency(Context->Device);
            UnlockContext(Context);
            return AL_NO_ERROR;

        case AL_MAX_DISTANCE:
        case AL_ROLLOFF_FACTOR:
        case AL_REFERENCE_DISTANCE:
        case AL_CONE_INNER_ANGLE:
        case AL_CONE_OUTER_ANGLE:
        case AL_SEC_OFFSET:
        case AL_SAMPLE_OFFSET:
        case AL_BYTE_OFFSET:
        case AL_DOPPLER_FACTOR:
            if((err=GetSourcedv(Source, Context, (int)prop, dvals)) == AL_NO_ERROR)
                *values = (ALint64)dvals[0];
            return err;

        case AL_SAMPLE_RW_OFFSETS_SOFT:
        case AL_BYTE_RW_OFFSETS_SOFT:
            if((err=GetSourcedv(Source, Context, (int)prop, dvals)) == AL_NO_ERROR)
            {
                values[0] = (ALint64)dvals[0];
                values[1] = (ALint64)dvals[1];
            }
            return err;

        case AL_POSITION:
        case AL_VELOCITY:
        case AL_DIRECTION:
            if((err=GetSourcedv(Source, Context, (int)prop, dvals)) == AL_NO_ERROR)
            {
                values[0] = (ALint64)dvals[0];
                values[1] = (ALint64)dvals[1];
                values[2] = (ALint64)dvals[2];
            }
            return err;

        case AL_SOURCE_RELATIVE:
        case AL_LOOPING:
        case AL_SOURCE_STATE:
        case AL_BUFFERS_QUEUED:
        case AL_BUFFERS_PROCESSED:
        case AL_SOURCE_TYPE:
        case AL_DIRECT_FILTER_GAINHF_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAIN_AUTO:
        case AL_AUXILIARY_SEND_FILTER_GAINHF_AUTO:
        case AL_DIRECT_CHANNELS_SOFT:
        case AL_DISTANCE_MODEL:
            if((err=GetSourceiv(Source, Context, (int)prop, ivals)) == AL_NO_ERROR)
                *values = ivals[0];
            return err;

        case siBuffer:
        case siDirectFilter:
            if((err=GetSourceiv(Source, Context, (int)prop, ivals)) == AL_NO_ERROR)
                *values = ((ALuint*)ivals)[0];
            return err;

        case siAuxSendFilter:
            if((err=GetSourceiv(Source, Context, (int)prop, ivals)) == AL_NO_ERROR)
            {
                values[0] = ((ALuint*)ivals)[0];
                values[1] = ((ALuint*)ivals)[1];
                values[2] = ((ALuint*)ivals)[2];
            }
            return err;
    }

    ERR("Unexpected property: 0x%04x\n", prop);
    RETERR(AL_INVALID_ENUM);
}

#undef RETERR


AL_API ALvoid AL_APIENTRY alGenSources(ALsizei n, ALuint *sources)
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
            ALsource *source = al_calloc(16, sizeof(ALsource));
            if(!source)
            {
                alDeleteSources(cur, sources);
                al_throwerr(Context, AL_OUT_OF_MEMORY);
            }
            InitSourceParams(source);

            err = NewThunkEntry(&source->id);
            if(err == AL_NO_ERROR)
                err = InsertUIntMapEntry(&Context->SourceMap, source->id, source);
            if(err != AL_NO_ERROR)
            {
                FreeThunkEntry(source->id);
                memset(source, 0, sizeof(ALsource));
                al_free(source);

                alDeleteSources(cur, sources);
                al_throwerr(Context, err);
            }

            sources[cur] = source->id;
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alDeleteSources(ALsizei n, const ALuint *sources)
{
    ALCcontext *Context;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        ALbufferlistitem *BufferList;
        ALsource *Source;
        ALsizei i, j;

        CHECK_VALUE(Context, n >= 0);

        /* Check that all Sources are valid */
        for(i = 0;i < n;i++)
        {
            if(LookupSource(Context, sources[i]) == NULL)
                al_throwerr(Context, AL_INVALID_NAME);
        }

        for(i = 0;i < n;i++)
        {
            ALsource **srclist, **srclistend;

            if((Source=RemoveSource(Context, sources[i])) == NULL)
                continue;
            FreeThunkEntry(Source->id);

            LockContext(Context);
            srclist = Context->ActiveSources;
            srclistend = srclist + Context->ActiveSourceCount;
            while(srclist != srclistend)
            {
                if(*srclist == Source)
                {
                    Context->ActiveSourceCount--;
                    *srclist = *(--srclistend);
                    break;
                }
                srclist++;
            }
            UnlockContext(Context);

            while(Source->queue != NULL)
            {
                BufferList = Source->queue;
                Source->queue = BufferList->next;

                if(BufferList->buffer != NULL)
                    DecrementRef(&BufferList->buffer->ref);
                free(BufferList);
            }

            for(j = 0;j < MAX_SENDS;++j)
            {
                if(Source->Send[j].Slot)
                    DecrementRef(&Source->Send[j].Slot->ref);
                Source->Send[j].Slot = NULL;
            }

            memset(Source, 0, sizeof(*Source));
            al_free(Source);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}


AL_API ALboolean AL_APIENTRY alIsSource(ALuint source)
{
    ALCcontext *Context;
    ALboolean  result;

    Context = GetContextRef();
    if(!Context) return AL_FALSE;

    result = (LookupSource(Context, source) ? AL_TRUE : AL_FALSE);

    ALCcontext_DecRef(Context);

    return result;
}


AL_API ALvoid AL_APIENTRY alSourcef(ALuint source, ALenum param, ALfloat value)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!(FloatValsByProp(param) == 1))
        alSetError(Context, AL_INVALID_ENUM);
    else
        SetSourcefv(Source, Context, param, &value);

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alSource3f(ALuint source, ALenum param, ALfloat value1, ALfloat value2, ALfloat value3)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!(FloatValsByProp(param) == 3))
        alSetError(Context, AL_INVALID_ENUM);
    else
    {
        ALfloat fvals[3] = { value1, value2, value3 };
        SetSourcefv(Source, Context, param, fvals);
    }

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alSourcefv(ALuint source, ALenum param, const ALfloat *values)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!values)
        alSetError(Context, AL_INVALID_VALUE);
    else if(!(FloatValsByProp(param) > 0))
        alSetError(Context, AL_INVALID_ENUM);
    else
        SetSourcefv(Source, Context, param, values);

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alSourcedSOFT(ALuint source, ALenum param, ALdouble value)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!(DoubleValsByProp(param) == 1))
        alSetError(Context, AL_INVALID_ENUM);
    else
    {
        ALfloat fval = (ALfloat)value;
        SetSourcefv(Source, Context, param, &fval);
    }

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alSource3dSOFT(ALuint source, ALenum param, ALdouble value1, ALdouble value2, ALdouble value3)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!(DoubleValsByProp(param) == 3))
        alSetError(Context, AL_INVALID_ENUM);
    else
    {
        ALfloat fvals[3] = { (ALfloat)value1, (ALfloat)value2, (ALfloat)value3 };
        SetSourcefv(Source, Context, param, fvals);
    }

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alSourcedvSOFT(ALuint source, ALenum param, const ALdouble *values)
{
    ALCcontext *Context;
    ALsource   *Source;
    ALint      count;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!values)
        alSetError(Context, AL_INVALID_VALUE);
    else if(!((count=DoubleValsByProp(param)) > 0 && count <= 3))
        alSetError(Context, AL_INVALID_ENUM);
    else
    {
        ALfloat fvals[3];
        ALint i;

        for(i = 0;i < count;i++)
            fvals[i] = (ALfloat)values[i];
        SetSourcefv(Source, Context, param, fvals);
    }

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alSourcei(ALuint source, ALenum param, ALint value)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!(IntValsByProp(param) == 1))
        alSetError(Context, AL_INVALID_ENUM);
    else
        SetSourceiv(Source, Context, param, &value);

    ALCcontext_DecRef(Context);
}

AL_API void AL_APIENTRY alSource3i(ALuint source, ALenum param, ALint value1, ALint value2, ALint value3)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!(IntValsByProp(param) == 3))
        alSetError(Context, AL_INVALID_ENUM);
    else
    {
        ALint ivals[3] = { value1, value2, value3 };
        SetSourceiv(Source, Context, param, ivals);
    }

    ALCcontext_DecRef(Context);
}

AL_API void AL_APIENTRY alSourceiv(ALuint source, ALenum param, const ALint *values)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!values)
        alSetError(Context, AL_INVALID_VALUE);
    else if(!(IntValsByProp(param) > 0))
        alSetError(Context, AL_INVALID_ENUM);
    else
        SetSourceiv(Source, Context, param, values);

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alSourcei64SOFT(ALuint source, ALenum param, ALint64SOFT value)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!(Int64ValsByProp(param) == 1))
        alSetError(Context, AL_INVALID_ENUM);
    else
        SetSourcei64v(Source, Context, param, &value);

    ALCcontext_DecRef(Context);
}

AL_API void AL_APIENTRY alSource3i64SOFT(ALuint source, ALenum param, ALint64SOFT value1, ALint64SOFT value2, ALint64SOFT value3)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!(Int64ValsByProp(param) == 3))
        alSetError(Context, AL_INVALID_ENUM);
    else
    {
        ALint64SOFT i64vals[3] = { value1, value2, value3 };
        SetSourcei64v(Source, Context, param, i64vals);
    }

    ALCcontext_DecRef(Context);
}

AL_API void AL_APIENTRY alSourcei64vSOFT(ALuint source, ALenum param, const ALint64SOFT *values)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!values)
        alSetError(Context, AL_INVALID_VALUE);
    else if(!(Int64ValsByProp(param) > 0))
        alSetError(Context, AL_INVALID_ENUM);
    else
        SetSourcei64v(Source, Context, param, values);

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alGetSourcef(ALuint source, ALenum param, ALfloat *value)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!value)
        alSetError(Context, AL_INVALID_VALUE);
    else if(!(FloatValsByProp(param) == 1))
        alSetError(Context, AL_INVALID_ENUM);
    else
    {
        ALdouble dval;
        if(GetSourcedv(Source, Context, param, &dval) == AL_NO_ERROR)
            *value = (ALfloat)dval;
    }

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alGetSource3f(ALuint source, ALenum param, ALfloat *value1, ALfloat *value2, ALfloat *value3)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!(value1 && value2 && value3))
        alSetError(Context, AL_INVALID_VALUE);
    else if(!(FloatValsByProp(param) == 3))
        alSetError(Context, AL_INVALID_ENUM);
    else
    {
        ALdouble dvals[3];
        if(GetSourcedv(Source, Context, param, dvals) == AL_NO_ERROR)
        {
            *value1 = (ALfloat)dvals[0];
            *value2 = (ALfloat)dvals[1];
            *value3 = (ALfloat)dvals[2];
        }
    }

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alGetSourcefv(ALuint source, ALenum param, ALfloat *values)
{
    ALCcontext *Context;
    ALsource   *Source;
    ALint      count;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!values)
        alSetError(Context, AL_INVALID_VALUE);
    else if(!((count=FloatValsByProp(param)) > 0 && count <= 3))
        alSetError(Context, AL_INVALID_ENUM);
    else
    {
        ALdouble dvals[3];
        if(GetSourcedv(Source, Context, param, dvals) == AL_NO_ERROR)
        {
            ALint i;
            for(i = 0;i < count;i++)
                values[i] = (ALfloat)dvals[i];
        }
    }

    ALCcontext_DecRef(Context);
}


AL_API void AL_APIENTRY alGetSourcedSOFT(ALuint source, ALenum param, ALdouble *value)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!value)
        alSetError(Context, AL_INVALID_VALUE);
    else if(!(DoubleValsByProp(param) == 1))
        alSetError(Context, AL_INVALID_ENUM);
    else
        GetSourcedv(Source, Context, param, value);

    ALCcontext_DecRef(Context);
}

AL_API void AL_APIENTRY alGetSource3dSOFT(ALuint source, ALenum param, ALdouble *value1, ALdouble *value2, ALdouble *value3)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!(value1 && value2 && value3))
        alSetError(Context, AL_INVALID_VALUE);
    else if(!(DoubleValsByProp(param) == 3))
        alSetError(Context, AL_INVALID_ENUM);
    else
    {
        ALdouble dvals[3];
        if(GetSourcedv(Source, Context, param, dvals) == AL_NO_ERROR)
        {
            *value1 = dvals[0];
            *value2 = dvals[1];
            *value3 = dvals[2];
        }
    }

    ALCcontext_DecRef(Context);
}

AL_API void AL_APIENTRY alGetSourcedvSOFT(ALuint source, ALenum param, ALdouble *values)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!values)
        alSetError(Context, AL_INVALID_VALUE);
    else if(!(DoubleValsByProp(param) > 0))
        alSetError(Context, AL_INVALID_ENUM);
    else
        GetSourcedv(Source, Context, param, values);

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alGetSourcei(ALuint source, ALenum param, ALint *value)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!value)
        alSetError(Context, AL_INVALID_VALUE);
    else if(!(IntValsByProp(param) == 1))
        alSetError(Context, AL_INVALID_ENUM);
    else
        GetSourceiv(Source, Context, param, value);

    ALCcontext_DecRef(Context);
}


AL_API void AL_APIENTRY alGetSource3i(ALuint source, ALenum param, ALint *value1, ALint *value2, ALint *value3)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!(value1 && value2 && value3))
        alSetError(Context, AL_INVALID_VALUE);
    else if(!(IntValsByProp(param) == 3))
        alSetError(Context, AL_INVALID_ENUM);
    else
    {
        ALint ivals[3];
        if(GetSourceiv(Source, Context, param, ivals) == AL_NO_ERROR)
        {
            *value1 = ivals[0];
            *value2 = ivals[1];
            *value3 = ivals[2];
        }
    }

    ALCcontext_DecRef(Context);
}


AL_API void AL_APIENTRY alGetSourceiv(ALuint source, ALenum param, ALint *values)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!values)
        alSetError(Context, AL_INVALID_VALUE);
    else if(!(IntValsByProp(param) > 0))
        alSetError(Context, AL_INVALID_ENUM);
    else
        GetSourceiv(Source, Context, param, values);

    ALCcontext_DecRef(Context);
}


AL_API void AL_APIENTRY alGetSourcei64SOFT(ALuint source, ALenum param, ALint64SOFT *value)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!value)
        alSetError(Context, AL_INVALID_VALUE);
    else if(!(Int64ValsByProp(param) == 1))
        alSetError(Context, AL_INVALID_ENUM);
    else
        GetSourcei64v(Source, Context, param, value);

    ALCcontext_DecRef(Context);
}

AL_API void AL_APIENTRY alGetSource3i64SOFT(ALuint source, ALenum param, ALint64SOFT *value1, ALint64SOFT *value2, ALint64SOFT *value3)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!(value1 && value2 && value3))
        alSetError(Context, AL_INVALID_VALUE);
    else if(!(Int64ValsByProp(param) == 3))
        alSetError(Context, AL_INVALID_ENUM);
    else
    {
        ALint64 i64vals[3];
        if(GetSourcei64v(Source, Context, param, i64vals) == AL_NO_ERROR)
        {
            *value1 = i64vals[0];
            *value2 = i64vals[1];
            *value3 = i64vals[2];
        }
    }

    ALCcontext_DecRef(Context);
}

AL_API void AL_APIENTRY alGetSourcei64vSOFT(ALuint source, ALenum param, ALint64SOFT *values)
{
    ALCcontext *Context;
    ALsource   *Source;

    Context = GetContextRef();
    if(!Context) return;

    if((Source=LookupSource(Context, source)) == NULL)
        alSetError(Context, AL_INVALID_NAME);
    else if(!values)
        alSetError(Context, AL_INVALID_VALUE);
    else if(!(Int64ValsByProp(param) > 0))
        alSetError(Context, AL_INVALID_ENUM);
    else
        GetSourcei64v(Source, Context, param, values);

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alSourcePlay(ALuint source)
{
    alSourcePlayv(1, &source);
}
AL_API ALvoid AL_APIENTRY alSourcePlayv(ALsizei n, const ALuint *sources)
{
    ALCcontext *Context;
    ALsource   *Source;
    ALsizei    i;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, n >= 0);
        for(i = 0;i < n;i++)
        {
            if(!LookupSource(Context, sources[i]))
                al_throwerr(Context, AL_INVALID_NAME);
        }

        LockContext(Context);
        while(Context->MaxActiveSources-Context->ActiveSourceCount < n)
        {
            void *temp = NULL;
            ALsizei newcount;

            newcount = Context->MaxActiveSources << 1;
            if(newcount > 0)
                temp = realloc(Context->ActiveSources,
                               sizeof(*Context->ActiveSources) * newcount);
            if(!temp)
            {
                UnlockContext(Context);
                al_throwerr(Context, AL_OUT_OF_MEMORY);
            }

            Context->ActiveSources = temp;
            Context->MaxActiveSources = newcount;
        }

        for(i = 0;i < n;i++)
        {
            Source = LookupSource(Context, sources[i]);
            if(Context->DeferUpdates) Source->new_state = AL_PLAYING;
            else SetSourceState(Source, Context, AL_PLAYING);
        }
        UnlockContext(Context);
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alSourcePause(ALuint source)
{
    alSourcePausev(1, &source);
}
AL_API ALvoid AL_APIENTRY alSourcePausev(ALsizei n, const ALuint *sources)
{
    ALCcontext *Context;
    ALsource   *Source;
    ALsizei    i;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, n >= 0);
        for(i = 0;i < n;i++)
        {
            if(!LookupSource(Context, sources[i]))
                al_throwerr(Context, AL_INVALID_NAME);
        }

        LockContext(Context);
        for(i = 0;i < n;i++)
        {
            Source = LookupSource(Context, sources[i]);
            if(Context->DeferUpdates) Source->new_state = AL_PAUSED;
            else SetSourceState(Source, Context, AL_PAUSED);
        }
        UnlockContext(Context);
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alSourceStop(ALuint source)
{
    alSourceStopv(1, &source);
}
AL_API ALvoid AL_APIENTRY alSourceStopv(ALsizei n, const ALuint *sources)
{
    ALCcontext *Context;
    ALsource   *Source;
    ALsizei    i;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, n >= 0);
        for(i = 0;i < n;i++)
        {
            if(!LookupSource(Context, sources[i]))
                al_throwerr(Context, AL_INVALID_NAME);
        }

        LockContext(Context);
        for(i = 0;i < n;i++)
        {
            Source = LookupSource(Context, sources[i]);
            Source->new_state = AL_NONE;
            SetSourceState(Source, Context, AL_STOPPED);
        }
        UnlockContext(Context);
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alSourceRewind(ALuint source)
{
    alSourceRewindv(1, &source);
}
AL_API ALvoid AL_APIENTRY alSourceRewindv(ALsizei n, const ALuint *sources)
{
    ALCcontext *Context;
    ALsource   *Source;
    ALsizei    i;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, n >= 0);
        for(i = 0;i < n;i++)
        {
            if(!LookupSource(Context, sources[i]))
                al_throwerr(Context, AL_INVALID_NAME);
        }

        LockContext(Context);
        for(i = 0;i < n;i++)
        {
            Source = LookupSource(Context, sources[i]);
            Source->new_state = AL_NONE;
            SetSourceState(Source, Context, AL_INITIAL);
        }
        UnlockContext(Context);
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alSourceQueueBuffers(ALuint source, ALsizei nb, const ALuint *buffers)
{
    ALCcontext *Context;
    ALsource   *Source;
    ALsizei    i;
    ALbufferlistitem *BufferListStart = NULL;
    ALbufferlistitem *BufferList;
    ALbuffer *BufferFmt;

    if(nb == 0)
        return;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        ALCdevice *device = Context->Device;

        CHECK_VALUE(Context, nb >= 0);

        if((Source=LookupSource(Context, source)) == NULL)
            al_throwerr(Context, AL_INVALID_NAME);

        LockContext(Context);
        if(Source->SourceType == AL_STATIC)
        {
            UnlockContext(Context);
            /* Can't queue on a Static Source */
            al_throwerr(Context, AL_INVALID_OPERATION);
        }

        BufferFmt = NULL;

        /* Check for a valid Buffer, for its frequency and format */
        BufferList = Source->queue;
        while(BufferList)
        {
            if(BufferList->buffer)
            {
                BufferFmt = BufferList->buffer;
                break;
            }
            BufferList = BufferList->next;
        }

        for(i = 0;i < nb;i++)
        {
            ALbuffer *buffer = NULL;
            if(buffers[i] && (buffer=LookupBuffer(device, buffers[i])) == NULL)
            {
                UnlockContext(Context);
                al_throwerr(Context, AL_INVALID_NAME);
            }

            if(!BufferListStart)
            {
                BufferListStart = malloc(sizeof(ALbufferlistitem));
                BufferListStart->buffer = buffer;
                BufferListStart->next = NULL;
                BufferListStart->prev = NULL;
                BufferList = BufferListStart;
            }
            else
            {
                BufferList->next = malloc(sizeof(ALbufferlistitem));
                BufferList->next->buffer = buffer;
                BufferList->next->next = NULL;
                BufferList->next->prev = BufferList;
                BufferList = BufferList->next;
            }
            if(!buffer) continue;
            IncrementRef(&buffer->ref);

            ReadLock(&buffer->lock);
            if(BufferFmt == NULL)
            {
                BufferFmt = buffer;

                Source->NumChannels = ChannelsFromFmt(buffer->FmtChannels);
                Source->SampleSize  = BytesFromFmt(buffer->FmtType);
                if(buffer->FmtChannels == FmtMono)
                    Source->Update = CalcSourceParams;
                else
                    Source->Update = CalcNonAttnSourceParams;

                Source->NeedsUpdate = AL_TRUE;
            }
            else if(BufferFmt->Frequency != buffer->Frequency ||
                    BufferFmt->OriginalChannels != buffer->OriginalChannels ||
                    BufferFmt->OriginalType != buffer->OriginalType)
            {
                ReadUnlock(&buffer->lock);
                UnlockContext(Context);
                al_throwerr(Context, AL_INVALID_OPERATION);
            }
            ReadUnlock(&buffer->lock);
        }

        /* Source is now streaming */
        Source->SourceType = AL_STREAMING;

        if(Source->queue == NULL)
            Source->queue = BufferListStart;
        else
        {
            /* Append to the end of the queue */
            BufferList = Source->queue;
            while(BufferList->next != NULL)
                BufferList = BufferList->next;

            BufferListStart->prev = BufferList;
            BufferList->next = BufferListStart;
        }
        BufferListStart = NULL;

        Source->BuffersInQueue += nb;

        UnlockContext(Context);
    }
    al_endtry;

    while(BufferListStart)
    {
        BufferList = BufferListStart;
        BufferListStart = BufferList->next;

        if(BufferList->buffer)
            DecrementRef(&BufferList->buffer->ref);
        free(BufferList);
    }

    ALCcontext_DecRef(Context);
}

AL_API ALvoid AL_APIENTRY alSourceUnqueueBuffers(ALuint source, ALsizei nb, ALuint *buffers)
{
    ALCcontext *Context;
    ALsource   *Source;
    ALsizei    i;
    ALbufferlistitem *BufferList;

    if(nb == 0)
        return;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, nb >= 0);

        if((Source=LookupSource(Context, source)) == NULL)
            al_throwerr(Context, AL_INVALID_NAME);

        LockContext(Context);
        if(Source->Looping || Source->SourceType != AL_STREAMING ||
           (ALuint)nb > Source->BuffersPlayed)
        {
            UnlockContext(Context);
            /* Trying to unqueue pending buffers, or a buffer that wasn't queued. */
            al_throwerr(Context, AL_INVALID_VALUE);
        }

        for(i = 0;i < nb;i++)
        {
            BufferList = Source->queue;
            Source->queue = BufferList->next;
            Source->BuffersInQueue--;
            Source->BuffersPlayed--;

            if(BufferList->buffer)
            {
                buffers[i] = BufferList->buffer->id;
                DecrementRef(&BufferList->buffer->ref);
            }
            else
                buffers[i] = 0;

            free(BufferList);
        }
        if(Source->queue)
            Source->queue->prev = NULL;
        UnlockContext(Context);
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}


static ALvoid InitSourceParams(ALsource *Source)
{
    ALuint i;

    Source->InnerAngle = 360.0f;
    Source->OuterAngle = 360.0f;
    Source->Pitch = 1.0f;
    Source->Position[0] = 0.0f;
    Source->Position[1] = 0.0f;
    Source->Position[2] = 0.0f;
    Source->Orientation[0] = 0.0f;
    Source->Orientation[1] = 0.0f;
    Source->Orientation[2] = 0.0f;
    Source->Velocity[0] = 0.0f;
    Source->Velocity[1] = 0.0f;
    Source->Velocity[2] = 0.0f;
    Source->RefDistance = 1.0f;
    Source->MaxDistance = FLT_MAX;
    Source->RollOffFactor = 1.0f;
    Source->Looping = AL_FALSE;
    Source->Gain = 1.0f;
    Source->MinGain = 0.0f;
    Source->MaxGain = 1.0f;
    Source->OuterGain = 0.0f;
    Source->OuterGainHF = 1.0f;

    Source->DryGainHFAuto = AL_TRUE;
    Source->WetGainAuto = AL_TRUE;
    Source->WetGainHFAuto = AL_TRUE;
    Source->AirAbsorptionFactor = 0.0f;
    Source->RoomRolloffFactor = 0.0f;
    Source->DopplerFactor = 1.0f;
    Source->DirectChannels = AL_FALSE;

    Source->DistanceModel = DefaultDistanceModel;

    Source->Resampler = DefaultResampler;

    Source->state = AL_INITIAL;
    Source->new_state = AL_NONE;
    Source->SourceType = AL_UNDETERMINED;
    Source->Offset = -1.0;

    Source->DirectGain = 1.0f;
    Source->DirectGainHF = 1.0f;
    for(i = 0;i < MAX_SENDS;i++)
    {
        Source->Send[i].Gain = 1.0f;
        Source->Send[i].GainHF = 1.0f;
    }

    Source->NeedsUpdate = AL_TRUE;

    Source->Hrtf.Moving = AL_FALSE;
    Source->Hrtf.Counter = 0;
}


/* SetSourceState
 *
 * Sets the source's new play state given its current state.
 */
ALvoid SetSourceState(ALsource *Source, ALCcontext *Context, ALenum state)
{
    if(state == AL_PLAYING)
    {
        ALbufferlistitem *BufferList;
        ALsizei j, k;

        /* Check that there is a queue containing at least one valid, non zero
         * length Buffer. */
        BufferList = Source->queue;
        while(BufferList)
        {
            if(BufferList->buffer != NULL && BufferList->buffer->SampleLen)
                break;
            BufferList = BufferList->next;
        }

        if(Source->state != AL_PLAYING)
        {
            for(j = 0;j < MaxChannels;j++)
            {
                for(k = 0;k < SRC_HISTORY_LENGTH;k++)
                    Source->Hrtf.History[j][k] = 0.0f;
                for(k = 0;k < HRIR_LENGTH;k++)
                {
                    Source->Hrtf.Values[j][k][0] = 0.0f;
                    Source->Hrtf.Values[j][k][1] = 0.0f;
                }
            }
        }

        if(Source->state != AL_PAUSED)
        {
            Source->state = AL_PLAYING;
            Source->position = 0;
            Source->position_fraction = 0;
            Source->BuffersPlayed = 0;
        }
        else
            Source->state = AL_PLAYING;

        // Check if an Offset has been set
        if(Source->Offset >= 0.0)
            ApplyOffset(Source);

        /* If there's nothing to play, or device is disconnected, go right to
         * stopped */
        if(!BufferList || !Context->Device->Connected)
        {
            SetSourceState(Source, Context, AL_STOPPED);
            return;
        }

        for(j = 0;j < Context->ActiveSourceCount;j++)
        {
            if(Context->ActiveSources[j] == Source)
                break;
        }
        if(j == Context->ActiveSourceCount)
            Context->ActiveSources[Context->ActiveSourceCount++] = Source;
    }
    else if(state == AL_PAUSED)
    {
        if(Source->state == AL_PLAYING)
        {
            Source->state = AL_PAUSED;
            Source->Hrtf.Moving = AL_FALSE;
            Source->Hrtf.Counter = 0;
        }
    }
    else if(state == AL_STOPPED)
    {
        if(Source->state != AL_INITIAL)
        {
            Source->state = AL_STOPPED;
            Source->BuffersPlayed = Source->BuffersInQueue;
            Source->Hrtf.Moving = AL_FALSE;
            Source->Hrtf.Counter = 0;
        }
        Source->Offset = -1.0;
    }
    else if(state == AL_INITIAL)
    {
        if(Source->state != AL_INITIAL)
        {
            Source->state = AL_INITIAL;
            Source->position = 0;
            Source->position_fraction = 0;
            Source->BuffersPlayed = 0;
            Source->Hrtf.Moving = AL_FALSE;
            Source->Hrtf.Counter = 0;
        }
        Source->Offset = -1.0;
    }
}

/* GetSourceOffset
 *
 * Gets the current read offset for the given Source, in 32.32 fixed-point
 * samples. The offset is relative to the start of the queue (not the start of
 * the current buffer).
 */
static ALint64 GetSourceOffset(const ALsource *Source)
{
    const ALbufferlistitem *BufferList;
    ALuint64 readPos;
    ALuint i;

    if(Source->state != AL_PLAYING && Source->state != AL_PAUSED)
        return 0;

    /* NOTE: This is the offset into the *current* buffer, so add the length of
     * any played buffers */
    readPos  = (ALuint64)Source->position << 32;
    readPos |= (ALuint64)Source->position_fraction << (32-FRACTIONBITS);
    BufferList = Source->queue;
    for(i = 0;i < Source->BuffersPlayed && BufferList;i++)
    {
        if(BufferList->buffer)
            readPos += (ALuint64)BufferList->buffer->SampleLen << 32;
        BufferList = BufferList->next;
    }

    return (ALint64)minu64(readPos, MAKEU64(0x7fffffff,0xffffffff));
}

/* GetSourceSecOffset
 *
 * Gets the current read offset for the given Source, in seconds. The offset is
 * relative to the start of the queue (not the start of the current buffer).
 */
static ALdouble GetSourceSecOffset(const ALsource *Source)
{
    const ALbufferlistitem *BufferList;
    const ALbuffer *Buffer = NULL;
    ALuint64 readPos;
    ALuint i;

    BufferList = Source->queue;
    while(BufferList)
    {
        if(BufferList->buffer)
        {
            Buffer = BufferList->buffer;
            break;
        }
        BufferList = BufferList->next;
    }

    if((Source->state != AL_PLAYING && Source->state != AL_PAUSED) || !Buffer)
        return 0.0;

    /* NOTE: This is the offset into the *current* buffer, so add the length of
     * any played buffers */
    readPos  = (ALuint64)Source->position << FRACTIONBITS;
    readPos |= (ALuint64)Source->position_fraction;
    BufferList = Source->queue;
    for(i = 0;i < Source->BuffersPlayed && BufferList;i++)
    {
        if(BufferList->buffer)
            readPos += (ALuint64)BufferList->buffer->SampleLen << FRACTIONBITS;
        BufferList = BufferList->next;
    }

    return (ALdouble)readPos / (ALdouble)FRACTIONONE / (ALdouble)Buffer->Frequency;
}

/* GetSourceOffsets
 *
 * Gets the current read and write offsets for the given Source, in the
 * appropriate format (Bytes, Samples or Seconds). The offsets are relative to
 * the start of the queue (not the start of the current buffer).
 */
static ALvoid GetSourceOffsets(const ALsource *Source, ALenum name, ALdouble *offset, ALdouble updateLen)
{
    const ALbufferlistitem *BufferList;
    const ALbuffer         *Buffer = NULL;
    ALuint readPos, writePos;
    ALuint totalBufferLen;
    ALuint i;

    // Find the first valid Buffer in the Queue
    BufferList = Source->queue;
    while(BufferList)
    {
        if(BufferList->buffer)
        {
            Buffer = BufferList->buffer;
            break;
        }
        BufferList = BufferList->next;
    }

    if((Source->state != AL_PLAYING && Source->state != AL_PAUSED) || !Buffer)
    {
        offset[0] = 0.0;
        offset[1] = 0.0;
        return;
    }

    if(updateLen > 0.0 && updateLen < 0.015)
        updateLen = 0.015;

    /* NOTE: This is the offset into the *current* buffer, so add the length of
     * any played buffers */
    readPos = Source->position;
    totalBufferLen = 0;
    BufferList = Source->queue;
    for(i = 0;BufferList;i++)
    {
        if(BufferList->buffer)
        {
            if(i < Source->BuffersPlayed)
                readPos += BufferList->buffer->SampleLen;
            totalBufferLen += BufferList->buffer->SampleLen;
        }
        BufferList = BufferList->next;
    }
    if(Source->state == AL_PLAYING)
        writePos = readPos + (ALuint)(updateLen*Buffer->Frequency);
    else
        writePos = readPos;

    if(Source->Looping)
    {
        readPos %= totalBufferLen;
        writePos %= totalBufferLen;
    }
    else
    {
        /* Wrap positions back to 0 */
        if(readPos >= totalBufferLen)
            readPos = 0;
        if(writePos >= totalBufferLen)
            writePos = 0;
    }

    switch(name)
    {
        case AL_SEC_OFFSET:
            offset[0] = (ALdouble)readPos / Buffer->Frequency;
            offset[1] = (ALdouble)writePos / Buffer->Frequency;
            break;

        case AL_SAMPLE_OFFSET:
        case AL_SAMPLE_RW_OFFSETS_SOFT:
            offset[0] = (ALdouble)readPos;
            offset[1] = (ALdouble)writePos;
            break;

        case AL_BYTE_OFFSET:
        case AL_BYTE_RW_OFFSETS_SOFT:
            if(Buffer->OriginalType == UserFmtIMA4)
            {
                ALuint BlockSize = 36 * ChannelsFromFmt(Buffer->FmtChannels);
                ALuint FrameBlockSize = 65;

                /* Round down to nearest ADPCM block */
                offset[0] = (ALdouble)(readPos / FrameBlockSize * BlockSize);
                if(Source->state != AL_PLAYING)
                    offset[1] = offset[0];
                else
                {
                    /* Round up to nearest ADPCM block */
                    offset[1] = (ALdouble)((writePos+FrameBlockSize-1) /
                                           FrameBlockSize * BlockSize);
                }
            }
            else
            {
                ALuint FrameSize = FrameSizeFromUserFmt(Buffer->OriginalChannels, Buffer->OriginalType);
                offset[0] = (ALdouble)(readPos * FrameSize);
                offset[1] = (ALdouble)(writePos * FrameSize);
            }
            break;
    }
}


/* ApplyOffset
 *
 * Apply the stored playback offset to the Source. This function will update
 * the number of buffers "played" given the stored offset.
 */
ALboolean ApplyOffset(ALsource *Source)
{
    const ALbufferlistitem *BufferList;
    const ALbuffer         *Buffer;
    ALint bufferLen, totalBufferLen;
    ALint buffersPlayed;
    ALint offset;

    /* Get sample frame offset */
    offset = GetSampleOffset(Source);
    if(offset == -1)
        return AL_FALSE;

    buffersPlayed = 0;
    totalBufferLen = 0;

    BufferList = Source->queue;
    while(BufferList)
    {
        Buffer = BufferList->buffer;
        bufferLen = Buffer ? Buffer->SampleLen : 0;

        if(bufferLen <= offset-totalBufferLen)
        {
            /* Offset is past this buffer so increment to the next buffer */
            buffersPlayed++;
        }
        else if(totalBufferLen <= offset)
        {
            /* Offset is in this buffer */
            Source->BuffersPlayed = buffersPlayed;

            Source->position = offset - totalBufferLen;
            Source->position_fraction = 0;
            return AL_TRUE;
        }

        totalBufferLen += bufferLen;

        BufferList = BufferList->next;
    }

    /* Offset is out of range of the queue */
    return AL_FALSE;
}


/* GetSampleOffset
 *
 * Returns the sample offset into the Source's queue (from the Sample, Byte or
 * Second offset supplied by the application). This takes into account the fact
 * that the buffer format may have been modifed since.
 */
static ALint GetSampleOffset(ALsource *Source)
{
    const ALbuffer *Buffer = NULL;
    const ALbufferlistitem *BufferList;
    ALint Offset = -1;

    /* Find the first valid Buffer in the Queue */
    BufferList = Source->queue;
    while(BufferList)
    {
        if(BufferList->buffer)
        {
            Buffer = BufferList->buffer;
            break;
        }
        BufferList = BufferList->next;
    }

    if(!Buffer)
    {
        Source->Offset = -1.0;
        return -1;
    }

    switch(Source->OffsetType)
    {
    case AL_BYTE_OFFSET:
        /* Determine the ByteOffset (and ensure it is block aligned) */
        Offset = (ALint)Source->Offset;
        if(Buffer->OriginalType == UserFmtIMA4)
        {
            Offset /= 36 * ChannelsFromUserFmt(Buffer->OriginalChannels);
            Offset *= 65;
        }
        else
            Offset /= FrameSizeFromUserFmt(Buffer->OriginalChannels, Buffer->OriginalType);
        break;

    case AL_SAMPLE_OFFSET:
        Offset = (ALint)Source->Offset;
        break;

    case AL_SEC_OFFSET:
        Offset = (ALint)(Source->Offset * Buffer->Frequency);
        break;
    }
    Source->Offset = -1.0;

    return Offset;
}


/* ReleaseALSources
 *
 * Destroys all sources in the source map.
 */
ALvoid ReleaseALSources(ALCcontext *Context)
{
    ALsizei pos;
    ALuint j;
    for(pos = 0;pos < Context->SourceMap.size;pos++)
    {
        ALsource *temp = Context->SourceMap.array[pos].value;
        Context->SourceMap.array[pos].value = NULL;

        while(temp->queue != NULL)
        {
            ALbufferlistitem *BufferList = temp->queue;
            temp->queue = BufferList->next;

            if(BufferList->buffer != NULL)
                DecrementRef(&BufferList->buffer->ref);
            free(BufferList);
        }

        for(j = 0;j < MAX_SENDS;++j)
        {
            if(temp->Send[j].Slot)
                DecrementRef(&temp->Send[j].Slot->ref);
            temp->Send[j].Slot = NULL;
        }

        FreeThunkEntry(temp->id);
        memset(temp, 0, sizeof(*temp));
        al_free(temp);
    }
}
