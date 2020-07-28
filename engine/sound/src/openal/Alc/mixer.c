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

#include <math.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <assert.h>

#include "alMain.h"
#include "AL/al.h"
#include "AL/alc.h"
#include "alSource.h"
#include "alBuffer.h"
#include "alListener.h"
#include "alAuxEffectSlot.h"
#include "alu.h"
#include "bs2b.h"


static inline ALfloat Sample_ALbyte(ALbyte val)
{ return val * (1.0f/127.0f); }

static inline ALfloat Sample_ALshort(ALshort val)
{ return val * (1.0f/32767.0f); }

static inline ALfloat Sample_ALfloat(ALfloat val)
{ return val; }

#define DECL_TEMPLATE(T)                                                      \
static void Load_##T(ALfloat *dst, const T *src, ALuint srcstep, ALuint samples)\
{                                                                             \
    ALuint i;                                                                 \
    for(i = 0;i < samples;i++)                                                \
        dst[i] = Sample_##T(src[i*srcstep]);                                  \
}

DECL_TEMPLATE(ALbyte)
DECL_TEMPLATE(ALshort)
DECL_TEMPLATE(ALfloat)

#undef DECL_TEMPLATE

static void LoadData(ALfloat *dst, const ALvoid *src, ALuint srcstep, enum FmtType srctype, ALuint samples)
{
    switch(srctype)
    {
        case FmtByte:
            Load_ALbyte(dst, src, srcstep, samples);
            break;
        case FmtShort:
            Load_ALshort(dst, src, srcstep, samples);
            break;
        case FmtFloat:
            Load_ALfloat(dst, src, srcstep, samples);
            break;
    }
}

static void SilenceData(ALfloat *dst, ALuint samples)
{
    ALuint i;
    for(i = 0;i < samples;i++)
        dst[i] = 0.0f;
}


static void DoFilter(ALfilterState *filter, ALfloat *restrict dst, const ALfloat *restrict src,
                     ALuint numsamples)
{
    ALuint i;
    for(i = 0;i < numsamples;i++)
        dst[i] = ALfilterState_processSingle(filter, src[i]);
    dst[i] = ALfilterState_processSingleC(filter, src[i]);
}


