#ifndef _AL_EFFECT_H_
#define _AL_EFFECT_H_

#include "alMain.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ALeffect;

enum {
    EAXREVERB = 0,
    REVERB,
    ECHO,
    MODULATOR,
    DEDICATED,
    CHORUS,
    FLANGER,
    EQUALIZER,
    DISTORTION,

    MAX_EFFECTS
};
extern ALboolean DisabledEffects[MAX_EFFECTS];

extern ALfloat ReverbBoost;
extern ALboolean EmulateEAXReverb;

struct ALeffectVtable {
    void (*const setParami)(struct ALeffect *effect, ALCcontext *context, ALenum param, ALint val);
    void (*const setParamiv)(struct ALeffect *effect, ALCcontext *context, ALenum param, const ALint *vals);
    void (*const setParamf)(struct ALeffect *effect, ALCcontext *context, ALenum param, ALfloat val);
    void (*const setParamfv)(struct ALeffect *effect, ALCcontext *context, ALenum param, const ALfloat *vals);

    void (*const getParami)(struct ALeffect *effect, ALCcontext *context, ALenum param, ALint *val);
    void (*const getParamiv)(struct ALeffect *effect, ALCcontext *context, ALenum param, ALint *vals);
    void (*const getParamf)(struct ALeffect *effect, ALCcontext *context, ALenum param, ALfloat *val);
    void (*const getParamfv)(struct ALeffect *effect, ALCcontext *context, ALenum param, ALfloat *vals);
};

#define DEFINE_ALEFFECT_VTABLE(T)           \
const struct ALeffectVtable T##_vtable = {  \
    T##_setParami, T##_setParamiv,          \
    T##_setParamf, T##_setParamfv,          \
    T##_getParami, T##_getParamiv,          \
    T##_getParamf, T##_getParamfv,          \
}

extern const struct ALeffectVtable ALeaxreverb_vtable;
extern const struct ALeffectVtable ALreverb_vtable;
extern const struct ALeffectVtable ALchorus_vtable;
extern const struct ALeffectVtable ALdistortion_vtable;
extern const struct ALeffectVtable ALecho_vtable;
extern const struct ALeffectVtable ALequalizer_vtable;
extern const struct ALeffectVtable ALflanger_vtable;
extern const struct ALeffectVtable ALmodulator_vtable;
extern const struct ALeffectVtable ALnull_vtable;
extern const struct ALeffectVtable ALdedicated_vtable;


typedef union ALeffectProps {
    struct {
        // Shared Reverb Properties
        ALfloat Density;
        ALfloat Diffusion;
        ALfloat Gain;
        ALfloat GainHF;
        ALfloat DecayTime;
        ALfloat DecayHFRatio;
        ALfloat ReflectionsGain;
        ALfloat ReflectionsDelay;
        ALfloat LateReverbGain;
        ALfloat LateReverbDelay;
        ALfloat AirAbsorptionGainHF;
        ALfloat RoomRolloffFactor;
        ALboolean DecayHFLimit;

        // Additional EAX Reverb Properties
        ALfloat GainLF;
        ALfloat DecayLFRatio;
        ALfloat ReflectionsPan[3];
        ALfloat LateReverbPan[3];
        ALfloat EchoTime;
        ALfloat EchoDepth;
        ALfloat ModulationTime;
        ALfloat ModulationDepth;
        ALfloat HFReference;
        ALfloat LFReference;
    } Reverb;

    struct {
        ALfloat Delay;
        ALfloat LRDelay;

        ALfloat Damping;
        ALfloat Feedback;

        ALfloat Spread;
    } Echo;

    struct {
        ALfloat Frequency;
        ALfloat HighPassCutoff;
        ALint Waveform;
    } Modulator;

    struct {
        ALfloat Gain;
    } Dedicated;

    struct {
        ALint Waveform;
        ALint Phase;
        ALfloat Rate;
        ALfloat Depth;
        ALfloat Feedback;
        ALfloat Delay;
    } Chorus;

    struct {
        ALint Waveform;
        ALint Phase;
        ALfloat Rate;
        ALfloat Depth;
        ALfloat Feedback;
        ALfloat Delay;
    } Flanger;

    struct {
        ALfloat Delay;
        ALfloat LowCutoff;
        ALfloat LowGain;
        ALfloat Mid1Center;
        ALfloat Mid1Gain;
        ALfloat Mid1Width;
        ALfloat Mid2Center;
        ALfloat Mid2Gain;
        ALfloat Mid2Width;
        ALfloat HighCutoff;
        ALfloat HighGain;
    } Equalizer;

    struct {
        ALfloat Edge;
        ALfloat Gain;
        ALfloat LowpassCutoff;
        ALfloat EQCenter;
        ALfloat EQBandwidth;
    } Distortion;
} ALeffectProps;

typedef struct ALeffect {
    // Effect type (AL_EFFECT_NULL, ...)
    ALenum type;

    ALeffectProps Props;

    const struct ALeffectVtable *vtbl;

    /* Self ID */
    ALuint id;
} ALeffect;

static inline ALboolean IsReverbEffect(ALenum type)
{ return type == AL_EFFECT_REVERB || type == AL_EFFECT_EAXREVERB; }

ALenum InitEffect(ALeffect *effect);
ALvoid ReleaseALEffects(ALCdevice *device);

ALvoid LoadReverbPreset(const char *name, ALeffect *effect);

#ifdef __cplusplus
}
#endif

#endif
