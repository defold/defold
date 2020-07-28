/**
 * OpenAL cross platform audio library
 * Copyright (C) 2011 by Chris Robinson
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
#include <ctype.h>

#include "AL/al.h"
#include "AL/alc.h"
#include "alMain.h"
#include "alSource.h"
#include "alu.h"


#ifndef PATH_MAX
#define PATH_MAX 4096
#endif


/* Current data set limits defined by the makehrtf utility. */
#define MIN_IR_SIZE                  (8)
#define MAX_IR_SIZE                  (128)
#define MOD_IR_SIZE                  (8)

#define MIN_EV_COUNT                 (5)
#define MAX_EV_COUNT                 (128)

#define MIN_AZ_COUNT                 (1)
#define MAX_AZ_COUNT                 (128)

struct Hrtf {
    ALuint sampleRate;
    ALuint irSize;
    ALubyte evCount;

    const ALubyte *azCount;
    const ALushort *evOffset;
    const ALshort *coeffs;
    const ALubyte *delays;

    struct Hrtf *next;
};

static const ALchar magicMarker00[8] = "MinPHR00";
static const ALchar magicMarker01[8] = "MinPHR01";

/* Define the default HRTF:
 *  ALubyte  defaultAzCount  [DefaultHrtf.evCount]
 *  ALushort defaultEvOffset [DefaultHrtf.evCount]
 *  ALshort  defaultCoeffs   [DefaultHrtf.irCount * defaultHrtf.irSize]
 *  ALubyte  defaultDelays   [DefaultHrtf.irCount]
 *
 *  struct Hrtf DefaultHrtf
 */
#include "hrtf_tables.inc"

static struct Hrtf *LoadedHrtfs = NULL;

/* Calculate the elevation indices given the polar elevation in radians.
 * This will return two indices between 0 and (Hrtf->evCount - 1) and an
 * interpolation factor between 0.0 and 1.0.
 */
static void CalcEvIndices(const struct Hrtf *Hrtf, ALfloat ev, ALuint *evidx, ALfloat *evmu)
{
    ev = (F_PI_2 + ev) * (Hrtf->evCount-1) / F_PI;
    evidx[0] = fastf2u(ev);
    evidx[1] = minu(evidx[0] + 1, Hrtf->evCount-1);
    *evmu = ev - evidx[0];
}

/* Calculate the azimuth indices given the polar azimuth in radians.  This
 * will return two indices between 0 and (Hrtf->azCount[ei] - 1) and an
 * interpolation factor between 0.0 and 1.0.
 */
static void CalcAzIndices(const struct Hrtf *Hrtf, ALuint evidx, ALfloat az, ALuint *azidx, ALfloat *azmu)
{
    az = (F_PI*2.0f + az) * Hrtf->azCount[evidx] / (F_PI*2.0f);
    azidx[0] = fastf2u(az) % Hrtf->azCount[evidx];
    azidx[1] = (azidx[0] + 1) % Hrtf->azCount[evidx];
    *azmu = az - floorf(az);
}

/* Calculates the normalized HRTF transition factor (delta) from the changes
 * in gain and listener to source angle between updates.  The result is a
 * normalized delta factor that can be used to calculate moving HRIR stepping
 * values.
 */
ALfloat CalcHrtfDelta(ALfloat oldGain, ALfloat newGain, const ALfloat olddir[3], const ALfloat newdir[3])
{
    ALfloat gainChange, angleChange, change;

    // Calculate the normalized dB gain change.
    newGain = maxf(newGain, 0.0001f);
    oldGain = maxf(oldGain, 0.0001f);
    gainChange = fabsf(log10f(newGain / oldGain) / log10f(0.0001f));

    // Calculate the normalized listener to source angle change when there is
    // enough gain to notice it.
    angleChange = 0.0f;
    if(gainChange > 0.0001f || newGain > 0.0001f)
    {
        // No angle change when the directions are equal or degenerate (when
        // both have zero length).
        if(newdir[0]-olddir[0] || newdir[1]-olddir[1] || newdir[2]-olddir[2])
            angleChange = acosf(olddir[0]*newdir[0] +
                                olddir[1]*newdir[1] +
                                olddir[2]*newdir[2]) / F_PI;

    }

    // Use the largest of the two changes for the delta factor, and apply a
    // significance shaping function to it.
    change = maxf(angleChange * 25.0f, gainChange) * 2.0f;
    return minf(change, 1.0f);
}

