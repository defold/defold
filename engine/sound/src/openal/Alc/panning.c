/**
 * OpenAL cross platform audio library
 * Copyright (C) 1999-2010 by authors.
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
#include "alu.h"

static void SetSpeakerArrangement(const char *name, ALfloat SpeakerAngle[MaxChannels],
                                  enum Channel Speaker2Chan[MaxChannels], ALint chans)
{
    char *confkey, *next;
    char *layout_str;
    char *sep, *end;
    enum Channel val;
    const char *str;
    int i;

    if(!ConfigValueStr(NULL, name, &str) && !ConfigValueStr(NULL, "layout", &str))
        return;

    layout_str = strdup(str);
    next = confkey = layout_str;
    while(next && *next)
    {
        confkey = next;
        next = strchr(confkey, ',');
        if(next)
        {
            *next = 0;
            do {
                next++;
            } while(isspace(*next) || *next == ',');
        }

        sep = strchr(confkey, '=');
        if(!sep || confkey == sep)
        {
            ERR("Malformed speaker key: %s\n", confkey);
            continue;
        }

        end = sep - 1;
        while(isspace(*end) && end != confkey)
            end--;
        *(++end) = 0;

        if(strcmp(confkey, "fl") == 0 || strcmp(confkey, "front-left") == 0)
            val = FrontLeft;
        else if(strcmp(confkey, "fr") == 0 || strcmp(confkey, "front-right") == 0)
            val = FrontRight;
        else if(strcmp(confkey, "fc") == 0 || strcmp(confkey, "front-center") == 0)
            val = FrontCenter;
        else if(strcmp(confkey, "bl") == 0 || strcmp(confkey, "back-left") == 0)
            val = BackLeft;
        else if(strcmp(confkey, "br") == 0 || strcmp(confkey, "back-right") == 0)
            val = BackRight;
        else if(strcmp(confkey, "bc") == 0 || strcmp(confkey, "back-center") == 0)
            val = BackCenter;
        else if(strcmp(confkey, "sl") == 0 || strcmp(confkey, "side-left") == 0)
            val = SideLeft;
        else if(strcmp(confkey, "sr") == 0 || strcmp(confkey, "side-right") == 0)
            val = SideRight;
        else
        {
            ERR("Unknown speaker for %s: \"%s\"\n", name, confkey);
            continue;
        }

        *(sep++) = 0;
        while(isspace(*sep))
            sep++;

        for(i = 0;i < chans;i++)
        {
            if(Speaker2Chan[i] == val)
            {
                long angle = strtol(sep, NULL, 10);
                if(angle >= -180 && angle <= 180)
                    SpeakerAngle[i] = angle * F_PI/180.0f;
                else
                    ERR("Invalid angle for speaker \"%s\": %ld\n", confkey, angle);
                break;
            }
        }
    }
    free(layout_str);
    layout_str = NULL;

    for(i = 0;i < chans;i++)
    {
        int min = i;
        int i2;

        for(i2 = i+1;i2 < chans;i2++)
        {
            if(SpeakerAngle[i2] < SpeakerAngle[min])
                min = i2;
        }

        if(min != i)
        {
            ALfloat tmpf;
            enum Channel tmpc;

            tmpf = SpeakerAngle[i];
            SpeakerAngle[i] = SpeakerAngle[min];
            SpeakerAngle[min] = tmpf;

            tmpc = Speaker2Chan[i];
            Speaker2Chan[i] = Speaker2Chan[min];
            Speaker2Chan[min] = tmpc;
        }
    }
}


/**
 * ComputeAngleGains
 *
 * Sets channel gains based on a given source's angle and its half-width. The
 * angle and hwidth parameters are in radians.
 */
