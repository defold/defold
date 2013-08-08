#ifndef AL_MAIN_H
#define AL_MAIN_H

#include <string.h>
#include <stdio.h>
#include <stdarg.h>
#include <assert.h>
#include <math.h>

#ifdef HAVE_FENV_H
#include <fenv.h>
#endif

#include "AL/al.h"
#include "AL/alc.h"
#include "AL/alext.h"

#ifndef AL_SOFT_deferred_updates
#define AL_SOFT_deferred_updates 1
#define AL_DEFERRED_UPDATES_SOFT                 0xC002
typedef ALvoid (AL_APIENTRY*LPALDEFERUPDATESSOFT)(void);
typedef ALvoid (AL_APIENTRY*LPALPROCESSUPDATESSOFT)(void);
#ifdef AL_ALEXT_PROTOTYPES
AL_API ALvoid AL_APIENTRY alDeferUpdatesSOFT(void);
AL_API ALvoid AL_APIENTRY alProcessUpdatesSOFT(void);
#endif
#endif

#ifndef ALC_SOFT_HRTF
#define ALC_SOFT_HRTF 1
#define ALC_HRTF_SOFT                            0x1992
#endif


#if defined(HAVE_STDINT_H)
#include <stdint.h>
typedef int64_t ALint64;
typedef uint64_t ALuint64;
#elif defined(HAVE___INT64)
typedef __int64 ALint64;
typedef unsigned __int64 ALuint64;
#elif (SIZEOF_LONG == 8)
typedef long ALint64;
typedef unsigned long ALuint64;
#elif (SIZEOF_LONG_LONG == 8)
typedef long long ALint64;
typedef unsigned long long ALuint64;
#endif

typedef ptrdiff_t ALintptrEXT;
typedef ptrdiff_t ALsizeiptrEXT;

#define MAKEU64(x,y) (((ALuint64)(x)<<32)|(ALuint64)(y))

#ifdef HAVE_GCC_FORMAT
#define PRINTF_STYLE(x, y) __attribute__((format(printf, (x), (y))))
#else
#define PRINTF_STYLE(x, y)
#endif


static const union {
    ALuint u;
    ALubyte b[sizeof(ALuint)];
} EndianTest = { 1 };
#define IS_LITTLE_ENDIAN (EndianTest.b[0] == 1)

#define COUNTOF(x) (sizeof((x))/sizeof((x)[0]))


#define DERIVE_FROM_TYPE(t)          t t##_parent
#define STATIC_CAST(to, obj)         (&(obj)->to##_parent)
#define STATIC_UPCAST(to, from, obj) ((to*)((char*)(obj) - offsetof(to, from##_parent)))

#define SET_VTABLE1(T1, obj)     ((obj)->vtbl = &(T1##_vtable))
#define SET_VTABLE2(T1, T2, obj) SET_VTABLE1(T1##_##T2, STATIC_CAST(T2, (obj)))

/* Helper to extract an argument list for VCALL. Not used directly. */
#define EXTRACT_VCALL_ARGS(...)  __VA_ARGS__

/* Call a "virtual" method on an object, with arguments. */
#define VCALL(obj, func, args)  ((obj)->vtbl->func((obj), EXTRACT_VCALL_ARGS args))
/* Call a "virtual" method on an object, with no arguments. */
#define VCALL_NOARGS(obj, func) ((obj)->vtbl->func((obj)))

#define DELETE_OBJ(obj) do {                                                  \
    if((obj) != NULL)                                                         \
    {                                                                         \
        VCALL_NOARGS((obj),Destruct);                                         \
        VCALL_NOARGS((obj),Delete);                                           \
    }                                                                         \
} while(0)


#ifdef _WIN32

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

typedef DWORD pthread_key_t;
int pthread_key_create(pthread_key_t *key, void (*callback)(void*));
int pthread_key_delete(pthread_key_t key);
void *pthread_getspecific(pthread_key_t key);
int pthread_setspecific(pthread_key_t key, void *val);

#define HAVE_DYNLOAD 1
void *LoadLib(const char *name);
void CloseLib(void *handle);
void *GetSymbol(void *handle, const char *name);

WCHAR *strdupW(const WCHAR *str);