/* Calculates static HRIR coefficients and delays for the given polar
 * elevation and azimuth in radians.  Linear interpolation is used to
 * increase the apparent resolution of the HRIR data set.  The coefficients
 * are also normalized and attenuated by the specified gain.
 */
void GetLerpedHrtfCoeffs(const struct Hrtf *Hrtf, ALfloat elevation, ALfloat azimuth, ALfloat gain, ALfloat (*coeffs)[2], ALuint *delays)
{
    ALuint evidx[2], azidx[2];
    ALuint lidx[4], ridx[4];
    ALfloat mu[3], blend[4];
    ALuint i;

    // Claculate elevation indices and interpolation factor.
    CalcEvIndices(Hrtf, elevation, evidx, &mu[2]);

    // Calculate azimuth indices and interpolation factor for the first
    // elevation.
    CalcAzIndices(Hrtf, evidx[0], azimuth, azidx, &mu[0]);

    // Calculate the first set of linear HRIR indices for left and right
    // channels.
    lidx[0] = Hrtf->evOffset[evidx[0]] + azidx[0];
    lidx[1] = Hrtf->evOffset[evidx[0]] + azidx[1];
    ridx[0] = Hrtf->evOffset[evidx[0]] + ((Hrtf->azCount[evidx[0]]-azidx[0]) % Hrtf->azCount[evidx[0]]);
    ridx[1] = Hrtf->evOffset[evidx[0]] + ((Hrtf->azCount[evidx[0]]-azidx[1]) % Hrtf->azCount[evidx[0]]);

    // Calculate azimuth indices and interpolation factor for the second
    // elevation.
    CalcAzIndices(Hrtf, evidx[1], azimuth, azidx, &mu[1]);

    // Calculate the second set of linear HRIR indices for left and right
    // channels.
    lidx[2] = Hrtf->evOffset[evidx[1]] + azidx[0];
    lidx[3] = Hrtf->evOffset[evidx[1]] + azidx[1];
    ridx[2] = Hrtf->evOffset[evidx[1]] + ((Hrtf->azCount[evidx[1]]-azidx[0]) % Hrtf->azCount[evidx[1]]);
    ridx[3] = Hrtf->evOffset[evidx[1]] + ((Hrtf->azCount[evidx[1]]-azidx[1]) % Hrtf->azCount[evidx[1]]);

    /* Calculate 4 blending weights for 2D bilinear interpolation. */
    blend[0] = (1.0f-mu[0]) * (1.0f-mu[2]);
    blend[1] = (     mu[0]) * (1.0f-mu[2]);
    blend[2] = (1.0f-mu[1]) * (     mu[2]);
    blend[3] = (     mu[1]) * (     mu[2]);

    /* Calculate the HRIR delays using linear interpolation. */
    delays[0] = fastf2u(Hrtf->delays[lidx[0]]*blend[0] + Hrtf->delays[lidx[1]]*blend[1] +
                        Hrtf->delays[lidx[2]]*blend[2] + Hrtf->delays[lidx[3]]*blend[3] +
                        0.5f) << HRTFDELAY_BITS;
    delays[1] = fastf2u(Hrtf->delays[ridx[0]]*blend[0] + Hrtf->delays[ridx[1]]*blend[1] +
                        Hrtf->delays[ridx[2]]*blend[2] + Hrtf->delays[ridx[3]]*blend[3] +
                        0.5f) << HRTFDELAY_BITS;

    /* Calculate the sample offsets for the HRIR indices. */
    lidx[0] *= Hrtf->irSize;
    lidx[1] *= Hrtf->irSize;
    lidx[2] *= Hrtf->irSize;
    lidx[3] *= Hrtf->irSize;
    ridx[0] *= Hrtf->irSize;
    ridx[1] *= Hrtf->irSize;
    ridx[2] *= Hrtf->irSize;
    ridx[3] *= Hrtf->irSize;

    /* Calculate the normalized and attenuated HRIR coefficients using linear
     * interpolation when there is enough gain to warrant it.  Zero the
     * coefficients if gain is too low.
     */
    if(gain > 0.0001f)
    {
        gain *= 1.0f/32767.0f;
        for(i = 0;i < Hrtf->irSize;i++)
        {
            coeffs[i][0] = (Hrtf->coeffs[lidx[0]+i]*blend[0] +
                            Hrtf->coeffs[lidx[1]+i]*blend[1] +
                            Hrtf->coeffs[lidx[2]+i]*blend[2] +
                            Hrtf->coeffs[lidx[3]+i]*blend[3]) * gain;
            coeffs[i][1] = (Hrtf->coeffs[ridx[0]+i]*blend[0] +
                            Hrtf->coeffs[ridx[1]+i]*blend[1] +
                            Hrtf->coeffs[ridx[2]+i]*blend[2] +
                            Hrtf->coeffs[ridx[3]+i]*blend[3]) * gain;
        }
    }
    else
    {
        for(i = 0;i < Hrtf->irSize;i++)
        {
            coeffs[i][0] = 0.0f;
            coeffs[i][1] = 0.0f;
        }
    }
}