ALvoid ComputeAngleGains(const ALCdevice *device, ALfloat angle, ALfloat hwidth, ALfloat ingain, ALfloat *gains)
{
    ALfloat tmpgains[MaxChannels] = { 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f };
    enum Channel Speaker2Chan[MaxChannels];
    ALfloat SpeakerAngle[MaxChannels];
    ALfloat langle, rangle;
    ALfloat a;
    ALuint i;

    for(i = 0;i < device->NumChan;i++)
        Speaker2Chan[i] = device->Speaker2Chan[i];
    for(i = 0;i < device->NumChan;i++)
        SpeakerAngle[i] = device->SpeakerAngle[i];

    /* Some easy special-cases first... */
    if(device->NumChan <= 1 || hwidth >= F_PI)
    {
        /* Full coverage for all speakers. */
        for(i = 0;i < device->NumChan;i++)
        {
            enum Channel chan = Speaker2Chan[i];
            gains[chan] = ingain;
        }
        return;
    }
    if(hwidth <= 0.0f)
    {
        /* Infinitely small sound point. */
        for(i = 0;i < device->NumChan-1;i++)
        {
            if(angle >= SpeakerAngle[i] && angle < SpeakerAngle[i+1])
            {
                /* Sound is between speakers i and i+1 */
                a =             (angle-SpeakerAngle[i]) /
                    (SpeakerAngle[i+1]-SpeakerAngle[i]);
                gains[Speaker2Chan[i]]   = sqrtf(1.0f-a) * ingain;
                gains[Speaker2Chan[i+1]] = sqrtf(     a) * ingain;
                return;
            }
        }
        /* Sound is between last and first speakers */
        if(angle < SpeakerAngle[0])
            angle += F_PI*2.0f;
        a =                       (angle-SpeakerAngle[i]) /
            (F_PI*2.0f + SpeakerAngle[0]-SpeakerAngle[i]);
        gains[Speaker2Chan[i]] = sqrtf(1.0f-a) * ingain;
        gains[Speaker2Chan[0]] = sqrtf(     a) * ingain;
        return;
    }

    if(fabsf(angle)+hwidth > F_PI)
    {
        /* The coverage area would go outside of -pi...+pi. Instead, rotate the
         * speaker angles so it would be as if angle=0, and keep them wrapped
         * within -pi...+pi. */
        if(angle > 0.0f)
        {
            ALuint done;
            ALuint i = 0;
            while(i < device->NumChan && device->SpeakerAngle[i]-angle < -F_PI)
                i++;
            for(done = 0;i < device->NumChan;done++)
            {
                SpeakerAngle[done] = device->SpeakerAngle[i]-angle;
                Speaker2Chan[done] = device->Speaker2Chan[i];
                i++;
            }
            for(i = 0;done < device->NumChan;i++)
            {
                SpeakerAngle[done] = device->SpeakerAngle[i]-angle + F_PI*2.0f;
                Speaker2Chan[done] = device->Speaker2Chan[i];
                done++;
            }
        }
        else
        {
            /* NOTE: '< device->NumChan' on the iterators is correct here since
             * we need to handle index 0. Because the iterators are unsigned,
             * they'll underflow and wrap to become 0xFFFFFFFF, which will
             * break as expected. */
            ALuint done;
            ALuint i = device->NumChan-1;
            while(i < device->NumChan && device->SpeakerAngle[i]-angle > F_PI)
                i--;
            for(done = device->NumChan-1;i < device->NumChan;done--)
            {
                SpeakerAngle[done] = device->SpeakerAngle[i]-angle;
                Speaker2Chan[done] = device->Speaker2Chan[i];
                i--;
            }
            for(i = device->NumChan-1;done < device->NumChan;i--)
            {
                SpeakerAngle[done] = device->SpeakerAngle[i]-angle - F_PI*2.0f;
                Speaker2Chan[done] = device->Speaker2Chan[i];
                done--;
            }
        }
        angle = 0.0f;
    }
    langle = angle - hwidth;
    rangle = angle + hwidth;

    /* First speaker */
    i = 0;
    do {
        ALuint last = device->NumChan-1;
        enum Channel chan = Speaker2Chan[i];

        if(SpeakerAngle[i] >= langle && SpeakerAngle[i] <= rangle)
        {
            tmpgains[chan] = 1.0f;
            continue;
        }

        if(SpeakerAngle[i] < langle && SpeakerAngle[i+1] > langle)
        {
            a =            (langle-SpeakerAngle[i]) /
                (SpeakerAngle[i+1]-SpeakerAngle[i]);
            tmpgains[chan] = lerp(tmpgains[chan], 1.0f, 1.0f-a);
        }
        if(SpeakerAngle[i] > rangle)
        {
            a =          (F_PI*2.0f + rangle-SpeakerAngle[last]) /
                (F_PI*2.0f + SpeakerAngle[i]-SpeakerAngle[last]);
            tmpgains[chan] = lerp(tmpgains[chan], 1.0f, a);
        }
        else if(SpeakerAngle[last] < rangle)
        {
            a =                      (rangle-SpeakerAngle[last]) /
                (F_PI*2.0f + SpeakerAngle[i]-SpeakerAngle[last]);
            tmpgains[chan] = lerp(tmpgains[chan], 1.0f, a);
        }
    } while(0);

    for(i = 1;i < device->NumChan-1;i++)
    {
        enum Channel chan = Speaker2Chan[i];
        if(SpeakerAngle[i] >= langle && SpeakerAngle[i] <= rangle)
        {
            tmpgains[chan] = 1.0f;
            continue;
        }

        if(SpeakerAngle[i] < langle && SpeakerAngle[i+1] > langle)
        {
            a =            (langle-SpeakerAngle[i]) /
                (SpeakerAngle[i+1]-SpeakerAngle[i]);
            tmpgains[chan] = lerp(tmpgains[chan], 1.0f, 1.0f-a);
        }
        if(SpeakerAngle[i] > rangle && SpeakerAngle[i-1] < rangle)
        {
            a =          (rangle-SpeakerAngle[i-1]) /
                (SpeakerAngle[i]-SpeakerAngle[i-1]);
            tmpgains[chan] = lerp(tmpgains[chan], 1.0f, a);
        }
    }

    /* Last speaker */
    i = device->NumChan-1;
    do {
        enum Channel chan = Speaker2Chan[i];
        if(SpeakerAngle[i] >= langle && SpeakerAngle[i] <= rangle)
        {
            tmpgains[Speaker2Chan[i]] = 1.0f;
            continue;
        }
        if(SpeakerAngle[i] > rangle && SpeakerAngle[i-1] < rangle)
        {
            a =          (rangle-SpeakerAngle[i-1]) /
                (SpeakerAngle[i]-SpeakerAngle[i-1]);
            tmpgains[chan] = lerp(tmpgains[chan], 1.0f, a);
        }
        if(SpeakerAngle[i] < langle)
        {
            a =                      (langle-SpeakerAngle[i]) /
                (F_PI*2.0f + SpeakerAngle[0]-SpeakerAngle[i]);
            tmpgains[chan] = lerp(tmpgains[chan], 1.0f, 1.0f-a);
        }
        else if(SpeakerAngle[0] > langle)
        {
            a =          (F_PI*2.0f + langle-SpeakerAngle[i]) /
                (F_PI*2.0f + SpeakerAngle[0]-SpeakerAngle[i]);
            tmpgains[chan] = lerp(tmpgains[chan], 1.0f, 1.0f-a);
        }
    } while(0);

    for(i = 0;i < device->NumChan;i++)
    {
        enum Channel chan = device->Speaker2Chan[i];
        gains[chan] = sqrtf(tmpgains[chan]) * ingain;
    }
}