typedef LONG pthread_once_t;
#define PTHREAD_ONCE_INIT 0
void pthread_once(pthread_once_t *once, void (*callback)(void));

static inline int sched_yield(void)
{ SwitchToThread(); return 0; }

#else

#include <unistd.h>
#include <assert.h>
#include <pthread.h>
#include <sys/time.h>
#include <time.h>
#include <errno.h>

typedef pthread_mutex_t CRITICAL_SECTION;
void InitializeCriticalSection(CRITICAL_SECTION *cs);
void DeleteCriticalSection(CRITICAL_SECTION *cs);
void EnterCriticalSection(CRITICAL_SECTION *cs);
void LeaveCriticalSection(CRITICAL_SECTION *cs);

ALuint timeGetTime(void);
void Sleep(ALuint t);

#if defined(HAVE_DLFCN_H)
#define HAVE_DYNLOAD 1
void *LoadLib(const char *name);
void CloseLib(void *handle);
void *GetSymbol(void *handle, const char *name);
#endif

#endif

typedef void *volatile XchgPtr;

#if defined(__GNUC__) && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 1)) && !defined(__QNXNTO__)
typedef ALuint RefCount;
static inline RefCount IncrementRef(volatile RefCount *ptr)
{ return __sync_add_and_fetch(ptr, 1); }
static inline RefCount DecrementRef(volatile RefCount *ptr)
{ return __sync_sub_and_fetch(ptr, 1); }

static inline int ExchangeInt(volatile int *ptr, int newval)
{
    return __sync_lock_test_and_set(ptr, newval);
}
static inline void *ExchangePtr(XchgPtr *ptr, void *newval)
{
    return __sync_lock_test_and_set(ptr, newval);
}
static inline ALboolean CompExchangeInt(volatile int *ptr, int oldval, int newval)
{
    return __sync_bool_compare_and_swap(ptr, oldval, newval);
}
static inline ALboolean CompExchangePtr(XchgPtr *ptr, void *oldval, void *newval)
{
    return __sync_bool_compare_and_swap(ptr, oldval, newval);
}

#elif defined(__GNUC__) && (defined(__i386__) || defined(__x86_64__))

static inline int xaddl(volatile int *dest, int incr)
{
    int ret;
    __asm__ __volatile__("lock; xaddl %0,(%1)"
                         : "=r" (ret)
                         : "r" (dest), "0" (incr)
                         : "memory");
    return ret;
}

typedef int RefCount;
static inline RefCount IncrementRef(volatile RefCount *ptr)
{ return xaddl(ptr, 1)+1; }
static inline RefCount DecrementRef(volatile RefCount *ptr)
{ return xaddl(ptr, -1)-1; }

static inline int ExchangeInt(volatile int *dest, int newval)
{
    int ret;
    __asm__ __volatile__("lock; xchgl %0,(%1)"
                         : "=r" (ret)
                         : "r" (dest), "0" (newval)
                         : "memory");
    return ret;
}

static inline ALboolean CompExchangeInt(volatile int *dest, int oldval, int newval)
{
    int ret;
    __asm__ __volatile__("lock; cmpxchgl %2,(%1)"
                         : "=a" (ret)
                         : "r" (dest), "r" (newval), "0" (oldval)
                         : "memory");
    return ret == oldval;
}

static inline void *ExchangePtr(XchgPtr *dest, void *newval)
{
    void *ret;
    __asm__ __volatile__(
#ifdef __i386__
                         "lock; xchgl %0,(%1)"
#else
                         "lock; xchgq %0,(%1)"
#endif
                         : "=r" (ret)
                         : "r" (dest), "0" (newval)
                         : "memory"
    );
    return ret;
}

static inline ALboolean CompExchangePtr(XchgPtr *dest, void *oldval, void *newval)
{
    void *ret;
    __asm__ __volatile__(
#ifdef __i386__
                         "lock; cmpxchgl %2,(%1)"
#else
                         "lock; cmpxchgq %2,(%1)"
#endif
                         : "=a" (ret)
                         : "r" (dest), "r" (newval), "0" (oldval)
                         : "memory"
    );
    return ret == oldval;
}

#elif defined(_WIN32)