/* Calculates the moving HRIR target coefficients, target delays, and
 * stepping values for the given polar elevation and azimuth in radians.
 * Linear interpolation is used to increase the apparent resolution of the
 * HRIR data set.  The coefficients are also normalized and attenuated by the
 * specified gain.  Stepping resolution and count is determined using the
 * given delta factor between 0.0 and 1.0.
 */
ALuint GetMovingHrtfCoeffs(const struct Hrtf *Hrtf, ALfloat elevation, ALfloat azimuth, ALfloat gain, ALfloat delta, ALint counter, ALfloat (*coeffs)[2], ALuint *delays, ALfloat (*coeffStep)[2], ALint *delayStep)
{
    ALuint evidx[2], azidx[2];
    ALuint lidx[4], ridx[4];
    ALfloat mu[3], blend[4];
    ALfloat left, right;
    ALfloat step;
    ALuint i;

    // Claculate elevation indices and interpolation factor.
    CalcEvIndices(Hrtf, elevation, evidx, &mu[2]);

    // Calculate azimuth indices and interpolation factor for the first
    // elevation.
    CalcAzIndices(Hrtf, evidx[0], azimuth, azidx, &mu[0]);

    // Calculate the first set of linear HRIR indices for left and right
    // channels.
    lidx[0] = Hrtf->evOffset[evidx[0]] + azidx[0];
    lidx[1] = Hrtf->evOffset[evidx[0]] + azidx[1];
    ridx[0] = Hrtf->evOffset[evidx[0]] + ((Hrtf->azCount[evidx[0]]-azidx[0]) % Hrtf->azCount[evidx[0]]);
    ridx[1] = Hrtf->evOffset[evidx[0]] + ((Hrtf->azCount[evidx[0]]-azidx[1]) % Hrtf->azCount[evidx[0]]);

    // Calculate azimuth indices and interpolation factor for the second
    // elevation.
    CalcAzIndices(Hrtf, evidx[1], azimuth, azidx, &mu[1]);

    // Calculate the second set of linear HRIR indices for left and right
    // channels.
    lidx[2] = Hrtf->evOffset[evidx[1]] + azidx[0];
    lidx[3] = Hrtf->evOffset[evidx[1]] + azidx[1];
    ridx[2] = Hrtf->evOffset[evidx[1]] + ((Hrtf->azCount[evidx[1]]-azidx[0]) % Hrtf->azCount[evidx[1]]);
    ridx[3] = Hrtf->evOffset[evidx[1]] + ((Hrtf->azCount[evidx[1]]-azidx[1]) % Hrtf->azCount[evidx[1]]);

    // Calculate the stepping parameters.
    delta = maxf(floorf(delta*(Hrtf->sampleRate*0.015f) + 0.5f), 1.0f);
    step = 1.0f / delta;

    /* Calculate 4 blending weights for 2D bilinear interpolation. */
    blend[0] = (1.0f-mu[0]) * (1.0f-mu[2]);
    blend[1] = (     mu[0]) * (1.0f-mu[2]);
    blend[2] = (1.0f-mu[1]) * (     mu[2]);
    blend[3] = (     mu[1]) * (     mu[2]);

    /* Calculate the HRIR delays using linear interpolation.  Then calculate
     * the delay stepping values using the target and previous running
     * delays.
     */
    left = (ALfloat)(delays[0] - (delayStep[0] * counter));
    right = (ALfloat)(delays[1] - (delayStep[1] * counter));

    delays[0] = fastf2u(Hrtf->delays[lidx[0]]*blend[0] + Hrtf->delays[lidx[1]]*blend[1] +
                        Hrtf->delays[lidx[2]]*blend[2] + Hrtf->delays[lidx[3]]*blend[3] +
                        0.5f) << HRTFDELAY_BITS;
    delays[1] = fastf2u(Hrtf->delays[ridx[0]]*blend[0] + Hrtf->delays[ridx[1]]*blend[1] +
                        Hrtf->delays[ridx[2]]*blend[2] + Hrtf->delays[ridx[3]]*blend[3] +
                        0.5f) << HRTFDELAY_BITS;

    delayStep[0] = fastf2i(step * (delays[0] - left));
    delayStep[1] = fastf2i(step * (delays[1] - right));

    /* Calculate the sample offsets for the HRIR indices. */
    lidx[0] *= Hrtf->irSize;
    lidx[1] *= Hrtf->irSize;
    lidx[2] *= Hrtf->irSize;
    lidx[3] *= Hrtf->irSize;
    ridx[0] *= Hrtf->irSize;
    ridx[1] *= Hrtf->irSize;
    ridx[2] *= Hrtf->irSize;
    ridx[3] *= Hrtf->irSize;

    /* Calculate the normalized and attenuated target HRIR coefficients using
     * linear interpolation when there is enough gain to warrant it.  Zero
     * the target coefficients if gain is too low.  Then calculate the
     * coefficient stepping values using the target and previous running
     * coefficients.
     */
    if(gain > 0.0001f)
    {
        gain *= 1.0f/32767.0f;
        for(i = 0;i < HRIR_LENGTH;i++)
        {
            left = coeffs[i][0] - (coeffStep[i][0] * counter);
            right = coeffs[i][1] - (coeffStep[i][1] * counter);

            coeffs[i][0] = (Hrtf->coeffs[lidx[0]+i]*blend[0] +
                            Hrtf->coeffs[lidx[1]+i]*blend[1] +
                            Hrtf->coeffs[lidx[2]+i]*blend[2] +
                            Hrtf->coeffs[lidx[3]+i]*blend[3]) * gain;
            coeffs[i][1] = (Hrtf->coeffs[ridx[0]+i]*blend[0] +
                            Hrtf->coeffs[ridx[1]+i]*blend[1] +
                            Hrtf->coeffs[ridx[2]+i]*blend[2] +
                            Hrtf->coeffs[ridx[3]+i]*blend[3]) * gain;

            coeffStep[i][0] = step * (coeffs[i][0] - left);
            coeffStep[i][1] = step * (coeffs[i][1] - right);
        }
    }
    else
    {
        for(i = 0;i < HRIR_LENGTH;i++)
        {
            left = coeffs[i][0] - (coeffStep[i][0] * counter);
            right = coeffs[i][1] - (coeffStep[i][1] * counter);

            coeffs[i][0] = 0.0f;
            coeffs[i][1] = 0.0f;

            coeffStep[i][0] = step * -left;
            coeffStep[i][1] = step * -right;
        }
    }

    /* The stepping count is the number of samples necessary for the HRIR to
     * complete its transition.  The mixer will only apply stepping for this
     * many samples.
     */
    return fastf2u(delta);
}


