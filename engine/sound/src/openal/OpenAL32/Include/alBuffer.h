#ifndef _AL_BUFFER_H_
#define _AL_BUFFER_H_

#include "alMain.h"

#ifdef __cplusplus
extern "C" {
#endif

/* User formats */
enum UserFmtType {
    UserFmtByte   = AL_BYTE_SOFT,
    UserFmtUByte  = AL_UNSIGNED_BYTE_SOFT,
    UserFmtShort  = AL_SHORT_SOFT,
    UserFmtUShort = AL_UNSIGNED_SHORT_SOFT,
    UserFmtInt    = AL_INT_SOFT,
    UserFmtUInt   = AL_UNSIGNED_INT_SOFT,
    UserFmtFloat  = AL_FLOAT_SOFT,
    UserFmtDouble = AL_DOUBLE_SOFT,
    UserFmtByte3  = AL_BYTE3_SOFT,
    UserFmtUByte3 = AL_UNSIGNED_BYTE3_SOFT,
    UserFmtMulaw,
    UserFmtAlaw,
    UserFmtIMA4,
};
enum UserFmtChannels {
    UserFmtMono   = AL_MONO_SOFT,
    UserFmtStereo = AL_STEREO_SOFT,
    UserFmtRear   = AL_REAR_SOFT,
    UserFmtQuad   = AL_QUAD_SOFT,
    UserFmtX51    = AL_5POINT1_SOFT, /* (WFX order) */
    UserFmtX61    = AL_6POINT1_SOFT, /* (WFX order) */
    UserFmtX71    = AL_7POINT1_SOFT, /* (WFX order) */
};

ALuint BytesFromUserFmt(enum UserFmtType type);
ALuint ChannelsFromUserFmt(enum UserFmtChannels chans);
static inline ALuint FrameSizeFromUserFmt(enum UserFmtChannels chans,
                                          enum UserFmtType type)
{
    return ChannelsFromUserFmt(chans) * BytesFromUserFmt(type);
}


/* Storable formats */
enum FmtType {
    FmtByte  = UserFmtByte,
    FmtShort = UserFmtShort,
    FmtFloat = UserFmtFloat,
};
enum FmtChannels {
    FmtMono   = UserFmtMono,
    FmtStereo = UserFmtStereo,
    FmtRear   = UserFmtRear,
    FmtQuad   = UserFmtQuad,
    FmtX51    = UserFmtX51,
    FmtX61    = UserFmtX61,
    FmtX71    = UserFmtX71,
};

ALuint BytesFromFmt(enum FmtType type);
ALuint ChannelsFromFmt(enum FmtChannels chans);
static inline ALuint FrameSizeFromFmt(enum FmtChannels chans, enum FmtType type)
{
    return ChannelsFromFmt(chans) * BytesFromFmt(type);
}


typedef struct ALbuffer
{
    ALvoid  *data;

    ALsizei  Frequency;
    ALenum   Format;
    ALsizei  SampleLen;

    enum FmtChannels FmtChannels;
    enum FmtType     FmtType;

    enum UserFmtChannels OriginalChannels;
    enum UserFmtType     OriginalType;
    ALsizei              OriginalSize;

    ALsizei  LoopStart;
    ALsizei  LoopEnd;

    /* Number of times buffer was attached to a source (deletion can only occur when 0) */
    RefCount ref;

    RWLock lock;

    /* Self ID */
    ALuint id;
} ALbuffer;

ALvoid ReleaseALBuffers(ALCdevice *device);

#ifdef __cplusplus
}
#endif

#endif