typedef LONG RefCount;
static inline RefCount IncrementRef(volatile RefCount *ptr)
{ return InterlockedIncrement(ptr); }
static inline RefCount DecrementRef(volatile RefCount *ptr)
{ return InterlockedDecrement(ptr); }

extern ALbyte LONG_size_does_not_match_int[(sizeof(LONG)==sizeof(int))?1:-1];

static inline int ExchangeInt(volatile int *ptr, int newval)
{
    union {
        volatile int *i;
        volatile LONG *l;
    } u = { ptr };
    return InterlockedExchange(u.l, newval);
}
static inline void *ExchangePtr(XchgPtr *ptr, void *newval)
{
    return InterlockedExchangePointer(ptr, newval);
}
static inline ALboolean CompExchangeInt(volatile int *ptr, int oldval, int newval)
{
    union {
        volatile int *i;
        volatile LONG *l;
    } u = { ptr };
    return InterlockedCompareExchange(u.l, newval, oldval) == oldval;
}
static inline ALboolean CompExchangePtr(XchgPtr *ptr, void *oldval, void *newval)
{
    return InterlockedCompareExchangePointer(ptr, newval, oldval) == oldval;
}

#elif defined(__APPLE__)

#include <libkern/OSAtomic.h>

typedef int32_t RefCount;
static inline RefCount IncrementRef(volatile RefCount *ptr)
{ return OSAtomicIncrement32Barrier(ptr); }
static inline RefCount DecrementRef(volatile RefCount *ptr)
{ return OSAtomicDecrement32Barrier(ptr); }

static inline int ExchangeInt(volatile int *ptr, int newval)
{
    /* Really? No regular old atomic swap? */
    int oldval;
    do {
        oldval = *ptr;
    } while(!OSAtomicCompareAndSwap32Barrier(oldval, newval, ptr));
    return oldval;
}
static inline void *ExchangePtr(XchgPtr *ptr, void *newval)
{
    void *oldval;
    do {
        oldval = *ptr;
    } while(!OSAtomicCompareAndSwapPtrBarrier(oldval, newval, ptr));
    return oldval;
}
static inline ALboolean CompExchangeInt(volatile int *ptr, int oldval, int newval)
{
    return OSAtomicCompareAndSwap32Barrier(oldval, newval, ptr);
}
static inline ALboolean CompExchangePtr(XchgPtr *ptr, void *oldval, void *newval)
{
    return OSAtomicCompareAndSwapPtrBarrier(oldval, newval, ptr);
}

#else
#error "No atomic functions available on this platform!"
typedef ALuint RefCount;
#endif


typedef struct {
    volatile RefCount read_count;
    volatile RefCount write_count;
    volatile ALenum read_lock;
    volatile ALenum read_entry_lock;
    volatile ALenum write_lock;
} RWLock;

void RWLockInit(RWLock *lock);
void ReadLock(RWLock *lock);
void ReadUnlock(RWLock *lock);
void WriteLock(RWLock *lock);
void WriteUnlock(RWLock *lock);


typedef struct UIntMap {
    struct {
        ALuint key;
        ALvoid *value;
    } *array;
    ALsizei size;
    ALsizei maxsize;
    ALsizei limit;
    RWLock lock;
} UIntMap;
extern UIntMap TlsDestructor;

void InitUIntMap(UIntMap *map, ALsizei limit);
void ResetUIntMap(UIntMap *map);
ALenum InsertUIntMapEntry(UIntMap *map, ALuint key, ALvoid *value);
ALvoid *RemoveUIntMapKey(UIntMap *map, ALuint key);
ALvoid *LookupUIntMapKey(UIntMap *map, ALuint key);

static inline void LockUIntMapRead(UIntMap *map)
{ ReadLock(&map->lock); }
static inline void UnlockUIntMapRead(UIntMap *map)
{ ReadUnlock(&map->lock); }
static inline void LockUIntMapWrite(UIntMap *map)
{ WriteLock(&map->lock); }
static inline void UnlockUIntMapWrite(UIntMap *map)
{ WriteUnlock(&map->lock); }