static struct Hrtf *LoadHrtf00(FILE *f, ALuint deviceRate)
{
    const ALubyte maxDelay = SRC_HISTORY_LENGTH-1;
    struct Hrtf *Hrtf = NULL;
    ALboolean failed = AL_FALSE;
    ALuint rate = 0, irCount = 0;
    ALushort irSize = 0;
    ALubyte evCount = 0;
    ALubyte *azCount = NULL;
    ALushort *evOffset = NULL;
    ALshort *coeffs = NULL;
    ALubyte *delays = NULL;
    ALuint i, j;

    rate  = fgetc(f);
    rate |= fgetc(f)<<8;
    rate |= fgetc(f)<<16;
    rate |= fgetc(f)<<24;

    irCount  = fgetc(f);
    irCount |= fgetc(f)<<8;

    irSize  = fgetc(f);
    irSize |= fgetc(f)<<8;

    evCount = fgetc(f);

    if(rate != deviceRate)
    {
        ERR("HRIR rate does not match device rate: rate=%d (%d)\n",
            rate, deviceRate);
        failed = AL_TRUE;
    }
    if(irSize < MIN_IR_SIZE || irSize > MAX_IR_SIZE || (irSize%MOD_IR_SIZE))
    {
        ERR("Unsupported HRIR size: irSize=%d (%d to %d by %d)\n",
            irSize, MIN_IR_SIZE, MAX_IR_SIZE, MOD_IR_SIZE);
        failed = AL_TRUE;
    }
    if(evCount < MIN_EV_COUNT || evCount > MAX_EV_COUNT)
    {
        ERR("Unsupported elevation count: evCount=%d (%d to %d)\n",
            evCount, MIN_EV_COUNT, MAX_EV_COUNT);
        failed = AL_TRUE;
    }