ALvoid MixSource(ALsource *Source, ALCdevice *Device, ALuint SamplesToDo)
{
    ALbufferlistitem *BufferListItem;
    ALuint DataPosInt, DataPosFrac;
    ALuint BuffersPlayed;
    ALboolean Looping;
    ALuint increment;
    enum Resampler Resampler;
    ALenum State;
    ALuint OutPos;
    ALuint NumChannels;
    ALuint SampleSize;
    ALint64 DataSize64;
    ALuint chan, j;

    /* Get source info */
    State         = Source->state;
    BuffersPlayed = Source->BuffersPlayed;
    DataPosInt    = Source->position;
    DataPosFrac   = Source->position_fraction;
    Looping       = Source->Looping;
    increment     = Source->Params.Step;
    Resampler     = (increment==FRACTIONONE) ? PointResampler : Source->Resampler;
    NumChannels   = Source->NumChannels;
    SampleSize    = Source->SampleSize;

    /* Get current buffer queue item */
    BufferListItem = Source->queue;
    for(j = 0;j < BuffersPlayed;j++)
        BufferListItem = BufferListItem->next;

    OutPos = 0;
    do {
        const ALuint BufferPrePadding = ResamplerPrePadding[Resampler];
        const ALuint BufferPadding = ResamplerPadding[Resampler];
        ALuint SrcBufferSize, DstBufferSize;

        /* Figure out how many buffer bytes will be needed */
        DataSize64  = SamplesToDo-OutPos+1;
        DataSize64 *= increment;
        DataSize64 += DataPosFrac+FRACTIONMASK;
        DataSize64 >>= FRACTIONBITS;
        DataSize64 += BufferPadding+BufferPrePadding;

        SrcBufferSize = (ALuint)mini64(DataSize64, BUFFERSIZE);

        /* Figure out how many samples we can actually mix from this. */
        DataSize64  = SrcBufferSize;
        DataSize64 -= BufferPadding+BufferPrePadding;
        DataSize64 <<= FRACTIONBITS;
        DataSize64 -= increment;
        DataSize64 -= DataPosFrac;

        DstBufferSize = (ALuint)((DataSize64+(increment-1)) / increment);
        DstBufferSize = minu(DstBufferSize, (SamplesToDo-OutPos));

        /* Some mixers like having a multiple of 4, so try to give that unless
         * this is the last update. */
        if(OutPos+DstBufferSize < SamplesToDo)
            DstBufferSize &= ~3;

        for(chan = 0;chan < NumChannels;chan++)
        {
            ALfloat *SrcData = Device->SampleData1;
            ALfloat *ResampledData = Device->SampleData2;
            ALuint SrcDataSize = 0;

            if(Source->SourceType == AL_STATIC)
            {
                const ALbuffer *ALBuffer = Source->queue->buffer;
                const ALubyte *Data = ALBuffer->data;
                ALuint DataSize;
                ALuint pos;

                /* If current pos is beyond the loop range, do not loop */
                if(Looping == AL_FALSE || DataPosInt >= (ALuint)ALBuffer->LoopEnd)
                {
                    Looping = AL_FALSE;

                    if(DataPosInt >= BufferPrePadding)
                        pos = DataPosInt - BufferPrePadding;
                    else
                    {
                        DataSize = BufferPrePadding - DataPosInt;
                        DataSize = minu(SrcBufferSize - SrcDataSize, DataSize);

                        SilenceData(&SrcData[SrcDataSize], DataSize);
                        SrcDataSize += DataSize;

                        pos = 0;
                    }

                    /* Copy what's left to play in the source buffer, and clear the
                     * rest of the temp buffer */
                    DataSize = minu(SrcBufferSize - SrcDataSize, ALBuffer->SampleLen - pos);

                    LoadData(&SrcData[SrcDataSize], &Data[(pos*NumChannels + chan)*SampleSize],
                             NumChannels, ALBuffer->FmtType, DataSize);
                    SrcDataSize += DataSize;

                    SilenceData(&SrcData[SrcDataSize], SrcBufferSize - SrcDataSize);
                    SrcDataSize += SrcBufferSize - SrcDataSize;
                }
                else
                {
                    ALuint LoopStart = ALBuffer->LoopStart;
                    ALuint LoopEnd   = ALBuffer->LoopEnd;

                    if(DataPosInt >= LoopStart)
                    {
                        pos = DataPosInt-LoopStart;
                        while(pos < BufferPrePadding)
                            pos += LoopEnd-LoopStart;
                        pos -= BufferPrePadding;
                        pos += LoopStart;
                    }
                    else if(DataPosInt >= BufferPrePadding)
                        pos = DataPosInt - BufferPrePadding;
                    else
                    {
                        DataSize = BufferPrePadding - DataPosInt;
                        DataSize = minu(SrcBufferSize - SrcDataSize, DataSize);

                        SilenceData(&SrcData[SrcDataSize], DataSize);
                        SrcDataSize += DataSize;

                        pos = 0;
                    }

                    /* Copy what's left of this loop iteration, then copy repeats
                     * of the loop section */
                    DataSize = LoopEnd - pos;
                    DataSize = minu(SrcBufferSize - SrcDataSize, DataSize);

                    LoadData(&SrcData[SrcDataSize], &Data[(pos*NumChannels + chan)*SampleSize],
                             NumChannels, ALBuffer->FmtType, DataSize);
                    SrcDataSize += DataSize;

                    DataSize = LoopEnd-LoopStart;
                    while(SrcBufferSize > SrcDataSize)
                    {
                        DataSize = minu(SrcBufferSize - SrcDataSize, DataSize);

                        LoadData(&SrcData[SrcDataSize], &Data[(LoopStart*NumChannels + chan)*SampleSize],
                                 NumChannels, ALBuffer->FmtType, DataSize);
                        SrcDataSize += DataSize;
                    }
                }
            }
            else
            {
                /* Crawl the buffer queue to fill in the temp buffer */
                ALbufferlistitem *tmpiter = BufferListItem;
                ALuint pos;

                if(DataPosInt >= BufferPrePadding)
                    pos = DataPosInt - BufferPrePadding;
                else
                {
                    pos = BufferPrePadding - DataPosInt;
                    while(pos > 0)
                    {
                        if(!tmpiter->prev && !Looping)
                        {
                            ALuint DataSize = minu(SrcBufferSize - SrcDataSize, pos);

                            SilenceData(&SrcData[SrcDataSize], DataSize);
                            SrcDataSize += DataSize;

                            pos = 0;
                            break;
                        }

                        if(tmpiter->prev)
                            tmpiter = tmpiter->prev;
                        else
                        {
                            while(tmpiter->next)
                                tmpiter = tmpiter->next;
                        }

                        if(tmpiter->buffer)
                        {
                            if((ALuint)tmpiter->buffer->SampleLen > pos)
                            {
                                pos = tmpiter->buffer->SampleLen - pos;
                                break;
                            }
                            pos -= tmpiter->buffer->SampleLen;
                        }
                    }
                }

                while(tmpiter && SrcBufferSize > SrcDataSize)
                {
                    const ALbuffer *ALBuffer;
                    if((ALBuffer=tmpiter->buffer) != NULL)
                    {
                        const ALubyte *Data = ALBuffer->data;
                        ALuint DataSize = ALBuffer->SampleLen;

                        /* Skip the data already played */
                        if(DataSize <= pos)
                            pos -= DataSize;
                        else
                        {
                            Data += (pos*NumChannels + chan)*SampleSize;
                            DataSize -= pos;
                            pos -= pos;

                            DataSize = minu(SrcBufferSize - SrcDataSize, DataSize);
                            LoadData(&SrcData[SrcDataSize], Data, NumChannels,
                                     ALBuffer->FmtType, DataSize);
                            SrcDataSize += DataSize;
                        }
                    }
                    tmpiter = tmpiter->next;
                    if(!tmpiter && Looping)
                        tmpiter = Source->queue;
                    else if(!tmpiter)
                    {
                        SilenceData(&SrcData[SrcDataSize], SrcBufferSize - SrcDataSize);
                        SrcDataSize += SrcBufferSize - SrcDataSize;
                    }
                }
            }

            /* Now resample, then filter and mix to the appropriate outputs. */
            Source->Params.Resample(&SrcData[BufferPrePadding], DataPosFrac,
                                    increment, ResampledData, DstBufferSize);

            {
                DirectParams *directparms = &Source->Params.Direct;

                DoFilter(&directparms->Filter[chan], SrcData, ResampledData,
                         DstBufferSize);
                Source->Params.DryMix(directparms, SrcData, chan, OutPos,
                                      SamplesToDo, DstBufferSize);
            }

            for(j = 0;j < Device->NumAuxSends;j++)
            {
                SendParams *sendparms = &Source->Params.Send[j];
                if(!sendparms->Slot)
                    continue;

                DoFilter(&sendparms->Filter[chan], SrcData, ResampledData,
                         DstBufferSize);
                Source->Params.WetMix(sendparms, SrcData, OutPos,
                                      SamplesToDo, DstBufferSize);
            }
        }
        /* Update positions */
        for(j = 0;j < DstBufferSize;j++)
        {
            DataPosFrac += increment;
            DataPosInt  += DataPosFrac>>FRACTIONBITS;
            DataPosFrac &= FRACTIONMASK;
        }
        OutPos += DstBufferSize;

        /* Handle looping sources */
        while(1)
        {
            const ALbuffer *ALBuffer;
            ALuint DataSize = 0;
            ALuint LoopStart = 0;
            ALuint LoopEnd = 0;

            if((ALBuffer=BufferListItem->buffer) != NULL)
            {
                DataSize = ALBuffer->SampleLen;
                LoopStart = ALBuffer->LoopStart;
                LoopEnd = ALBuffer->LoopEnd;
                if(LoopEnd > DataPosInt)
                    break;
            }

            if(Looping && Source->SourceType == AL_STATIC)
            {
                DataPosInt = ((DataPosInt-LoopStart)%(LoopEnd-LoopStart)) + LoopStart;
                break;
            }

            if(DataSize > DataPosInt)
                break;

            if(BufferListItem->next)
            {
                BufferListItem = BufferListItem->next;
                BuffersPlayed++;
            }
            else if(Looping)
            {
                BufferListItem = Source->queue;
                BuffersPlayed = 0;
            }
            else
            {
                State = AL_STOPPED;
                BufferListItem = Source->queue;
                BuffersPlayed = Source->BuffersInQueue;
                DataPosInt = 0;
                DataPosFrac = 0;
                break;
            }

            DataPosInt -= DataSize;
        }
    } while(State == AL_PLAYING && OutPos < SamplesToDo);

    /* Update source info */
    Source->state             = State;
    Source->BuffersPlayed     = BuffersPlayed;
    Source->position          = DataPosInt;
    Source->position_fraction = DataPosFrac;
    Source->Hrtf.Offset      += OutPos;
    if(State == AL_PLAYING)
        Source->Hrtf.Counter = maxu(Source->Hrtf.Counter, OutPos) - OutPos;
    else
    {
        Source->Hrtf.Counter = 0;
        Source->Hrtf.Moving  = AL_FALSE;
    }
}
