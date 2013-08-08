#ifndef _AL_FILTER_H_
#define _AL_FILTER_H_

#include "alMain.h"

#ifdef __cplusplus
extern "C" {
#endif

#define LOWPASSFREQREF  (5000)


/* Filters implementation is based on the "Cookbook formulae for audio   *
 * EQ biquad filter coefficients" by Robert Bristow-Johnson              *
 * http://www.musicdsp.org/files/Audio-EQ-Cookbook.txt                   */

typedef enum ALfilterType {
    ALfilterType_HighShelf,
    ALfilterType_LowShelf,
    ALfilterType_Peaking,

    ALfilterType_LowPass,
    ALfilterType_BandPass,
} ALfilterType;

typedef struct ALfilterState {
    ALfloat x[2]; /* History of two last input samples  */
    ALfloat y[2]; /* History of two last output samples */
    ALfloat a[3]; /* Transfer function coefficients "a" */
    ALfloat b[3]; /* Transfer function coefficients "b" */
} ALfilterState;

void ALfilterState_clear(ALfilterState *filter);
void ALfilterState_setParams(ALfilterState *filter, ALfilterType type, ALfloat gain, ALfloat freq_scale, ALfloat bandwidth);

static inline ALfloat ALfilterState_processSingle(ALfilterState *filter, ALfloat sample)
{
    ALfloat outsmp;

    outsmp = filter->b[0] * sample +
             filter->b[1] * filter->x[0] +
             filter->b[2] * filter->x[1] -
             filter->a[1] * filter->y[0] -
             filter->a[2] * filter->y[1];
    filter->x[1] = filter->x[0];
    filter->x[0] = sample;
    filter->y[1] = filter->y[0];
    filter->y[0] = outsmp;

    return outsmp;
}

static inline ALfloat ALfilterState_processSingleC(const ALfilterState *filter, ALfloat sample)
{
    return filter->b[0] * sample +
           filter->b[1] * filter->x[0] +
           filter->b[2] * filter->x[1] -
           filter->a[1] * filter->y[0] -
           filter->a[2] * filter->y[1];
}


typedef struct ALfilter {
    // Filter type (AL_FILTER_NULL, ...)
    ALenum type;

    ALfloat Gain;
    ALfloat GainHF;

    void (*SetParami)(struct ALfilter *filter, ALCcontext *context, ALenum param, ALint val);
    void (*SetParamiv)(struct ALfilter *filter, ALCcontext *context, ALenum param, const ALint *vals);
    void (*SetParamf)(struct ALfilter *filter, ALCcontext *context, ALenum param, ALfloat val);
    void (*SetParamfv)(struct ALfilter *filter, ALCcontext *context, ALenum param, const ALfloat *vals);

    void (*GetParami)(struct ALfilter *filter, ALCcontext *context, ALenum param, ALint *val);
    void (*GetParamiv)(struct ALfilter *filter, ALCcontext *context, ALenum param, ALint *vals);
    void (*GetParamf)(struct ALfilter *filter, ALCcontext *context, ALenum param, ALfloat *val);
    void (*GetParamfv)(struct ALfilter *filter, ALCcontext *context, ALenum param, ALfloat *vals);

    /* Self ID */
    ALuint id;
} ALfilter;

#define ALfilter_SetParami(x, c, p, v)  ((x)->SetParami((x),(c),(p),(v)))
#define ALfilter_SetParamiv(x, c, p, v) ((x)->SetParamiv((x),(c),(p),(v)))
#define ALfilter_SetParamf(x, c, p, v)  ((x)->SetParamf((x),(c),(p),(v)))
#define ALfilter_SetParamfv(x, c, p, v) ((x)->SetParamfv((x),(c),(p),(v)))

#define ALfilter_GetParami(x, c, p, v)  ((x)->GetParami((x),(c),(p),(v)))
#define ALfilter_GetParamiv(x, c, p, v) ((x)->GetParamiv((x),(c),(p),(v)))
#define ALfilter_GetParamf(x, c, p, v)  ((x)->GetParamf((x),(c),(p),(v)))
#define ALfilter_GetParamfv(x, c, p, v) ((x)->GetParamfv((x),(c),(p),(v)))

ALvoid ReleaseALFilters(ALCdevice *device);

#ifdef __cplusplus
}
#endif

#endif