    if(failed)
        return NULL;

    azCount = malloc(sizeof(azCount[0])*evCount);
    evOffset = malloc(sizeof(evOffset[0])*evCount);
    if(azCount == NULL || evOffset == NULL)
    {
        ERR("Out of memory.\n");
        failed = AL_TRUE;
    }

    if(!failed)
    {
        evOffset[0]  = fgetc(f);
        evOffset[0] |= fgetc(f)<<8;
        for(i = 1;i < evCount;i++)
        {
            evOffset[i]  = fgetc(f);
            evOffset[i] |= fgetc(f)<<8;
            if(evOffset[i] <= evOffset[i-1])
            {
                ERR("Invalid evOffset: evOffset[%d]=%d (last=%d)\n",
                    i, evOffset[i], evOffset[i-1]);
                failed = AL_TRUE;
            }

            azCount[i-1] = evOffset[i] - evOffset[i-1];
            if(azCount[i-1] < MIN_AZ_COUNT || azCount[i-1] > MAX_AZ_COUNT)
            {
                ERR("Unsupported azimuth count: azCount[%d]=%d (%d to %d)\n",
                    i-1, azCount[i-1], MIN_AZ_COUNT, MAX_AZ_COUNT);
                failed = AL_TRUE;
            }
        }
        if(irCount <= evOffset[i-1])
        {
            ERR("Invalid evOffset: evOffset[%d]=%d (irCount=%d)\n",
                i-1, evOffset[i-1], irCount);
            failed = AL_TRUE;
        }

        azCount[i-1] = irCount - evOffset[i-1];
        if(azCount[i-1] < MIN_AZ_COUNT || azCount[i-1] > MAX_AZ_COUNT)
        {
            ERR("Unsupported azimuth count: azCount[%d]=%d (%d to %d)\n",
                i-1, azCount[i-1], MIN_AZ_COUNT, MAX_AZ_COUNT);
            failed = AL_TRUE;
        }
    }

    if(!failed)
    {
        coeffs = malloc(sizeof(coeffs[0])*irSize*irCount);
        delays = malloc(sizeof(delays[0])*irCount);
        if(coeffs == NULL || delays == NULL)
        {
            ERR("Out of memory.\n");
            failed = AL_TRUE;
        }
    }

    if(!failed)
    {
        for(i = 0;i < irCount*irSize;i+=irSize)
        {
            for(j = 0;j < irSize;j++)
            {
                ALshort coeff;
                coeff  = fgetc(f);
                coeff |= fgetc(f)<<8;
                coeffs[i+j] = coeff;
            }
        }
        for(i = 0;i < irCount;i++)
        {
            delays[i] = fgetc(f);
            if(delays[i] > maxDelay)
            {
                ERR("Invalid delays[%d]: %d (%d)\n", i, delays[i], maxDelay);
                failed = AL_TRUE;
            }
        }

        if(feof(f))
        {
            ERR("Premature end of data\n");
            failed = AL_TRUE;
        }
    }

    if(!failed)
    {
        Hrtf = malloc(sizeof(struct Hrtf));
        if(Hrtf == NULL)
        {
            ERR("Out of memory.\n");
            failed = AL_TRUE;
        }
    }

    if(!failed)
    {
        Hrtf->sampleRate = rate;
        Hrtf->irSize = irSize;
        Hrtf->evCount = evCount;
        Hrtf->azCount = azCount;
        Hrtf->evOffset = evOffset;
        Hrtf->coeffs = coeffs;
        Hrtf->delays = delays;
        Hrtf->next = NULL;
        return Hrtf;
    }

    free(azCount);
    free(evOffset);
    free(coeffs);
    free(delays);
    return NULL;
}


static struct Hrtf *LoadHrtf01(FILE *f, ALuint deviceRate)
{
    const ALubyte maxDelay = SRC_HISTORY_LENGTH-1;
    struct Hrtf *Hrtf = NULL;
    ALboolean failed = AL_FALSE;
    ALuint rate = 0, irCount = 0;
    ALubyte irSize = 0, evCount = 0;
    ALubyte *azCount = NULL;
    ALushort *evOffset = NULL;
    ALshort *coeffs = NULL;
    ALubyte *delays = NULL;
    ALuint i, j;

