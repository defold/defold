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

#include <string.h>
#include <stdlib.h>

#include "alMain.h"


struct RingBuffer {
    ALubyte *mem;

    ALsizei frame_size;
    ALsizei length;
    ALint read_pos;
    ALint write_pos;

    CRITICAL_SECTION cs;
};


RingBuffer *CreateRingBuffer(ALsizei frame_size, ALsizei length)
{
    RingBuffer *ring = calloc(1, sizeof(*ring) + ((length+1) * frame_size));
    if(ring)
    {
        ring->mem = (ALubyte*)(ring+1);

        ring->frame_size = frame_size;
        ring->length = length+1;
        ring->read_pos = 0;
        ring->write_pos = 0;

        InitializeCriticalSection(&ring->cs);
    }
    return ring;
}

void DestroyRingBuffer(RingBuffer *ring)
{
    if(ring)
    {
        DeleteCriticalSection(&ring->cs);
        free(ring);
    }
}

ALsizei RingBufferSize(RingBuffer *ring)
{
    ALsizei s;

    EnterCriticalSection(&ring->cs);
    s = (ring->write_pos-ring->read_pos+ring->length) % ring->length;
    LeaveCriticalSection(&ring->cs);

    return s;
}

void WriteRingBuffer(RingBuffer *ring, const ALubyte *data, ALsizei len)
{
    int remain;

    EnterCriticalSection(&ring->cs);

    remain = (ring->read_pos-ring->write_pos-1+ring->length) % ring->length;
    if(remain < len) len = remain;

    if(len > 0)
    {
        remain = ring->length - ring->write_pos;
        if(remain < len)
        {
            memcpy(ring->mem+(ring->write_pos*ring->frame_size), data,
                   remain*ring->frame_size);
            memcpy(ring->mem, data+(remain*ring->frame_size),
                   (len-remain)*ring->frame_size);
        }
        else
            memcpy(ring->mem+(ring->write_pos*ring->frame_size), data,
                   len*ring->frame_size);

        ring->write_pos += len;
        ring->write_pos %= ring->length;
    }

    LeaveCriticalSection(&ring->cs);
}

void ReadRingBuffer(RingBuffer *ring, ALubyte *data, ALsizei len)
{
    int remain;

    EnterCriticalSection(&ring->cs);

    remain = ring->length - ring->read_pos;
    if(remain < len)
    {
        memcpy(data, ring->mem+(ring->read_pos*ring->frame_size), remain*ring->frame_size);
        memcpy(data+(remain*ring->frame_size), ring->mem, (len-remain)*ring->frame_size);
    }
    else
        memcpy(data, ring->mem+(ring->read_pos*ring->frame_size), len*ring->frame_size);

    ring->read_pos += len;
    ring->read_pos %= ring->length;

    LeaveCriticalSection(&ring->cs);
}