ALvoid aluInitPanning(ALCdevice *Device)
{
    const char *layoutname = NULL;
    enum Channel *Speaker2Chan;
    ALfloat *SpeakerAngle;

    Speaker2Chan = Device->Speaker2Chan;
    SpeakerAngle = Device->SpeakerAngle;
    switch(Device->FmtChans)
    {
        case DevFmtMono:
            Device->NumChan = 1;
            Speaker2Chan[0] = FrontCenter;
            SpeakerAngle[0] = F_PI/180.0f * 0.0f;
            layoutname = NULL;
            break;

        case DevFmtStereo:
            Device->NumChan = 2;
            Speaker2Chan[0] = FrontLeft;
            Speaker2Chan[1] = FrontRight;
            SpeakerAngle[0] = F_PI/180.0f * -90.0f;
            SpeakerAngle[1] = F_PI/180.0f *  90.0f;
            layoutname = "layout_stereo";
            break;

        case DevFmtQuad:
            Device->NumChan = 4;
            Speaker2Chan[0] = BackLeft;
            Speaker2Chan[1] = FrontLeft;
            Speaker2Chan[2] = FrontRight;
            Speaker2Chan[3] = BackRight;
            SpeakerAngle[0] = F_PI/180.0f * -135.0f;
            SpeakerAngle[1] = F_PI/180.0f *  -45.0f;
            SpeakerAngle[2] = F_PI/180.0f *   45.0f;
            SpeakerAngle[3] = F_PI/180.0f *  135.0f;
            layoutname = "layout_quad";
            break;

        case DevFmtX51:
            Device->NumChan = 5;
            Speaker2Chan[0] = BackLeft;
            Speaker2Chan[1] = FrontLeft;
            Speaker2Chan[2] = FrontCenter;
            Speaker2Chan[3] = FrontRight;
            Speaker2Chan[4] = BackRight;
            SpeakerAngle[0] = F_PI/180.0f * -110.0f;
            SpeakerAngle[1] = F_PI/180.0f *  -30.0f;
            SpeakerAngle[2] = F_PI/180.0f *    0.0f;
            SpeakerAngle[3] = F_PI/180.0f *   30.0f;
            SpeakerAngle[4] = F_PI/180.0f *  110.0f;
            layoutname = "layout_surround51";
            break;

        case DevFmtX51Side:
            Device->NumChan = 5;
            Speaker2Chan[0] = SideLeft;
            Speaker2Chan[1] = FrontLeft;
            Speaker2Chan[2] = FrontCenter;
            Speaker2Chan[3] = FrontRight;
            Speaker2Chan[4] = SideRight;
            SpeakerAngle[0] = F_PI/180.0f * -90.0f;
            SpeakerAngle[1] = F_PI/180.0f * -30.0f;
            SpeakerAngle[2] = F_PI/180.0f *   0.0f;
            SpeakerAngle[3] = F_PI/180.0f *  30.0f;
            SpeakerAngle[4] = F_PI/180.0f *  90.0f;
            layoutname = "layout_side51";
            break;

        case DevFmtX61:
            Device->NumChan = 6;
            Speaker2Chan[0] = SideLeft;
            Speaker2Chan[1] = FrontLeft;
            Speaker2Chan[2] = FrontCenter;
            Speaker2Chan[3] = FrontRight;
            Speaker2Chan[4] = SideRight;
            Speaker2Chan[5] = BackCenter;
            SpeakerAngle[0] = F_PI/180.0f * -90.0f;
            SpeakerAngle[1] = F_PI/180.0f * -30.0f;
            SpeakerAngle[2] = F_PI/180.0f *   0.0f;
            SpeakerAngle[3] = F_PI/180.0f *  30.0f;
            SpeakerAngle[4] = F_PI/180.0f *  90.0f;
            SpeakerAngle[5] = F_PI/180.0f * 180.0f;
            layoutname = "layout_surround61";
            break;

        case DevFmtX71:
            Device->NumChan = 7;
            Speaker2Chan[0] = BackLeft;
            Speaker2Chan[1] = SideLeft;
            Speaker2Chan[2] = FrontLeft;
            Speaker2Chan[3] = FrontCenter;
            Speaker2Chan[4] = FrontRight;
            Speaker2Chan[5] = SideRight;
            Speaker2Chan[6] = BackRight;
            SpeakerAngle[0] = F_PI/180.0f * -150.0f;
            SpeakerAngle[1] = F_PI/180.0f *  -90.0f;
            SpeakerAngle[2] = F_PI/180.0f *  -30.0f;
            SpeakerAngle[3] = F_PI/180.0f *    0.0f;
            SpeakerAngle[4] = F_PI/180.0f *   30.0f;
            SpeakerAngle[5] = F_PI/180.0f *   90.0f;
            SpeakerAngle[6] = F_PI/180.0f *  150.0f;
            layoutname = "layout_surround71";
            break;
    }
    if(layoutname && Device->Type != Loopback)
        SetSpeakerArrangement(layoutname, SpeakerAngle, Speaker2Chan, Device->NumChan);
}