    rate  = fgetc(f);
    rate |= fgetc(f)<<8;
    rate |= fgetc(f)<<16;
    rate |= fgetc(f)<<24;

    irSize = fgetc(f);

    evCount = fgetc(f);

    if(rate != deviceRate)
    {
        ERR("HRIR rate does not match device rate: rate=%d (%d)\n",
                rate, deviceRate);
        failed = AL_TRUE;
    }
    if(irSize < MIN_IR_SIZE || irSize > MAX_IR_SIZE || (irSize%MOD_IR_SIZE))
    {
        ERR("Unsupported HRIR size: irSize=%d (%d to %d by %d)\n",
            irSize, MIN_IR_SIZE, MAX_IR_SIZE, MOD_IR_SIZE);
        failed = AL_TRUE;
    }
    if(evCount < MIN_EV_COUNT || evCount > MAX_EV_COUNT)
    {
        ERR("Unsupported elevation count: evCount=%d (%d to %d)\n",
            evCount, MIN_EV_COUNT, MAX_EV_COUNT);
        failed = AL_TRUE;
    }

    if(failed)
        return NULL;

    azCount = malloc(sizeof(azCount[0])*evCount);
    evOffset = malloc(sizeof(evOffset[0])*evCount);
    if(azCount == NULL || evOffset == NULL)
    {
        ERR("Out of memory.\n");
        failed = AL_TRUE;
    }

    if(!failed)
    {
        for(i = 0;i < evCount;i++)
        {
            azCount[i] = fgetc(f);
            if(azCount[i] < MIN_AZ_COUNT || azCount[i] > MAX_AZ_COUNT)
            {
                ERR("Unsupported azimuth count: azCount[%d]=%d (%d to %d)\n",
                    i, azCount[i], MIN_AZ_COUNT, MAX_AZ_COUNT);
                failed = AL_TRUE;
            }
        }
    }

    if(!failed)
    {
        evOffset[0] = 0;
        irCount = azCount[0];
        for(i = 1;i < evCount;i++)
        {
            evOffset[i] = evOffset[i-1] + azCount[i-1];
            irCount += azCount[i];
        }

        coeffs = malloc(sizeof(coeffs[0])*irSize*irCount);
        delays = malloc(sizeof(delays[0])*irCount);
        if(coeffs == NULL || delays == NULL)
        {
            ERR("Out of memory.\n");
            failed = AL_TRUE;
        }
    }

    if(!failed)
    {
        for(i = 0;i < irCount*irSize;i+=irSize)
        {
            for(j = 0;j < irSize;j++)
            {
                ALshort coeff;
                coeff  = fgetc(f);
                coeff |= fgetc(f)<<8;
                coeffs[i+j] = coeff;
            }
        }
        for(i = 0;i < irCount;i++)
        {
            delays[i] = fgetc(f);
            if(delays[i] > maxDelay)
            {
                ERR("Invalid delays[%d]: %d (%d)\n", i, delays[i], maxDelay);
                failed = AL_TRUE;
            }
        }

        if(feof(f))
        {
            ERR("Premature end of data\n");
            failed = AL_TRUE;
        }
    }

    if(!failed)
    {
        Hrtf = malloc(sizeof(struct Hrtf));
        if(Hrtf == NULL)
        {
            ERR("Out of memory.\n");
            failed = AL_TRUE;
        }
    }

    if(!failed)
    {
        Hrtf->sampleRate = rate;
        Hrtf->irSize = irSize;
        Hrtf->evCount = evCount;
        Hrtf->azCount = azCount;
        Hrtf->evOffset = evOffset;
        Hrtf->coeffs = coeffs;
        Hrtf->delays = delays;
        Hrtf->next = NULL;
        return Hrtf;
    }

    free(azCount);
    free(evOffset);
    free(coeffs);
    free(delays);
    return NULL;
}


static struct Hrtf *LoadHrtf(ALuint deviceRate)
{
    const char *fnamelist = NULL;