#ifdef __cplusplus
extern "C" {
#endif

struct Hrtf;


#define DEFAULT_OUTPUT_RATE  (44100)
#define MIN_OUTPUT_RATE      (8000)


// Find the next power-of-2 for non-power-of-2 numbers.
static inline ALuint NextPowerOf2(ALuint value)
{
    if(value > 0)
    {
        value--;
        value |= value>>1;
        value |= value>>2;
        value |= value>>4;
        value |= value>>8;
        value |= value>>16;
    }
    return value+1;
}

/* Fast float-to-int conversion. Assumes the FPU is already in round-to-zero
 * mode. */
static inline ALint fastf2i(ALfloat f)
{
#ifdef HAVE_LRINTF
    return lrintf(f);
#elif defined(_MSC_VER) && defined(_M_IX86)
    ALint i;
    __asm fld f
    __asm fistp i
    return i;
#else
    return (ALint)f;
#endif
}

/* Fast float-to-uint conversion. Assumes the FPU is already in round-to-zero
 * mode. */
static inline ALuint fastf2u(ALfloat f)
{ return fastf2i(f); }


enum DevProbe {
    ALL_DEVICE_PROBE,
    CAPTURE_DEVICE_PROBE
};

typedef struct {
    ALCenum (*OpenPlayback)(ALCdevice*, const ALCchar*);
    void (*ClosePlayback)(ALCdevice*);
    ALCboolean (*ResetPlayback)(ALCdevice*);
    ALCboolean (*StartPlayback)(ALCdevice*);
    void (*StopPlayback)(ALCdevice*);

    ALCenum (*OpenCapture)(ALCdevice*, const ALCchar*);
    void (*CloseCapture)(ALCdevice*);
    void (*StartCapture)(ALCdevice*);
    void (*StopCapture)(ALCdevice*);
    ALCenum (*CaptureSamples)(ALCdevice*, void*, ALCuint);
    ALCuint (*AvailableSamples)(ALCdevice*);

    void (*Lock)(ALCdevice*);
    void (*Unlock)(ALCdevice*);

    ALint64 (*GetLatency)(ALCdevice*);
} BackendFuncs;

struct BackendInfo {
    const char *name;
    ALCboolean (*Init)(BackendFuncs*);
    void (*Deinit)(void);
    void (*Probe)(enum DevProbe);
    BackendFuncs Funcs;
};

ALCboolean alc_alsa_init(BackendFuncs *func_list);
void alc_alsa_deinit(void);
void alc_alsa_probe(enum DevProbe type);
ALCboolean alc_oss_init(BackendFuncs *func_list);
void alc_oss_deinit(void);
void alc_oss_probe(enum DevProbe type);
ALCboolean alc_solaris_init(BackendFuncs *func_list);
void alc_solaris_deinit(void);
void alc_solaris_probe(enum DevProbe type);
ALCboolean alc_sndio_init(BackendFuncs *func_list);
void alc_sndio_deinit(void);
void alc_sndio_probe(enum DevProbe type);
ALCboolean alcMMDevApiInit(BackendFuncs *func_list);
void alcMMDevApiDeinit(void);
void alcMMDevApiProbe(enum DevProbe type);
ALCboolean alcDSoundInit(BackendFuncs *func_list);
void alcDSoundDeinit(void);
void alcDSoundProbe(enum DevProbe type);
ALCboolean alcWinMMInit(BackendFuncs *FuncList);
void alcWinMMDeinit(void);
void alcWinMMProbe(enum DevProbe type);
ALCboolean alc_pa_init(BackendFuncs *func_list);
void alc_pa_deinit(void);
void alc_pa_probe(enum DevProbe type);
ALCboolean alc_wave_init(BackendFuncs *func_list);
void alc_wave_deinit(void);
void alc_wave_probe(enum DevProbe type);
ALCboolean alc_pulse_init(BackendFuncs *func_list);
void alc_pulse_deinit(void);
void alc_pulse_probe(enum DevProbe type);
ALCboolean alc_ca_init(BackendFuncs *func_list);
void alc_ca_deinit(void);
void alc_ca_probe(enum DevProbe type);
ALCboolean alc_opensl_init(BackendFuncs *func_list);
void alc_opensl_deinit(void);
void alc_opensl_probe(enum DevProbe type);
ALCboolean alc_qsa_init(BackendFuncs *func_list);
void alc_qsa_deinit(void);
void alc_qsa_probe(enum DevProbe type);
ALCboolean alc_null_init(BackendFuncs *func_list);
void alc_null_deinit(void);
void alc_null_probe(enum DevProbe type);
ALCboolean alc_loopback_init(BackendFuncs *func_list);
void alc_loopback_deinit(void);
void alc_loopback_probe(enum DevProbe type);


enum DistanceModel {
    InverseDistanceClamped  = AL_INVERSE_DISTANCE_CLAMPED,
    LinearDistanceClamped   = AL_LINEAR_DISTANCE_CLAMPED,
    ExponentDistanceClamped = AL_EXPONENT_DISTANCE_CLAMPED,
    InverseDistance  = AL_INVERSE_DISTANCE,
    LinearDistance   = AL_LINEAR_DISTANCE,
    ExponentDistance = AL_EXPONENT_DISTANCE,
    DisableDistance  = AL_NONE,

    DefaultDistanceModel = InverseDistanceClamped
};

enum Resampler {
    PointResampler,
    LinearResampler,
    CubicResampler,

    ResamplerMax,
};

enum Channel {
    FrontLeft = 0,
    FrontRight,
    FrontCenter,
    LFE,
    BackLeft,
    BackRight,
    BackCenter,
    SideLeft,
    SideRight,

    MaxChannels,
};


/* Device formats */
enum DevFmtType {
    DevFmtByte   = ALC_BYTE_SOFT,
    DevFmtUByte  = ALC_UNSIGNED_BYTE_SOFT,
    DevFmtShort  = ALC_SHORT_SOFT,
    DevFmtUShort = ALC_UNSIGNED_SHORT_SOFT,
    DevFmtInt    = ALC_INT_SOFT,
    DevFmtUInt   = ALC_UNSIGNED_INT_SOFT,
    DevFmtFloat  = ALC_FLOAT_SOFT,

    DevFmtTypeDefault = DevFmtFloat
};
enum DevFmtChannels {
    DevFmtMono   = ALC_MONO_SOFT,
    DevFmtStereo = ALC_STEREO_SOFT,
    DevFmtQuad   = ALC_QUAD_SOFT,
    DevFmtX51    = ALC_5POINT1_SOFT,
    DevFmtX61    = ALC_6POINT1_SOFT,
    DevFmtX71    = ALC_7POINT1_SOFT,

    /* Similar to 5.1, except using the side channels instead of back */
    DevFmtX51Side = 0x80000000,

    DevFmtChannelsDefault = DevFmtStereo
};

ALuint BytesFromDevFmt(enum DevFmtType type);
ALuint ChannelsFromDevFmt(enum DevFmtChannels chans);
static inline ALuint FrameSizeFromDevFmt(enum DevFmtChannels chans,
                                           enum DevFmtType type)
{
    return ChannelsFromDevFmt(chans) * BytesFromDevFmt(type);
}


extern const struct EffectList {
    const char *name;
    int type;
    const char *ename;
    ALenum val;
} EffectList[];


enum DeviceType {
    Playback,
    Capture,
    Loopback
};


/* Size for temporary storage of buffer data, in ALfloats. Larger values need
 * more memory, while smaller values may need more iterations. The value needs
 * to be a sensible size, however, as it constrains the max stepping value used
 * for mixing, as well as the maximum number of samples per mixing iteration.
 *
 * The mixer requires being able to do two samplings per mixing loop. With the
 * cubic resampler (which requires 3 padding samples), this limits a 2048
 * buffer size to about 2044. This means that buffer_freq*source_pitch cannot
 * exceed device_freq*2044 for a 32-bit buffer.
 */
#ifndef BUFFERSIZE
#define BUFFERSIZE 2048
#endif


struct ALCdevice_struct
{
    volatile RefCount ref;

    ALCboolean Connected;
    enum DeviceType Type;

    CRITICAL_SECTION Mutex;

    ALuint       Frequency;
    ALuint       UpdateSize;
    ALuint       NumUpdates;
    enum DevFmtChannels FmtChans;
    enum DevFmtType     FmtType;

    ALCchar      *DeviceName;

    volatile ALCenum LastError;

    // Maximum number of sources that can be created
    ALuint       MaxNoOfSources;
    // Maximum number of slots that can be created
    ALuint       AuxiliaryEffectSlotMax;

    ALCuint      NumMonoSources;
    ALCuint      NumStereoSources;
    ALuint       NumAuxSends;

    // Map of Buffers for this device
    UIntMap BufferMap;

    // Map of Effects for this device
    UIntMap EffectMap;

    // Map of Filters for this device
    UIntMap FilterMap;

    /* HRTF filter tables */
    const struct Hrtf *Hrtf;

    // Stereo-to-binaural filter
    struct bs2b *Bs2b;
    ALCint       Bs2bLevel;

    // Device flags
    ALuint       Flags;

    ALuint ChannelOffsets[MaxChannels];

    enum Channel Speaker2Chan[MaxChannels];
    ALfloat SpeakerAngle[MaxChannels];
    ALuint  NumChan;

    /* Temp storage used for mixing. +1 for the predictive sample. */
    ALIGN(16) ALfloat SampleData1[BUFFERSIZE+1];
    ALIGN(16) ALfloat SampleData2[BUFFERSIZE+1];

    // Dry path buffer mix
    ALIGN(16) ALfloat DryBuffer[MaxChannels][BUFFERSIZE];

    ALIGN(16) ALfloat ClickRemoval[MaxChannels];
    ALIGN(16) ALfloat PendingClicks[MaxChannels];

    /* Default effect slot */
    struct ALeffectslot *DefaultSlot;

    // Contexts created on this device
    ALCcontext *volatile ContextList;

    BackendFuncs *Funcs;
    void         *ExtraData; // For the backend's use

    ALCdevice *volatile next;
};

#define ALCdevice_OpenPlayback(a,b)      ((a)->Funcs->OpenPlayback((a), (b)))
#define ALCdevice_ClosePlayback(a)       ((a)->Funcs->ClosePlayback((a)))
#define ALCdevice_ResetPlayback(a)       ((a)->Funcs->ResetPlayback((a)))
#define ALCdevice_StartPlayback(a)       ((a)->Funcs->StartPlayback((a)))
#define ALCdevice_StopPlayback(a)        ((a)->Funcs->StopPlayback((a)))
#define ALCdevice_OpenCapture(a,b)       ((a)->Funcs->OpenCapture((a), (b)))
#define ALCdevice_CloseCapture(a)        ((a)->Funcs->CloseCapture((a)))
#define ALCdevice_StartCapture(a)        ((a)->Funcs->StartCapture((a)))
#define ALCdevice_StopCapture(a)         ((a)->Funcs->StopCapture((a)))
#define ALCdevice_CaptureSamples(a,b,c)  ((a)->Funcs->CaptureSamples((a), (b), (c)))
#define ALCdevice_AvailableSamples(a)    ((a)->Funcs->AvailableSamples((a)))
#define ALCdevice_Lock(a)                ((a)->Funcs->Lock((a)))
#define ALCdevice_Unlock(a)              ((a)->Funcs->Unlock((a)))
#define ALCdevice_GetLatency(a)          ((a)->Funcs->GetLatency((a)))

// Frequency was requested by the app or config file
#define DEVICE_FREQUENCY_REQUEST                 (1<<1)
// Channel configuration was requested by the config file
#define DEVICE_CHANNELS_REQUEST                  (1<<2)
// Sample type was requested by the config file
#define DEVICE_SAMPLE_TYPE_REQUEST               (1<<3)
// HRTF was requested by the app
#define DEVICE_HRTF_REQUEST                      (1<<4)

// Stereo sources cover 120-degree angles around +/-90
#define DEVICE_WIDE_STEREO                       (1<<16)

// Specifies if the device is currently running
#define DEVICE_RUNNING                           (1<<31)

/* Invalid channel offset */
#define INVALID_OFFSET                           (~0u)


#define LookupBuffer(m, k) ((struct ALbuffer*)LookupUIntMapKey(&(m)->BufferMap, (k)))
#define LookupEffect(m, k) ((struct ALeffect*)LookupUIntMapKey(&(m)->EffectMap, (k)))
#define LookupFilter(m, k) ((struct ALfilter*)LookupUIntMapKey(&(m)->FilterMap, (k)))
#define RemoveBuffer(m, k) ((struct ALbuffer*)RemoveUIntMapKey(&(m)->BufferMap, (k)))
#define RemoveEffect(m, k) ((struct ALeffect*)RemoveUIntMapKey(&(m)->EffectMap, (k)))
#define RemoveFilter(m, k) ((struct ALfilter*)RemoveUIntMapKey(&(m)->FilterMap, (k)))


struct ALCcontext_struct
{
    volatile RefCount ref;

    struct ALlistener *Listener;

    UIntMap SourceMap;
    UIntMap EffectSlotMap;

    volatile ALenum LastError;

    volatile ALenum UpdateSources;

    volatile enum DistanceModel DistanceModel;
    volatile ALboolean SourceDistanceModel;

    volatile ALfloat DopplerFactor;
    volatile ALfloat DopplerVelocity;
    volatile ALfloat SpeedOfSound;
    volatile ALenum  DeferUpdates;

    struct ALsource **ActiveSources;
    ALsizei           ActiveSourceCount;
    ALsizei           MaxActiveSources;

    struct ALeffectslot **ActiveEffectSlots;
    ALsizei               ActiveEffectSlotCount;
    ALsizei               MaxActiveEffectSlots;

    ALCdevice  *Device;
    const ALCchar *ExtensionList;

    ALCcontext *volatile next;
};

#define LookupSource(m, k) ((struct ALsource*)LookupUIntMapKey(&(m)->SourceMap, (k)))
#define LookupEffectSlot(m, k) ((struct ALeffectslot*)LookupUIntMapKey(&(m)->EffectSlotMap, (k)))
#define RemoveSource(m, k) ((struct ALsource*)RemoveUIntMapKey(&(m)->SourceMap, (k)))
#define RemoveEffectSlot(m, k) ((struct ALeffectslot*)RemoveUIntMapKey(&(m)->EffectSlotMap, (k)))


ALCcontext *GetContextRef(void);

void ALCcontext_IncRef(ALCcontext *context);
void ALCcontext_DecRef(ALCcontext *context);

void AppendAllDevicesList(const ALCchar *name);
void AppendCaptureDeviceList(const ALCchar *name);

void ALCdevice_LockDefault(ALCdevice *device);
void ALCdevice_UnlockDefault(ALCdevice *device);
ALint64 ALCdevice_GetLatencyDefault(ALCdevice *device);

static inline void LockContext(ALCcontext *context)
{ ALCdevice_Lock(context->Device); }
static inline void UnlockContext(ALCcontext *context)
{ ALCdevice_Unlock(context->Device); }


void *al_malloc(size_t alignment, size_t size);
void *al_calloc(size_t alignment, size_t size);
void al_free(void *ptr);

typedef struct {
#ifdef HAVE_FENV_H
    DERIVE_FROM_TYPE(fenv_t);
#else
    int state;
#endif
#ifdef HAVE_SSE
    int sse_state;
#endif
} FPUCtl;
void SetMixerFPUMode(FPUCtl *ctl);
void RestoreFPUMode(const FPUCtl *ctl);

ALvoid *StartThread(ALuint (*func)(ALvoid*), ALvoid *ptr);
ALuint StopThread(ALvoid *thread);

typedef struct RingBuffer RingBuffer;
RingBuffer *CreateRingBuffer(ALsizei frame_size, ALsizei length);
void DestroyRingBuffer(RingBuffer *ring);
ALsizei RingBufferSize(RingBuffer *ring);
void WriteRingBuffer(RingBuffer *ring, const ALubyte *data, ALsizei len);
void ReadRingBuffer(RingBuffer *ring, ALubyte *data, ALsizei len);

void ReadALConfig(void);
void FreeALConfig(void);
int ConfigValueExists(const char *blockName, const char *keyName);
const char *GetConfigValue(const char *blockName, const char *keyName, const char *def);
int GetConfigValueBool(const char *blockName, const char *keyName, int def);
int ConfigValueStr(const char *blockName, const char *keyName, const char **ret);
int ConfigValueInt(const char *blockName, const char *keyName, int *ret);
int ConfigValueUInt(const char *blockName, const char *keyName, unsigned int *ret);
int ConfigValueFloat(const char *blockName, const char *keyName, float *ret);

void SetRTPriority(void);

void SetDefaultChannelOrder(ALCdevice *device);
void SetDefaultWFXChannelOrder(ALCdevice *device);

const ALCchar *DevFmtTypeString(enum DevFmtType type);
const ALCchar *DevFmtChannelsString(enum DevFmtChannels chans);

#define HRIR_BITS        (7)
#define HRIR_LENGTH      (1<<HRIR_BITS)
#define HRIR_MASK        (HRIR_LENGTH-1)
#define HRTFDELAY_BITS    (20)
#define HRTFDELAY_FRACONE (1<<HRTFDELAY_BITS)
#define HRTFDELAY_MASK    (HRTFDELAY_FRACONE-1)
const struct Hrtf *GetHrtf(ALCdevice *device);
void FindHrtfFormat(const ALCdevice *device, enum DevFmtChannels *chans, ALCuint *srate);
void FreeHrtfs(void);
ALuint GetHrtfIrSize(const struct Hrtf *Hrtf);
ALfloat CalcHrtfDelta(ALfloat oldGain, ALfloat newGain, const ALfloat olddir[3], const ALfloat newdir[3]);
void GetLerpedHrtfCoeffs(const struct Hrtf *Hrtf, ALfloat elevation, ALfloat azimuth, ALfloat gain, ALfloat (*coeffs)[2], ALuint *delays);
ALuint GetMovingHrtfCoeffs(const struct Hrtf *Hrtf, ALfloat elevation, ALfloat azimuth, ALfloat gain, ALfloat delta, ALint counter, ALfloat (*coeffs)[2], ALuint *delays, ALfloat (*coeffStep)[2], ALint *delayStep);

void al_print(const char *type, const char *func, const char *fmt, ...) PRINTF_STYLE(3,4);
#define AL_PRINT(T, ...) al_print((T), __FUNCTION__, __VA_ARGS__)

extern FILE *LogFile;
enum LogLevel {
    NoLog,
    LogError,
    LogWarning,
    LogTrace,
    LogRef
};
extern enum LogLevel LogLevel;

#define TRACEREF(...) do {                                                    \
    if(LogLevel >= LogRef)                                                    \
        AL_PRINT("(--)", __VA_ARGS__);                                        \
} while(0)

