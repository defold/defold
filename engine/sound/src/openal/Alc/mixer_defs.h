#ifndef MIXER_DEFS_H
#define MIXER_DEFS_H

#include "AL/alc.h"
#include "AL/al.h"
#include "alMain.h"

struct DirectParams;
struct SendParams;

/* C resamplers */
void Resample_copy32_C(const ALfloat *src, ALuint frac, ALuint increment, ALfloat *restrict dst, ALuint dstlen);
void Resample_point32_C(const ALfloat *src, ALuint frac, ALuint increment, ALfloat *restrict dst, ALuint dstlen);
void Resample_lerp32_C(const ALfloat *src, ALuint frac, ALuint increment, ALfloat *restrict dst, ALuint dstlen);
void Resample_cubic32_C(const ALfloat *src, ALuint frac, ALuint increment, ALfloat *restrict dst, ALuint dstlen);


/* C mixers */
void MixDirect_Hrtf_C(const struct DirectParams*,const ALfloat*restrict,ALuint,ALuint,ALuint,ALuint);
void MixDirect_C(const struct DirectParams*,const ALfloat*restrict,ALuint,ALuint,ALuint,ALuint);
void MixSend_C(const struct SendParams*,const ALfloat*restrict,ALuint,ALuint,ALuint);

/* SSE mixers */
void MixDirect_Hrtf_SSE(const struct DirectParams*,const ALfloat*restrict,ALuint,ALuint,ALuint,ALuint);
void MixDirect_SSE(const struct DirectParams*,const ALfloat*restrict,ALuint,ALuint,ALuint,ALuint);
void MixSend_SSE(const struct SendParams*,const ALfloat*restrict,ALuint,ALuint,ALuint);

/* Neon mixers */
void MixDirect_Hrtf_Neon(const struct DirectParams*,const ALfloat*restrict,ALuint,ALuint,ALuint,ALuint);

#endif /* MIXER_DEFS_H */
