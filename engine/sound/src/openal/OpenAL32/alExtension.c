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
#include <string.h>
#include <ctype.h>

#include "alError.h"
#include "alMain.h"
#include "alFilter.h"
#include "alEffect.h"
#include "alAuxEffectSlot.h"
#include "alSource.h"
#include "alBuffer.h"
#include "AL/al.h"
#include "AL/alc.h"


const struct EffectList EffectList[] = {
    { "eaxreverb",  EAXREVERB,  "AL_EFFECT_EAXREVERB",      AL_EFFECT_EAXREVERB },
    { "reverb",     REVERB,     "AL_EFFECT_REVERB",         AL_EFFECT_REVERB },
    { "echo",       ECHO,       "AL_EFFECT_ECHO",           AL_EFFECT_ECHO },
    { "modulator",  MODULATOR,  "AL_EFFECT_RING_MODULATOR", AL_EFFECT_RING_MODULATOR },
    { "dedicated",  DEDICATED,  "AL_EFFECT_DEDICATED_LOW_FREQUENCY_EFFECT", AL_EFFECT_DEDICATED_LOW_FREQUENCY_EFFECT },
    { "dedicated",  DEDICATED,  "AL_EFFECT_DEDICATED_DIALOGUE", AL_EFFECT_DEDICATED_DIALOGUE },
    { "chorus",     CHORUS,     "AL_EFFECT_CHORUS",         AL_EFFECT_CHORUS },
    { "flanger",    FLANGER,    "AL_EFFECT_FLANGER",        AL_EFFECT_FLANGER },
    { "equalizer",  EQUALIZER,  "AL_EFFECT_EQUALIZER",      AL_EFFECT_EQUALIZER },
    { "distortion", DISTORTION, "AL_EFFECT_DISTORTION",     AL_EFFECT_DISTORTION },
    { NULL, 0, NULL, (ALenum)0 }
};


AL_API ALboolean AL_APIENTRY alIsExtensionPresent(const ALchar *extName)
{
    ALboolean ret = AL_FALSE;
    ALCcontext *Context;
    const char *ptr;
    size_t len;

    Context = GetContextRef();
    if(!Context) return AL_FALSE;

    al_try
    {
        CHECK_VALUE(Context, extName);

        len = strlen(extName);
        ptr = Context->ExtensionList;
        while(ptr && *ptr)
        {
            if(strncasecmp(ptr, extName, len) == 0 &&
               (ptr[len] == '\0' || isspace(ptr[len])))
            {
                ret = AL_TRUE;
                break;
            }
            if((ptr=strchr(ptr, ' ')) != NULL)
            {
                do {
                    ++ptr;
                } while(isspace(*ptr));
            }
        }
    }
    al_endtry;

    ALCcontext_DecRef(Context);
    return ret;
}


AL_API ALvoid* AL_APIENTRY alGetProcAddress(const ALchar *funcName)
{
    if(!funcName)
        return NULL;
    return alcGetProcAddress(NULL, funcName);
}

AL_API ALenum AL_APIENTRY alGetEnumValue(const ALchar *enumName)
{
    if(!enumName)
        return (ALenum)0;
    return alcGetEnumValue(NULL, enumName);
}