#define TRACE(...) do {                                                       \
    if(LogLevel >= LogTrace)                                                  \
        AL_PRINT("(II)", __VA_ARGS__);                                        \
} while(0)

#define WARN(...) do {                                                        \
    if(LogLevel >= LogWarning)                                                \
        AL_PRINT("(WW)", __VA_ARGS__);                                        \
} while(0)

#define ERR(...) do {                                                         \
    if(LogLevel >= LogError)                                                  \
        AL_PRINT("(EE)", __VA_ARGS__);                                        \
} while(0)


extern ALint RTPrioLevel;


extern ALuint CPUCapFlags;
enum {
    CPU_CAP_SSE    = 1<<0,
    CPU_CAP_SSE2   = 1<<1,
    CPU_CAP_NEON   = 1<<2,
};

void FillCPUCaps(ALuint capfilter);


/**
 * Starts a try block. Must not be nested within another try block within the
 * same function.
 */
#define al_try do {                                                            \
    int _al_in_try_block = 1;
/** Marks the end of the try block. */
#define al_endtry _al_endtry_label:                                            \
    (void)_al_in_try_block;                                                    \
} while(0)

/**
 * The try block is terminated, and execution jumps to al_endtry.
 */
#define al_throw do {                                                         \
    _al_in_try_block = 0;                                                     \
    goto _al_endtry_label;                                                    \
} while(0)
/** Sets an AL error on the given context, before throwing. */
#define al_throwerr(ctx, err) do {                                            \
    alSetError((ctx), (err));                                                 \
    al_throw;                                                                 \
} while(0)

/**
 * Throws an AL_INVALID_VALUE error with the given ctx if the given condition
 * is false.
 */
#define CHECK_VALUE(ctx, cond) do {                                           \
    if(!(cond))                                                               \
        al_throwerr((ctx), AL_INVALID_VALUE);                                 \
} while(0)

#ifdef __cplusplus
}
#endif

#endif
