#ifndef _ALU_H_
#define _ALU_H_

#include "alMain.h"

#include <limits.h>
#include <math.h>
#ifdef HAVE_FLOAT_H
#include <float.h>
#endif
#ifdef HAVE_IEEEFP_H
#include <ieeefp.h>
#endif


#define F_PI    (3.14159265358979323846f)  /* pi */
#define F_PI_2  (1.57079632679489661923f)  /* pi/2 */

#ifndef FLT_EPSILON
#define FLT_EPSILON (1.19209290e-07f)
#endif


#ifdef __cplusplus
extern "C" {
#endif

struct ALsource;
struct ALbuffer;
struct DirectParams;
struct SendParams;

typedef void (*ResamplerFunc)(const ALfloat *src, ALuint frac, ALuint increment,
                              ALfloat *restrict dst, ALuint dstlen);

typedef ALvoid (*DryMixerFunc)(const struct DirectParams *params,
                               const ALfloat *restrict data, ALuint srcchan,
                               ALuint OutPos, ALuint SamplesToDo,
                               ALuint BufferSize);
typedef ALvoid (*WetMixerFunc)(const struct SendParams *params,
                               const ALfloat *restrict data,
                               ALuint OutPos, ALuint SamplesToDo,
                               ALuint BufferSize);


#define SPEEDOFSOUNDMETRESPERSEC  (343.3f)
#define AIRABSORBGAINHF           (0.99426f) /* -0.05dB */

#define FRACTIONBITS (14)
#define FRACTIONONE  (1<<FRACTIONBITS)
#define FRACTIONMASK (FRACTIONONE-1)


static inline ALfloat minf(ALfloat a, ALfloat b)
{ return ((a > b) ? b : a); }
static inline ALfloat maxf(ALfloat a, ALfloat b)
{ return ((a > b) ? a : b); }
static inline ALfloat clampf(ALfloat val, ALfloat min, ALfloat max)
{ return minf(max, maxf(min, val)); }

static inline ALuint minu(ALuint a, ALuint b)
{ return ((a > b) ? b : a); }
static inline ALuint maxu(ALuint a, ALuint b)
{ return ((a > b) ? a : b); }
static inline ALuint clampu(ALuint val, ALuint min, ALuint max)
{ return minu(max, maxu(min, val)); }

static inline ALint mini(ALint a, ALint b)
{ return ((a > b) ? b : a); }
static inline ALint maxi(ALint a, ALint b)
{ return ((a > b) ? a : b); }
static inline ALint clampi(ALint val, ALint min, ALint max)
{ return mini(max, maxi(min, val)); }

static inline ALint64 mini64(ALint64 a, ALint64 b)
{ return ((a > b) ? b : a); }
static inline ALint64 maxi64(ALint64 a, ALint64 b)
{ return ((a > b) ? a : b); }
static inline ALint64 clampi64(ALint64 val, ALint64 min, ALint64 max)
{ return mini64(max, maxi64(min, val)); }

static inline ALuint64 minu64(ALuint64 a, ALuint64 b)
{ return ((a > b) ? b : a); }
static inline ALuint64 maxu64(ALuint64 a, ALuint64 b)
{ return ((a > b) ? a : b); }
static inline ALuint64 clampu64(ALuint64 val, ALuint64 min, ALuint64 max)
{ return minu64(max, maxu64(min, val)); }


static inline ALfloat lerp(ALfloat val1, ALfloat val2, ALfloat mu)
{
    return val1 + (val2-val1)*mu;
}
static inline ALfloat cubic(ALfloat val0, ALfloat val1, ALfloat val2, ALfloat val3, ALfloat mu)
{
    ALfloat mu2 = mu*mu;
    ALfloat a0 = -0.5f*val0 +  1.5f*val1 + -1.5f*val2 +  0.5f*val3;
    ALfloat a1 =       val0 + -2.5f*val1 +  2.0f*val2 + -0.5f*val3;
    ALfloat a2 = -0.5f*val0              +  0.5f*val2;
    ALfloat a3 =                    val1;

    return a0*mu*mu2 + a1*mu2 + a2*mu + a3;
}


ALvoid aluInitPanning(ALCdevice *Device);

ALvoid ComputeAngleGains(const ALCdevice *device, ALfloat angle, ALfloat hwidth, ALfloat ingain, ALfloat *gains);

ALvoid CalcSourceParams(struct ALsource *ALSource, const ALCcontext *ALContext);
ALvoid CalcNonAttnSourceParams(struct ALsource *ALSource, const ALCcontext *ALContext);

ALvoid MixSource(struct ALsource *Source, ALCdevice *Device, ALuint SamplesToDo);

ALvoid aluMixData(ALCdevice *device, ALvoid *buffer, ALsizei size);
/* Caller must lock the device. */
ALvoid aluHandleDisconnect(ALCdevice *device);

extern ALfloat ConeScale;
extern ALfloat ZScale;

#ifdef __cplusplus
}
#endif

#endif

