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

#include "alMain.h"
#include "AL/alc.h"
#include "alError.h"
#include "alListener.h"
#include "alSource.h"

AL_API ALvoid AL_APIENTRY alListenerf(ALenum param, ALfloat value)
{
    ALCcontext *Context;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        switch(param)
        {
            case AL_GAIN:
                CHECK_VALUE(Context, value >= 0.0f && isfinite(value));

                Context->Listener->Gain = value;
                Context->UpdateSources = AL_TRUE;
                break;

            case AL_METERS_PER_UNIT:
                CHECK_VALUE(Context, value >= 0.0f && isfinite(value));

                Context->Listener->MetersPerUnit = value;
                Context->UpdateSources = AL_TRUE;
                break;

            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alListener3f(ALenum param, ALfloat value1, ALfloat value2, ALfloat value3)
{
    ALCcontext *Context;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        switch(param)
        {
            case AL_POSITION:
                CHECK_VALUE(Context, isfinite(value1) && isfinite(value2) && isfinite(value3));

                LockContext(Context);
                Context->Listener->Position[0] = value1;
                Context->Listener->Position[1] = value2;
                Context->Listener->Position[2] = value3;
                Context->UpdateSources = AL_TRUE;
                UnlockContext(Context);
                break;

            case AL_VELOCITY:
                CHECK_VALUE(Context, isfinite(value1) && isfinite(value2) && isfinite(value3));

                LockContext(Context);
                Context->Listener->Velocity[0] = value1;
                Context->Listener->Velocity[1] = value2;
                Context->Listener->Velocity[2] = value3;
                Context->UpdateSources = AL_TRUE;
                UnlockContext(Context);
                break;

            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alListenerfv(ALenum param, const ALfloat *values)
{
    ALCcontext *Context;

    if(values)
    {
        switch(param)
        {
            case AL_GAIN:
            case AL_METERS_PER_UNIT:
                alListenerf(param, values[0]);
                return;

            case AL_POSITION:
            case AL_VELOCITY:
                alListener3f(param, values[0], values[1], values[2]);
                return;
        }
    }

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, values);
        switch(param)
        {
            case AL_ORIENTATION:
                CHECK_VALUE(Context, isfinite(values[0]) && isfinite(values[1]) &&
                                     isfinite(values[2]) && isfinite(values[3]) &&
                                     isfinite(values[4]) && isfinite(values[5]));

                LockContext(Context);
                /* AT then UP */
                Context->Listener->Forward[0] = values[0];
                Context->Listener->Forward[1] = values[1];
                Context->Listener->Forward[2] = values[2];
                Context->Listener->Up[0] = values[3];
                Context->Listener->Up[1] = values[4];
                Context->Listener->Up[2] = values[5];
                Context->UpdateSources = AL_TRUE;
                UnlockContext(Context);
                break;

            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alListeneri(ALenum param, ALint value)
{
    ALCcontext *Context;

    (void)value;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        switch(param)
        {
            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}


AL_API void AL_APIENTRY alListener3i(ALenum param, ALint value1, ALint value2, ALint value3)
{
    ALCcontext *Context;

    switch(param)
    {
        case AL_POSITION:
        case AL_VELOCITY:
            alListener3f(param, (ALfloat)value1, (ALfloat)value2, (ALfloat)value3);
            return;
    }

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        switch(param)
        {
            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}


AL_API void AL_APIENTRY alListeneriv(ALenum param, const ALint *values)
{
    ALCcontext *Context;

    if(values)
    {
        ALfloat fvals[6];
        switch(param)
        {
            case AL_POSITION:
            case AL_VELOCITY:
                alListener3f(param, (ALfloat)values[0], (ALfloat)values[1], (ALfloat)values[2]);
                return;

            case AL_ORIENTATION:
                fvals[0] = (ALfloat)values[0];
                fvals[1] = (ALfloat)values[1];
                fvals[2] = (ALfloat)values[2];
                fvals[3] = (ALfloat)values[3];
                fvals[4] = (ALfloat)values[4];
                fvals[5] = (ALfloat)values[5];
                alListenerfv(param, fvals);
                return;
        }
    }

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, values);
        switch(param)
        {
            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alGetListenerf(ALenum param, ALfloat *value)
{
    ALCcontext *Context;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, value);
        switch(param)
        {
            case AL_GAIN:
                *value = Context->Listener->Gain;
                break;

            case AL_METERS_PER_UNIT:
                *value = Context->Listener->MetersPerUnit;
                break;

            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alGetListener3f(ALenum param, ALfloat *value1, ALfloat *value2, ALfloat *value3)
{
    ALCcontext *Context;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, value1 && value2 && value3);
        switch(param)
        {
            case AL_POSITION:
                LockContext(Context);
                *value1 = Context->Listener->Position[0];
                *value2 = Context->Listener->Position[1];
                *value3 = Context->Listener->Position[2];
                UnlockContext(Context);
                break;

            case AL_VELOCITY:
                LockContext(Context);
                *value1 = Context->Listener->Velocity[0];
                *value2 = Context->Listener->Velocity[1];
                *value3 = Context->Listener->Velocity[2];
                UnlockContext(Context);
                break;

            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alGetListenerfv(ALenum param, ALfloat *values)
{
    ALCcontext *Context;

    switch(param)
    {
        case AL_GAIN:
        case AL_METERS_PER_UNIT:
            alGetListenerf(param, values);
            return;

        case AL_POSITION:
        case AL_VELOCITY:
            alGetListener3f(param, values+0, values+1, values+2);
            return;
    }

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, values);
        switch(param)
        {
            case AL_ORIENTATION:
                LockContext(Context);
                // AT then UP
                values[0] = Context->Listener->Forward[0];
                values[1] = Context->Listener->Forward[1];
                values[2] = Context->Listener->Forward[2];
                values[3] = Context->Listener->Up[0];
                values[4] = Context->Listener->Up[1];
                values[5] = Context->Listener->Up[2];
                UnlockContext(Context);
                break;

            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}


AL_API ALvoid AL_APIENTRY alGetListeneri(ALenum param, ALint *value)
{
    ALCcontext *Context;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, value);
        switch(param)
        {
            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}


AL_API void AL_APIENTRY alGetListener3i(ALenum param, ALint *value1, ALint *value2, ALint *value3)
{
    ALCcontext *Context;

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, value1 && value2 && value3);
        switch (param)
        {
            case AL_POSITION:
                LockContext(Context);
                *value1 = (ALint)Context->Listener->Position[0];
                *value2 = (ALint)Context->Listener->Position[1];
                *value3 = (ALint)Context->Listener->Position[2];
                UnlockContext(Context);
                break;

            case AL_VELOCITY:
                LockContext(Context);
                *value1 = (ALint)Context->Listener->Velocity[0];
                *value2 = (ALint)Context->Listener->Velocity[1];
                *value3 = (ALint)Context->Listener->Velocity[2];
                UnlockContext(Context);
                break;

            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}


AL_API void AL_APIENTRY alGetListeneriv(ALenum param, ALint* values)
{
    ALCcontext *Context;

    switch(param)
    {
        case AL_POSITION:
        case AL_VELOCITY:
            alGetListener3i(param, values+0, values+1, values+2);
            return;
    }

    Context = GetContextRef();
    if(!Context) return;

    al_try
    {
        CHECK_VALUE(Context, values);
        switch(param)
        {
            case AL_ORIENTATION:
                LockContext(Context);
                // AT then UP
                values[0] = (ALint)Context->Listener->Forward[0];
                values[1] = (ALint)Context->Listener->Forward[1];
                values[2] = (ALint)Context->Listener->Forward[2];
                values[3] = (ALint)Context->Listener->Up[0];
                values[4] = (ALint)Context->Listener->Up[1];
                values[5] = (ALint)Context->Listener->Up[2];
                UnlockContext(Context);
                break;

            default:
                al_throwerr(Context, AL_INVALID_ENUM);
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
}