    if(!ConfigValueStr(NULL, "hrtf_tables", &fnamelist))
        return NULL;
    while(*fnamelist != '\0')
    {
        struct Hrtf *Hrtf = NULL;
        char fname[PATH_MAX];
        ALchar magic[8];
        ALuint i;
        FILE *f;

        while(isspace(*fnamelist) || *fnamelist == ',')
            fnamelist++;
        i = 0;
        while(*fnamelist != '\0' && *fnamelist != ',')
        {
            const char *next = strpbrk(fnamelist, "%,");
            while(fnamelist != next && *fnamelist && i < sizeof(fname))
                fname[i++] = *(fnamelist++);

            if(!next || *next == ',')
                break;

            /* *next == '%' */
            next++;
            if(*next == 'r')
            {
                int wrote = snprintf(&fname[i], sizeof(fname)-i, "%u", deviceRate);
                i += minu(wrote, sizeof(fname)-i);
                next++;
            }
            else if(*next == '%')
            {
                if(i < sizeof(fname))
                    fname[i++] = '%';
                next++;
            }
            else
                ERR("Invalid marker '%%%c'\n", *next);
            fnamelist = next;
        }
        i = minu(i, sizeof(fname)-1);
        fname[i] = '\0';
        while(i > 0 && isspace(fname[i-1]))
            i--;
        fname[i] = '\0';

        if(fname[0] == '\0')
            continue;

        TRACE("Loading %s...\n", fname);
        f = fopen(fname, "rb");
        if(f == NULL)
        {
            ERR("Could not open %s\n", fname);
            continue;
        }

        if(fread(magic, 1, sizeof(magic), f) != sizeof(magic))
            ERR("Failed to read header from %s\n", fname);
        else
        {
            if(memcmp(magic, magicMarker00, sizeof(magicMarker00)) == 0)
            {
                TRACE("Detected data set format v0\n");
                Hrtf = LoadHrtf00(f, deviceRate);
            }
            else if(memcmp(magic, magicMarker01, sizeof(magicMarker01)) == 0)
            {
                TRACE("Detected data set format v1\n");
                Hrtf = LoadHrtf01(f, deviceRate);
            }
            else
                ERR("Invalid header in %s: \"%.8s\"\n", fname, magic);
        }

        fclose(f);
        f = NULL;

        if(Hrtf)
        {
            Hrtf->next = LoadedHrtfs;
            LoadedHrtfs = Hrtf;
            TRACE("Loaded HRTF support for format: %s %uhz\n",
                  DevFmtChannelsString(DevFmtStereo), Hrtf->sampleRate);
            return Hrtf;
        }

        ERR("Failed to load %s\n", fname);
    }

    return NULL;
}

const struct Hrtf *GetHrtf(ALCdevice *device)
{
    if(device->FmtChans == DevFmtStereo)
    {
        struct Hrtf *Hrtf = LoadedHrtfs;
        while(Hrtf != NULL)
        {
            if(device->Frequency == Hrtf->sampleRate)
                return Hrtf;
            Hrtf = Hrtf->next;
        }

        Hrtf = LoadHrtf(device->Frequency);
        if(Hrtf != NULL)
            return Hrtf;

        if(device->Frequency == DefaultHrtf.sampleRate)
            return &DefaultHrtf;
    }
    ERR("Incompatible format: %s %uhz\n",
        DevFmtChannelsString(device->FmtChans), device->Frequency);
    return NULL;
}

void FindHrtfFormat(const ALCdevice *device, enum DevFmtChannels *chans, ALCuint *srate)
{
    const struct Hrtf *hrtf = &DefaultHrtf;

    if(device->Frequency != DefaultHrtf.sampleRate)
    {
        hrtf = LoadedHrtfs;
        while(hrtf != NULL)
        {
            if(device->Frequency == hrtf->sampleRate)
                break;
            hrtf = hrtf->next;
        }

        if(hrtf == NULL)
            hrtf = LoadHrtf(device->Frequency);
        if(hrtf == NULL)
            hrtf = &DefaultHrtf;
    }

    *chans = DevFmtStereo;
    *srate = hrtf->sampleRate;
}

void FreeHrtfs(void)
{
    struct Hrtf *Hrtf = NULL;

    while((Hrtf=LoadedHrtfs) != NULL)
    {
        LoadedHrtfs = Hrtf->next;
        free((void*)Hrtf->azCount);
        free((void*)Hrtf->evOffset);
        free((void*)Hrtf->coeffs);
        free((void*)Hrtf->delays);
        free(Hrtf);
    }
}

ALuint GetHrtfIrSize (const struct Hrtf *Hrtf)
{
    return Hrtf->irSize;
}
