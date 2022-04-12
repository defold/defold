//
// Copyright 2014-2018 Celtoys Ltd
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     http://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.
//

/*
@Contents:

    @DEPS:          External Dependencies
    @TIMERS:        Platform-specific timers
    @TLS:           Thread-Local Storage
    @ATOMIC:        Atomic Operations
    @RNG:           Random Number Generator
    @LFSR:          Galois Linear-feedback Shift Register
    @VMBUFFER:      Mirror Buffer using Virtual Memory for auto-wrap
    @NEW:           New/Delete operators with error values for simplifying object create/destroy
    @SAFEC:         Safe C Library excerpts
    @OSTHREADS:     Wrappers around OS-specific thread functions
    @THREADS:       Cross-platform thread object
    @OBJALLOC:      Reusable Object Allocator
    @DYNBUF:        Dynamic Buffer
    @HASHTABLE:     Integer pair hash map for inserts/finds. No removes for added simplicity.
    @STRINGTABLE:	Map from string hash to string offset in local buffer
    @SOCKETS:       Sockets TCP/IP Wrapper
    @SHA1:          SHA-1 Cryptographic Hash Function
    @BASE64:        Base-64 encoder
    @MURMURHASH:    Murmur-Hash 3
    @WEBSOCKETS:    WebSockets
    @MESSAGEQ:      Multiple producer, single consumer message queue
    @NETWORK:       Network Server
    @SAMPLE:        Base Sample Description (CPU by default)
    @SAMPLETREE:    A tree of samples with their allocator
    @TPROFILER:     Thread Profiler data, storing both sampling and instrumentation results
    @TGATHER:       Thread Gatherer, periodically polling for newly created threads
    @TSAMPLER:      Sampling thread contexts
    @REMOTERY:      Remotery
    @CUDA:          CUDA event sampling
    @D3D11:         Direct3D 11 event sampling
    @OPENGL:        OpenGL event sampling
    @METAL:         Metal event sampling
*/

#define RMT_IMPL
#include "Remotery.h"

#ifdef RMT_PLATFORM_WINDOWS
#pragma comment(lib, "ws2_32.lib")
#pragma comment(lib, "winmm.lib")
#endif

#if RMT_ENABLED

// Global settings
static rmtSettings g_Settings;
static rmtBool g_SettingsInitialized = RMT_FALSE;

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @DEPS: External Dependencies
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

// clang-format off

//
// Required CRT dependencies
//
#if RMT_USE_TINYCRT

	#include <TinyCRT/TinyCRT.h>
	#include <TinyCRT/TinyWinsock.h>
    #include <Memory/Memory.h>

    #define CreateFileMapping CreateFileMappingA

#else

    #ifdef RMT_PLATFORM_MACOS
        #include <mach/mach_time.h>
        #include <mach/vm_map.h>
        #include <mach/mach.h>
        #include <sys/time.h>
    #else
        #if !defined(__FreeBSD__) && !defined(__OpenBSD__)
            #include <malloc.h>
        #endif
    #endif

    #include <assert.h>
    #include <stdio.h>
    #include <time.h>
    #include <limits.h>
    #include <stdlib.h>

    #ifdef RMT_PLATFORM_WINDOWS
        #include <winsock2.h>
        #ifndef __MINGW32__
            #include <intrin.h>
        #endif
        #undef min
        #undef max
        #include <tlhelp32.h>
        #include <winnt.h>
        #include <processthreadsapi.h>
        #ifdef _XBOX_ONE
            #include "xmem.h"
        #endif
        typedef long NTSTATUS;  // winternl.h
    #endif

    #ifdef RMT_PLATFORM_LINUX
        #if defined(__FreeBSD__) || defined(__OpenBSD__)
            #include <pthread_np.h>
        #else
            #include <sys/prctl.h>
        #endif
    #endif

    #if defined(RMT_PLATFORM_POSIX)
        #include <pthread.h>
        #include <unistd.h>
        #include <string.h>
        #include <sys/socket.h>
        #include <sys/mman.h>
        #include <netinet/in.h>
        #include <fcntl.h>
        #include <errno.h>
        #include <dlfcn.h>
    #endif

    #ifdef __MINGW32__
        #include <pthread.h>
    #endif

#endif

#if RMT_USE_CUDA
    #include <cuda.h>
#endif

// clang-format on

#if defined(_MSC_VER) && !defined(__clang__)
    #define RMT_UNREFERENCED_PARAMETER(i) (i)
#else
    #define RMT_UNREFERENCED_PARAMETER(i) (void)(1 ? (void)0 : ((void)i))
#endif

static rmtU8 minU8(rmtU8 a, rmtU8 b)
{
    return a < b ? a : b;
}
static rmtU16 maxU16(rmtU16 a, rmtU16 b)
{
    return a > b ? a : b;
}
static rmtU32 minU32(rmtU32 a, rmtU32 b)
{
    return a < b ? a : b;
}
static rmtS64 maxS64(rmtS64 a, rmtS64 b)
{
    return a > b ? a : b;
}

// Memory management functions
static void* rmtMalloc(rmtU32 size)
{
    return g_Settings.malloc(g_Settings.mm_context, size);
}

static void* rmtRealloc(void* ptr, rmtU32 size)
{
    return g_Settings.realloc(g_Settings.mm_context, ptr, size);
}

static void rmtFree(void* ptr)
{
    g_Settings.free(g_Settings.mm_context, ptr);
}

// File system functions
static FILE* rmtOpenFile(const char* filename, const char* mode)
{
#if defined(RMT_PLATFORM_WINDOWS) && !RMT_USE_TINYCRT
    FILE* fp;
    return fopen_s(&fp, filename, mode) == 0 ? fp : NULL;
#else
    return fopen(filename, mode);
#endif
}

void rmtCloseFile(FILE* fp)
{
    if (fp != NULL)
    {
        fclose(fp);
    }
}

rmtBool rmtWriteFile(FILE* fp, const void* data, rmtU32 size)
{
    assert(fp != NULL);
    return fwrite(data, size, 1, fp) == size ? RMT_TRUE : RMT_FALSE;
}

#if RMT_USE_OPENGL
// DLL/Shared Library functions

static void* rmtLoadLibrary(const char* path)
{
#if defined(RMT_PLATFORM_WINDOWS)
    return (void*)LoadLibraryA(path);
#elif defined(RMT_PLATFORM_POSIX)
    return dlopen(path, RTLD_LOCAL | RTLD_LAZY);
#else
    return NULL;
#endif
}

static void rmtFreeLibrary(void* handle)
{
#if defined(RMT_PLATFORM_WINDOWS)
    FreeLibrary((HMODULE)handle);
#elif defined(RMT_PLATFORM_POSIX)
    dlclose(handle);
#endif
}

#if defined(RMT_PLATFORM_WINDOWS)
typedef FARPROC ProcReturnType;
#else
typedef void* ProcReturnType;
#endif

static ProcReturnType rmtGetProcAddress(void* handle, const char* symbol)
{
#if defined(RMT_PLATFORM_WINDOWS)
    return GetProcAddress((HMODULE)handle, (LPCSTR)symbol);
#elif defined(RMT_PLATFORM_POSIX)
    return dlsym(handle, symbol);
#endif
}

#endif

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @TIMERS: Platform-specific timers
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

//
// Get millisecond timer value that has only one guarantee: multiple calls are consistently comparable.
// On some platforms, even though this returns milliseconds, the timer may be far less accurate.
//
static rmtU32 msTimer_Get()
{
#ifdef RMT_PLATFORM_WINDOWS

    return (rmtU32)GetTickCount();

#else

    clock_t time = clock();

// CLOCKS_PER_SEC is 128 on FreeBSD, causing div/0
#if defined(__FreeBSD__) || defined(__OpenBSD__)
    rmtU32 msTime = (rmtU32)(time * 1000 / CLOCKS_PER_SEC);
#else
    rmtU32 msTime = (rmtU32)(time / (CLOCKS_PER_SEC / 1000));
#endif

    return msTime;

#endif
}

//
// Micro-second accuracy high performance counter
//
#ifndef RMT_PLATFORM_WINDOWS
typedef rmtU64 LARGE_INTEGER;
#endif
typedef struct
{
    LARGE_INTEGER counter_start;
    double counter_scale;
} usTimer;

static void usTimer_Init(usTimer* timer)
{
#if defined(RMT_PLATFORM_WINDOWS)
    LARGE_INTEGER performance_frequency;

    assert(timer != NULL);

    // Calculate the scale from performance counter to microseconds
    QueryPerformanceFrequency(&performance_frequency);
    timer->counter_scale = 1000000.0 / performance_frequency.QuadPart;

    // Record the offset for each read of the counter
    QueryPerformanceCounter(&timer->counter_start);

#elif defined(RMT_PLATFORM_MACOS)

    mach_timebase_info_data_t nsScale;
    mach_timebase_info(&nsScale);
    const double ns_per_us = 1.0e3;
    timer->counter_scale = (double)(nsScale.numer) / ((double)nsScale.denom * ns_per_us);

    timer->counter_start = mach_absolute_time();

#elif defined(RMT_PLATFORM_LINUX)

    struct timespec tv;
    clock_gettime(CLOCK_REALTIME, &tv);
    timer->counter_start = (rmtU64)(tv.tv_sec * (rmtU64)1000000) + (rmtU64)(tv.tv_nsec * 0.001);

#endif
}

static rmtU64 usTimer_Get(usTimer* timer)
{
#if defined(RMT_PLATFORM_WINDOWS)
    LARGE_INTEGER performance_count;

    assert(timer != NULL);

    // Read counter and convert to microseconds
    QueryPerformanceCounter(&performance_count);
    return (rmtU64)((performance_count.QuadPart - timer->counter_start.QuadPart) * timer->counter_scale);

#elif defined(RMT_PLATFORM_MACOS)

    rmtU64 curr_time = mach_absolute_time();
    return (rmtU64)((curr_time - timer->counter_start) * timer->counter_scale);

#elif defined(RMT_PLATFORM_LINUX)

    struct timespec tv;
    clock_gettime(CLOCK_REALTIME, &tv);
    return ((rmtU64)(tv.tv_sec * (rmtU64)1000000) + (rmtU64)(tv.tv_nsec * 0.001)) - timer->counter_start;

#endif
}

static void msSleep(rmtU32 time_ms)
{
#ifdef RMT_PLATFORM_WINDOWS
    Sleep(time_ms);
#elif defined(RMT_PLATFORM_POSIX)
    usleep(time_ms * 1000);
#endif
}

static struct tm* TimeDateNow()
{
    time_t time_now = time(NULL);

#if defined(RMT_PLATFORM_WINDOWS) && !RMT_USE_TINYCRT
    // Discard the thread-safety benefit of gmtime_s
    static struct tm tm_now;
    gmtime_s(&tm_now, &time_now);
    return &tm_now;
#else
    return gmtime(&time_now);
#endif
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @TLS: Thread-Local Storage
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

#define TLS_INVALID_HANDLE 0xFFFFFFFF

#if defined(RMT_PLATFORM_WINDOWS)
typedef rmtU32 rmtTLS;
#else
typedef pthread_key_t rmtTLS;
#endif

static rmtError tlsAlloc(rmtTLS* handle)
{
    assert(handle != NULL);

#if defined(RMT_PLATFORM_WINDOWS)
    *handle = (rmtTLS)TlsAlloc();
    if (*handle == TLS_OUT_OF_INDEXES)
    {
        *handle = TLS_INVALID_HANDLE;
        return RMT_ERROR_TLS_ALLOC_FAIL;
    }
#elif defined(RMT_PLATFORM_POSIX)
    if (pthread_key_create(handle, NULL) != 0)
    {
        *handle = TLS_INVALID_HANDLE;
        return RMT_ERROR_TLS_ALLOC_FAIL;
    }
#endif

    return RMT_ERROR_NONE;
}

static void tlsFree(rmtTLS handle)
{
    assert(handle != TLS_INVALID_HANDLE);
#if defined(RMT_PLATFORM_WINDOWS)
    TlsFree(handle);
#elif defined(RMT_PLATFORM_POSIX)
    pthread_key_delete((pthread_key_t)handle);
#endif
}

static void tlsSet(rmtTLS handle, void* value)
{
    assert(handle != TLS_INVALID_HANDLE);
#if defined(RMT_PLATFORM_WINDOWS)
    TlsSetValue(handle, value);
#elif defined(RMT_PLATFORM_POSIX)
    pthread_setspecific((pthread_key_t)handle, value);
#endif
}

static void* tlsGet(rmtTLS handle)
{
    assert(handle != TLS_INVALID_HANDLE);
#if defined(RMT_PLATFORM_WINDOWS)
    return TlsGetValue(handle);
#elif defined(RMT_PLATFORM_POSIX)
    return pthread_getspecific((pthread_key_t)handle);
#endif
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @MUTEX: Mutexes
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

#ifdef RMT_PLATFORM_WINDOWS
typedef CRITICAL_SECTION rmtMutex;
#else
typedef pthread_mutex_t rmtMutex;
#endif

static void mtxInit(rmtMutex* mutex)
{
    assert(mutex != NULL);
#if defined(RMT_PLATFORM_WINDOWS)
    InitializeCriticalSection(mutex);
#elif defined(RMT_PLATFORM_POSIX)
    pthread_mutex_init(mutex, NULL);
#endif
}

static void mtxLock(rmtMutex* mutex)
{
    assert(mutex != NULL);
#if defined(RMT_PLATFORM_WINDOWS)
    EnterCriticalSection(mutex);
#elif defined(RMT_PLATFORM_POSIX)
    pthread_mutex_lock(mutex);
#endif
}

static void mtxUnlock(rmtMutex* mutex)
{
    assert(mutex != NULL);
#if defined(RMT_PLATFORM_WINDOWS)
    LeaveCriticalSection(mutex);
#elif defined(RMT_PLATFORM_POSIX)
    pthread_mutex_unlock(mutex);
#endif
}

static void mtxDelete(rmtMutex* mutex)
{
    assert(mutex != NULL);
#if defined(RMT_PLATFORM_WINDOWS)
    DeleteCriticalSection(mutex);
#elif defined(RMT_PLATFORM_POSIX)
    pthread_mutex_destroy(mutex);
#endif
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @ATOMIC: Atomic Operations
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

static rmtBool AtomicCompareAndSwap(rmtU32 volatile* val, long old_val, long new_val)
{
#if defined(RMT_PLATFORM_WINDOWS) && !defined(__MINGW32__)
    return _InterlockedCompareExchange((long volatile*)val, new_val, old_val) == old_val ? RMT_TRUE : RMT_FALSE;
#elif defined(RMT_PLATFORM_POSIX) || defined(__MINGW32__)
    return __sync_bool_compare_and_swap(val, old_val, new_val) ? RMT_TRUE : RMT_FALSE;
#endif
}

static rmtBool AtomicCompareAndSwapPointer(long* volatile* ptr, long* old_ptr, long* new_ptr)
{
#if defined(RMT_PLATFORM_WINDOWS) && !defined(__MINGW32__)
#ifdef _WIN64
    return _InterlockedCompareExchange64((__int64 volatile*)ptr, (__int64)new_ptr, (__int64)old_ptr) == (__int64)old_ptr
               ? RMT_TRUE
               : RMT_FALSE;
#else
    return _InterlockedCompareExchange((long volatile*)ptr, (long)new_ptr, (long)old_ptr) == (long)old_ptr ? RMT_TRUE
                                                                                                           : RMT_FALSE;
#endif
#elif defined(RMT_PLATFORM_POSIX) || defined(__MINGW32__)
    return __sync_bool_compare_and_swap(ptr, old_ptr, new_ptr) ? RMT_TRUE : RMT_FALSE;
#endif
}

//
// NOTE: Does not guarantee a memory barrier
// TODO: Make sure all platforms don't insert a memory barrier as this is only for stats
//       Alternatively, add strong/weak memory order equivalents
//
static rmtS32 AtomicAdd(rmtS32 volatile* value, rmtS32 add)
{
#if defined(RMT_PLATFORM_WINDOWS) && !defined(__MINGW32__)
    return _InterlockedExchangeAdd((long volatile*)value, (long)add);
#elif defined(RMT_PLATFORM_POSIX) || defined(__MINGW32__)
    return __sync_fetch_and_add(value, add);
#endif
}

static void AtomicSub(rmtS32 volatile* value, rmtS32 sub)
{
    // Not all platforms have an implementation so just negate and add
    AtomicAdd(value, -sub);
}

static void CompilerWriteFence()
{
#if defined(__clang__)
    __asm__ volatile("" : : : "memory");
#elif defined(RMT_PLATFORM_WINDOWS) && !defined(__MINGW32__)
    _WriteBarrier();
#else
    asm volatile("" : : : "memory");
#endif
}

static void CompilerReadFence()
{
#if defined(__clang__)
    __asm__ volatile("" : : : "memory");
#elif defined(RMT_PLATFORM_WINDOWS) && !defined(__MINGW32__)
    _ReadBarrier();
#else
    asm volatile("" : : : "memory");
#endif
}

static rmtU32 LoadAcquire(rmtU32* volatile address)
{
    rmtU32 value = *address;
    CompilerReadFence();
    return value;
}

static long* LoadAcquirePointer(long* volatile* ptr)
{
    long* value = *ptr;
    CompilerReadFence();
    return value;
}

static void StoreRelease(rmtU32* volatile address, rmtU32 value)
{
    CompilerWriteFence();
    *address = value;
}

static void StoreReleasePointer(long* volatile* ptr, long* value)
{
    CompilerWriteFence();
    *ptr = value;
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @RNG: Random Number Generator
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

//
// WELL: Well Equidistributed Long-period Linear
// These algorithms produce numbers with better equidistrib ution than MT19937 and improve upon “bit-mixing” properties. They are
// fast, come in many sizes, and produce higher quality random numbers.
//
// This implementation has a period of 2^512, or 10^154.
//
// Implementation from: Game Programming Gems 7, Random Number Generation Chris Lomont
// Documentation: http://www.lomont.org/Math/Papers/2008/Lomont_PRNG_2008.pdf
//

// Global RNG state for now
// Far better than interfering with the user's rand()
#define Well512_StateSize 16
static rmtU32 Well512_State[Well512_StateSize];
static rmtU32 Well512_Index;

static void Well512_Init(rmtU32 seed)
{
    rmtU32 i;

    // Generate initial state from seed
    Well512_State[0] = seed;
    for (i = 1; i < Well512_StateSize; i++)
    {
        rmtU32 prev = Well512_State[i - 1];
        Well512_State[i] = (1812433253 * (prev ^ (prev >> 30)) + i);
    }
    Well512_Index = 0;
}

static rmtU32 Well512_RandomU32()
{
    rmtU32 a, b, c, d;

    a = Well512_State[Well512_Index];
    c = Well512_State[(Well512_Index + 13) & 15];
    b = a ^ c ^ (a << 16) ^ (c << 15);
    c = Well512_State[(Well512_Index + 9) & 15];
    c ^= (c >> 11);
    a = Well512_State[Well512_Index] = b ^ c;
    d = a ^ ((a << 5) & 0xDA442D24UL);
    Well512_Index = (Well512_Index + 15) & 15;
    a = Well512_State[Well512_Index];
    Well512_State[Well512_Index] = a ^ b ^ d ^ (a << 2) ^ (b << 18) ^ (c << 28);
    return Well512_State[Well512_Index];
}

static rmtU32 Well512_RandomOpenLimit(rmtU32 limit)
{
    // Using % to modulo with range is just masking out the higher bits, leaving a result that's objectively biased.
    // Dividing by RAND_MAX is better but leads to increased repetition at low ranges due to very large bucket sizes.
    // Instead use multiple passes with smaller bucket sizes, rejecting results that don't fit into this smaller range.
    rmtU32 bucket_size = UINT_MAX / limit;
    rmtU32 bucket_limit = bucket_size * limit;
    rmtU32 r;
    do
    {
        r = Well512_RandomU32();
    } while(r >= bucket_limit);

    return r / bucket_size;
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @LFSR: Galois Linear-feedback Shift Register
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

static rmtU32 Log2i(rmtU32 x)
{
	static const rmtU32 MultiplyDeBruijnBitPosition[32] =
	{
		0, 9, 1, 10, 13, 21, 2, 29, 11, 14, 16, 18, 22, 25, 3, 30,
		8, 12, 20, 28, 15, 17, 24, 7, 19, 27, 23, 6, 26, 5, 4, 31
	};

    // First round down to one less than a power of two
	x |= x >> 1;
	x |= x >> 2;
	x |= x >> 4;
	x |= x >> 8;
	x |= x >> 16;

	return MultiplyDeBruijnBitPosition[(rmtU32)(x * 0x07C4ACDDU) >> 27];
}

static rmtU32 GaloisLFSRMask(rmtU32 table_size_log2)
{
    // Taps for 4 to 8 bit ranges
    static const rmtU32 XORMasks[] =
    {
        ((1 << 0) | (1 << 1)),                          // 2
        ((1 << 1) | (1 << 2)),                          // 3
        ((1 << 2) | (1 << 3)),                          // 4
        ((1 << 2) | (1 << 4)),                          // 5
        ((1 << 4) | (1 << 5)),                          // 6
        ((1 << 5) | (1 << 6)),                          // 7
        ((1 << 3) | (1 << 4) | (1 << 5) | (1 << 7)),    // 8
    };

    // Map table size to required XOR mask
    assert(table_size_log2 >= 2);
    assert(table_size_log2 <= 8);
    return XORMasks[table_size_log2 - 2];
}

static rmtU32 GaloisLFSRNext(rmtU32 value, rmtU32 xor_mask)
{
    // Output bit
    rmtU32 lsb = value & 1;

    // Apply the register shift
    value >>= 1;

    // Apply toggle mask if the output bit is set
    if (lsb != 0)
    {
        value ^= xor_mask;
    }

    return value;
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @NEW: New/Delete operators with error values for simplifying object create/destroy
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

// Ensures the pointer is non-NULL, calls the destructor, frees memory and sets the pointer to NULL
#define Delete(type, obj)       \
    if (obj != NULL)            \
    {                           \
        type##_Destructor(obj); \
        rmtFree(obj);           \
        obj = NULL;             \
    }

// New is implemented in terms of two begin/end macros
// New will allocate enough space for the object and call the constructor
// If allocation fails the constructor won't be called
// If the constructor fails, the destructor is called and memory is released
// NOTE: Use of sizeof() requires that the type be defined at the point of call
// This is a disadvantage over requiring only a custom Create function
#define BeginNew(type, obj)                   \
    {                                         \
        obj = (type*)rmtMalloc(sizeof(type)); \
        if (obj == NULL)                      \
        {                                     \
            error = RMT_ERROR_MALLOC_FAIL;    \
        }                                     \
        else                                  \
        {

#define EndNew(type, obj)        \
    if (error != RMT_ERROR_NONE) \
        Delete(type, obj);       \
    }                            \
    }

// Specialisations for New with varying constructor parameter counts
#define New_0(type, obj)             \
    BeginNew(type, obj);             \
    error = type##_Constructor(obj); \
    EndNew(type, obj)
#define New_1(type, obj, a0)             \
    BeginNew(type, obj);                 \
    error = type##_Constructor(obj, a0); \
    EndNew(type, obj)
#define New_2(type, obj, a0, a1)             \
    BeginNew(type, obj);                     \
    error = type##_Constructor(obj, a0, a1); \
    EndNew(type, obj)
#define New_3(type, obj, a0, a1, a2)             \
    BeginNew(type, obj);                         \
    error = type##_Constructor(obj, a0, a1, a2); \
    EndNew(type, obj)

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @VMBUFFER: Mirror Buffer using Virtual Memory for auto-wrap
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

typedef struct VirtualMirrorBuffer
{
    // Page-rounded size of the buffer without mirroring
    rmtU32 size;

    // Pointer to the first part of the mirror
    // The second part comes directly after at ptr+size bytes
    rmtU8* ptr;

#ifdef RMT_PLATFORM_WINDOWS
#ifdef _XBOX_ONE
    size_t page_count;
    size_t* page_mapping;
#else
    HANDLE file_map_handle;
#endif
#endif

} VirtualMirrorBuffer;

#ifdef __ANDROID__
/*
 * Copyright (C) 2008 The Android Open Source Project
 *
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 */

#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/ashmem.h>
#include <fcntl.h>
#include <string.h>
#define ASHMEM_DEVICE "/dev/ashmem"

/*
 * ashmem_create_region - creates a new ashmem region and returns the file
 * descriptor, or <0 on error
 *
 * `name' is an optional label to give the region (visible in /proc/pid/maps)
 * `size' is the size of the region, in page-aligned bytes
 */
int ashmem_create_region(const char* name, size_t size)
{
    int fd, ret;

    fd = open(ASHMEM_DEVICE, O_RDWR);
    if (fd < 0)
        return fd;

    if (name)
    {
        char buf[ASHMEM_NAME_LEN] = {0};

        strncpy(buf, name, sizeof(buf));
        buf[sizeof(buf) - 1] = 0;
        ret = ioctl(fd, ASHMEM_SET_NAME, buf);
        if (ret < 0)
            goto error;
    }

    ret = ioctl(fd, ASHMEM_SET_SIZE, size);
    if (ret < 0)
        goto error;

    return fd;

error:
    close(fd);
    return ret;
}
#endif // __ANDROID__

static rmtError VirtualMirrorBuffer_Constructor(VirtualMirrorBuffer* buffer, rmtU32 size, int nb_attempts)
{
    static const rmtU32 k_64 = 64 * 1024;
    RMT_UNREFERENCED_PARAMETER(nb_attempts);

#ifdef RMT_PLATFORM_LINUX
#if defined(__FreeBSD__) || defined(__OpenBSD__)
    char path[] = "/tmp/ring-buffer-XXXXXX";
#else
    char path[] = "/dev/shm/ring-buffer-XXXXXX";
#endif
    int file_descriptor;
#endif

    // Round up to page-granulation; the nearest 64k boundary for now
    size = (size + k_64 - 1) / k_64 * k_64;

    // Set defaults
    buffer->size = size;
    buffer->ptr = NULL;
#ifdef RMT_PLATFORM_WINDOWS
#ifdef _XBOX_ONE
    buffer->page_count = 0;
    buffer->page_mapping = NULL;
#else
    buffer->file_map_handle = INVALID_HANDLE_VALUE;
#endif
#endif

#ifdef RMT_PLATFORM_WINDOWS
#ifdef _XBOX_ONE

    // Xbox version based on Windows version and XDK reference

    buffer->page_count = size / k_64;
    if (buffer->page_mapping)
    {
        free(buffer->page_mapping);
    }
    buffer->page_mapping = (size_t*)malloc(sizeof(ULONG) * buffer->page_count);

    while (nb_attempts-- > 0)
    {
        rmtU8* desired_addr;

        // Create a page mapping for pointing to its physical address with multiple virtual pages
        if (!AllocateTitlePhysicalPages(GetCurrentProcess(), MEM_LARGE_PAGES, &buffer->page_count,
                                        buffer->page_mapping))
        {
            free(buffer->page_mapping);
            buffer->page_mapping = NULL;
            break;
        }

        // Reserve two contiguous pages of virtual memory
        desired_addr = (rmtU8*)VirtualAlloc(0, size * 2, MEM_RESERVE, PAGE_NOACCESS);
        if (desired_addr == NULL)
            break;

        // Release the range immediately but retain the address for the next sequence of code to
        // try and map to it. In the mean-time some other OS thread may come along and allocate this
        // address range from underneath us so multiple attempts need to be made.
        VirtualFree(desired_addr, 0, MEM_RELEASE);

        // Immediately try to point both pages at the file mapping
        if (MapTitlePhysicalPages(desired_addr, buffer->page_count, MEM_LARGE_PAGES, PAGE_READWRITE,
                                  buffer->page_mapping) == desired_addr &&
            MapTitlePhysicalPages(desired_addr + size, buffer->page_count, MEM_LARGE_PAGES, PAGE_READWRITE,
                                  buffer->page_mapping) == desired_addr + size)
        {
            buffer->ptr = desired_addr;
            break;
        }

        // Failed to map the virtual pages; cleanup and try again
        FreeTitlePhysicalPages(GetCurrentProcess(), buffer->page_count, buffer->page_mapping);
        buffer->page_mapping = NULL;
    }

#else

    // Windows version based on https://gist.github.com/rygorous/3158316

    while (nb_attempts-- > 0)
    {
        rmtU8* desired_addr;

        // Create a file mapping for pointing to its physical address with multiple virtual pages
        buffer->file_map_handle = CreateFileMapping(INVALID_HANDLE_VALUE, 0, PAGE_READWRITE, 0, size, 0);
        if (buffer->file_map_handle == NULL)
            break;

#ifndef _UWP // NON-UWP Windows Desktop Version

        // Reserve two contiguous pages of virtual memory
        desired_addr = (rmtU8*)VirtualAlloc(0, size * 2, MEM_RESERVE, PAGE_NOACCESS);
        if (desired_addr == NULL)
            break;

        // Release the range immediately but retain the address for the next sequence of code to
        // try and map to it. In the mean-time some other OS thread may come along and allocate this
        // address range from underneath us so multiple attempts need to be made.
        VirtualFree(desired_addr, 0, MEM_RELEASE);

        // Immediately try to point both pages at the file mapping
        if (MapViewOfFileEx(buffer->file_map_handle, FILE_MAP_ALL_ACCESS, 0, 0, size, desired_addr) == desired_addr &&
            MapViewOfFileEx(buffer->file_map_handle, FILE_MAP_ALL_ACCESS, 0, 0, size, desired_addr + size) ==
                desired_addr + size)
        {
            buffer->ptr = desired_addr;
            break;
        }

#else // UWP

        // Implementation based on example from:
        // https://docs.microsoft.com/en-us/windows/desktop/api/memoryapi/nf-memoryapi-virtualalloc2
        //
        // Notes
        //  - just replaced the non-uwp functions by the uwp variants.
        //  - Both versions could be rewritten to not need the try-loop, see the example mentioned above. I just keep it
        //  as is for now.
        //  - Successfully tested on Hololens
        desired_addr = (rmtU8*)VirtualAlloc2FromApp(NULL, NULL, 2 * size, MEM_RESERVE | MEM_RESERVE_PLACEHOLDER,
                                                    PAGE_NOACCESS, NULL, 0);

        // Split the placeholder region into two regions of equal size.
        VirtualFree(desired_addr, size, MEM_RELEASE | MEM_PRESERVE_PLACEHOLDER);

        // Immediately try to point both pages at the file mapping.
        if (MapViewOfFile3FromApp(buffer->file_map_handle, NULL, desired_addr, 0, size, MEM_REPLACE_PLACEHOLDER,
                                  PAGE_READWRITE, NULL, 0) == desired_addr &&
            MapViewOfFile3FromApp(buffer->file_map_handle, NULL, desired_addr + size, 0, size, MEM_REPLACE_PLACEHOLDER,
                                  PAGE_READWRITE, NULL, 0) == desired_addr + size)
        {
            buffer->ptr = desired_addr;
            break;
        }
#endif
        // Failed to map the virtual pages; cleanup and try again
        CloseHandle(buffer->file_map_handle);
        buffer->file_map_handle = NULL;
    }

#endif // _XBOX_ONE

#endif

#ifdef RMT_PLATFORM_MACOS

    //
    // Mac version based on https://github.com/mikeash/MAMirroredQueue
    //
    // Copyright (c) 2010, Michael Ash
    // All rights reserved.
    //
    // Redistribution and use in source and binary forms, with or without modification, are permitted provided that
    // the following conditions are met:
    //
    // Redistributions of source code must retain the above copyright notice, this list of conditions and the following
    // disclaimer.
    //
    // Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
    // following disclaimer in the documentation and/or other materials provided with the distribution.
    // Neither the name of Michael Ash nor the names of its contributors may be used to endorse or promote products
    // derived from this software without specific prior written permission.
    //
    // THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
    // WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
    // PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
    // INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF
    // SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY
    // THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING
    // IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
    //

    while (nb_attempts-- > 0)
    {
        vm_prot_t cur_prot, max_prot;
        kern_return_t mach_error;
        rmtU8* ptr = NULL;
        rmtU8* target = NULL;

        // Allocate 2 contiguous pages of virtual memory
        if (vm_allocate(mach_task_self(), (vm_address_t*)&ptr, size * 2, VM_FLAGS_ANYWHERE) != KERN_SUCCESS)
            break;

        // Try to deallocate the last page, leaving its virtual memory address free
        target = ptr + size;
        if (vm_deallocate(mach_task_self(), (vm_address_t)target, size) != KERN_SUCCESS)
        {
            vm_deallocate(mach_task_self(), (vm_address_t)ptr, size * 2);
            break;
        }

        // Attempt to remap the page just deallocated to the buffer again
        mach_error = vm_remap(mach_task_self(), (vm_address_t*)&target, size,
                              0, // mask
                              0, // anywhere
                              mach_task_self(), (vm_address_t)ptr,
                              0, // copy
                              &cur_prot, &max_prot, VM_INHERIT_COPY);

        if (mach_error == KERN_NO_SPACE)
        {
            // Failed on this pass, cleanup and make another attempt
            if (vm_deallocate(mach_task_self(), (vm_address_t)ptr, size) != KERN_SUCCESS)
                break;
        }

        else if (mach_error == KERN_SUCCESS)
        {
            // Leave the loop on success
            buffer->ptr = ptr;
            break;
        }

        else
        {
            // Unknown error, can't recover
            vm_deallocate(mach_task_self(), (vm_address_t)ptr, size);
            break;
        }
    }

#endif

#ifdef RMT_PLATFORM_LINUX

    // Linux version based on now-defunct Wikipedia section
    // http://en.wikipedia.org/w/index.php?title=Circular_buffer&oldid=600431497

#ifdef __ANDROID__
    file_descriptor = ashmem_create_region("remotery_shm", size * 2);
    if (file_descriptor < 0)
    {
        return RMT_ERROR_VIRTUAL_MEMORY_BUFFER_FAIL;
    }
#else
    // Create a unique temporary filename in the shared memory folder
    file_descriptor = mkstemp(path);
    if (file_descriptor < 0)
        return RMT_ERROR_VIRTUAL_MEMORY_BUFFER_FAIL;

    // Delete the name
    if (unlink(path))
        return RMT_ERROR_VIRTUAL_MEMORY_BUFFER_FAIL;

    // Set the file size to twice the buffer size
    // TODO: this 2x behaviour can be avoided with similar solution to Win/Mac
    if (ftruncate(file_descriptor, size * 2))
        return RMT_ERROR_VIRTUAL_MEMORY_BUFFER_FAIL;

#endif
    // Map 2 contiguous pages
    buffer->ptr = (rmtU8*)mmap(NULL, size * 2, PROT_NONE, MAP_ANONYMOUS | MAP_PRIVATE, -1, 0);
    if (buffer->ptr == MAP_FAILED)
    {
        buffer->ptr = NULL;
        return RMT_ERROR_VIRTUAL_MEMORY_BUFFER_FAIL;
    }

    // Point both pages to the same memory file
    if (mmap(buffer->ptr, size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, file_descriptor, 0) != buffer->ptr ||
        mmap(buffer->ptr + size, size, PROT_READ | PROT_WRITE, MAP_FIXED | MAP_SHARED, file_descriptor, 0) !=
            buffer->ptr + size)
        return RMT_ERROR_VIRTUAL_MEMORY_BUFFER_FAIL;

#endif

    // Cleanup if exceeded number of attempts or failed
    if (buffer->ptr == NULL)
        return RMT_ERROR_VIRTUAL_MEMORY_BUFFER_FAIL;

    return RMT_ERROR_NONE;
}

static void VirtualMirrorBuffer_Destructor(VirtualMirrorBuffer* buffer)
{
    assert(buffer != 0);

#ifdef RMT_PLATFORM_WINDOWS
#ifdef _XBOX_ONE
    if (buffer->page_mapping != NULL)
    {
        VirtualFree(buffer->ptr, 0, MEM_DECOMMIT); // needed in conjunction with FreeTitlePhysicalPages
        FreeTitlePhysicalPages(GetCurrentProcess(), buffer->page_count, buffer->page_mapping);
        free(buffer->page_mapping);
        buffer->page_mapping = NULL;
    }
#else
    if (buffer->file_map_handle != NULL)
    {
        // FIXME, don't we need to unmap the file views obtained in VirtualMirrorBuffer_Constructor, both for
        // uwp/non-uwp See example
        // https://docs.microsoft.com/en-us/windows/desktop/api/memoryapi/nf-memoryapi-virtualalloc2

        CloseHandle(buffer->file_map_handle);
        buffer->file_map_handle = NULL;
    }
#endif
#endif

#ifdef RMT_PLATFORM_MACOS
    if (buffer->ptr != NULL)
        vm_deallocate(mach_task_self(), (vm_address_t)buffer->ptr, buffer->size * 2);
#endif

#ifdef RMT_PLATFORM_LINUX
    if (buffer->ptr != NULL)
        munmap(buffer->ptr, buffer->size * 2);
#endif

    buffer->ptr = NULL;
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @SAFEC: Safe C Library excerpts
   http://sourceforge.net/projects/safeclib/
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

/*------------------------------------------------------------------
 *
 * November 2008, Bo Berry
 *
 * Copyright (c) 2008-2011 by Cisco Systems, Inc
 * All rights reserved.
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the "Software"), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom the
 * Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall be
 * included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES
 * OF MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND
 * NONINFRINGEMENT.  IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT
 * HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR
 * OTHER DEALINGS IN THE SOFTWARE.
 *------------------------------------------------------------------
 */

// NOTE: Microsoft also has its own version of these functions so I'm do some hacky PP to remove them
#define strnlen_s strnlen_s_safe_c
#define strncat_s strncat_s_safe_c
#define strcpy_s strcpy_s_safe_c

#define RSIZE_MAX_STR (4UL << 10) /* 4KB */
#define RCNEGATE(x) x

#define EOK (0)
#define ESNULLP (400) /* null ptr                    */
#define ESZEROL (401) /* length is zero              */
#define ESLEMAX (403) /* length exceeds max          */
#define ESOVRLP (404) /* overlap undefined           */
#define ESNOSPC (406) /* not enough space for s2     */
#define ESUNTERM (407) /* unterminated string         */
#define ESNOTFND (409) /* not found                   */

#ifndef _ERRNO_T_DEFINED
#define _ERRNO_T_DEFINED
typedef int errno_t;
#endif

// rsize_t equivalent without going to the hassle of detecting if a platform has implemented C11/K3.2
typedef unsigned int r_size_t;

static r_size_t strnlen_s(const char* dest, r_size_t dmax)
{
    r_size_t count;

    if (dest == NULL)
    {
        return RCNEGATE(0);
    }

    if (dmax == 0)
    {
        return RCNEGATE(0);
    }

    if (dmax > RSIZE_MAX_STR)
    {
        return RCNEGATE(0);
    }

    count = 0;
    while (*dest && dmax)
    {
        count++;
        dmax--;
        dest++;
    }

    return RCNEGATE(count);
}

static errno_t strstr_s(char* dest, r_size_t dmax, const char* src, r_size_t slen, char** substring)
{
    r_size_t len;
    r_size_t dlen;
    int i;

    if (substring == NULL)
    {
        return RCNEGATE(ESNULLP);
    }
    *substring = NULL;

    if (dest == NULL)
    {
        return RCNEGATE(ESNULLP);
    }

    if (dmax == 0)
    {
        return RCNEGATE(ESZEROL);
    }

    if (dmax > RSIZE_MAX_STR)
    {
        return RCNEGATE(ESLEMAX);
    }

    if (src == NULL)
    {
        return RCNEGATE(ESNULLP);
    }

    if (slen == 0)
    {
        return RCNEGATE(ESZEROL);
    }

    if (slen > RSIZE_MAX_STR)
    {
        return RCNEGATE(ESLEMAX);
    }

    /*
     * src points to a string with zero length, or
     * src equals dest, return dest
     */
    if (*src == '\0' || dest == src)
    {
        *substring = dest;
        return RCNEGATE(EOK);
    }

    while (*dest && dmax)
    {
        i = 0;
        len = slen;
        dlen = dmax;

        while (src[i] && dlen)
        {

            /* not a match, not a substring */
            if (dest[i] != src[i])
            {
                break;
            }

            /* move to the next char */
            i++;
            len--;
            dlen--;

            if (src[i] == '\0' || !len)
            {
                *substring = dest;
                return RCNEGATE(EOK);
            }
        }
        dest++;
        dmax--;
    }

    /*
     * substring was not found, return NULL
     */
    *substring = NULL;
    return RCNEGATE(ESNOTFND);
}

static errno_t strncat_s(char* dest, r_size_t dmax, const char* src, r_size_t slen)
{
    const char* overlap_bumper;

    if (dest == NULL)
    {
        return RCNEGATE(ESNULLP);
    }

    if (src == NULL)
    {
        return RCNEGATE(ESNULLP);
    }

    if (slen > RSIZE_MAX_STR)
    {
        return RCNEGATE(ESLEMAX);
    }

    if (dmax == 0)
    {
        return RCNEGATE(ESZEROL);
    }

    if (dmax > RSIZE_MAX_STR)
    {
        return RCNEGATE(ESLEMAX);
    }

    /* hold base of dest in case src was not copied */

    if (dest < src)
    {
        overlap_bumper = src;

        /* Find the end of dest */
        while (*dest != '\0')
        {

            if (dest == overlap_bumper)
            {
                return RCNEGATE(ESOVRLP);
            }

            dest++;
            dmax--;
            if (dmax == 0)
            {
                return RCNEGATE(ESUNTERM);
            }
        }

        while (dmax > 0)
        {
            if (dest == overlap_bumper)
            {
                return RCNEGATE(ESOVRLP);
            }

            /*
             * Copying truncated before the source null is encountered
             */
            if (slen == 0)
            {
                *dest = '\0';
                return RCNEGATE(EOK);
            }

            *dest = *src;
            if (*dest == '\0')
            {
                return RCNEGATE(EOK);
            }

            dmax--;
            slen--;
            dest++;
            src++;
        }
    }
    else
    {
        overlap_bumper = dest;

        /* Find the end of dest */
        while (*dest != '\0')
        {

            /*
             * NOTE: no need to check for overlap here since src comes first
             * in memory and we're not incrementing src here.
             */
            dest++;
            dmax--;
            if (dmax == 0)
            {
                return RCNEGATE(ESUNTERM);
            }
        }

        while (dmax > 0)
        {
            if (src == overlap_bumper)
            {
                return RCNEGATE(ESOVRLP);
            }

            /*
             * Copying truncated
             */
            if (slen == 0)
            {
                *dest = '\0';
                return RCNEGATE(EOK);
            }

            *dest = *src;
            if (*dest == '\0')
            {
                return RCNEGATE(EOK);
            }

            dmax--;
            slen--;
            dest++;
            src++;
        }
    }

    /*
     * the entire src was not copied, so the string will be nulled.
     */
    return RCNEGATE(ESNOSPC);
}

errno_t strcpy_s(char* dest, r_size_t dmax, const char* src)
{
    const char* overlap_bumper;

    if (dest == NULL)
    {
        return RCNEGATE(ESNULLP);
    }

    if (dmax == 0)
    {
        return RCNEGATE(ESZEROL);
    }

    if (dmax > RSIZE_MAX_STR)
    {
        return RCNEGATE(ESLEMAX);
    }

    if (src == NULL)
    {
        *dest = '\0';
        return RCNEGATE(ESNULLP);
    }

    if (dest == src)
    {
        return RCNEGATE(EOK);
    }

    if (dest < src)
    {
        overlap_bumper = src;

        while (dmax > 0)
        {
            if (dest == overlap_bumper)
            {
                return RCNEGATE(ESOVRLP);
            }

            *dest = *src;
            if (*dest == '\0')
            {
                return RCNEGATE(EOK);
            }

            dmax--;
            dest++;
            src++;
        }
    }
    else
    {
        overlap_bumper = dest;

        while (dmax > 0)
        {
            if (src == overlap_bumper)
            {
                return RCNEGATE(ESOVRLP);
            }

            *dest = *src;
            if (*dest == '\0')
            {
                return RCNEGATE(EOK);
            }

            dmax--;
            dest++;
            src++;
        }
    }

    /*
     * the entire src must have been copied, if not reset dest
     * to null the string.
     */
    return RCNEGATE(ESNOSPC);
}

/* very simple integer to hex */
static const char* hex_encoding_table = "0123456789ABCDEF";

static void itoahex_s(char* dest, r_size_t dmax, rmtS32 value)
{
    r_size_t len;
    rmtS32 halfbytepos;

    halfbytepos = 8;

    /* strip leading 0's */
    while (halfbytepos > 1)
    {
        --halfbytepos;
        if (value >> (4 * halfbytepos) & 0xF)
        {
            ++halfbytepos;
            break;
        }
    }

    len = 0;
    while (len + 1 < dmax && halfbytepos > 0)
    {
        --halfbytepos;
        dest[len] = hex_encoding_table[value >> (4 * halfbytepos) & 0xF];
        ++len;
    }

    if (len < dmax)
    {
        dest[len] = 0;
    }
}

static const char* itoa_s(rmtS32 value)
{
    static char temp_dest[12] = { 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0 };
    int pos = 10;

    // Work back with the absolute value
    rmtS32 abs_value = abs(value);
    while (abs_value > 0)
    {
        temp_dest[pos--] = '0' + (abs_value % 10);
        abs_value /= 10;
    }

    // Place the negative
    if (value < 0)
    {
        temp_dest[pos--] = '-';
    }

    return temp_dest + pos + 1;
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @OSTHREADS: Wrappers around OS-specific thread functions
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

typedef rmtU64 rmtThreadId;

#ifdef RMT_PLATFORM_WINDOWS
typedef HANDLE rmtThreadHandle;
#else
typedef pthread_t rmtThreadHandle;
#endif

#ifdef RMT_PLATFORM_WINDOWS
typedef CONTEXT rmtCpuContext;
#else
typedef int rmtCpuContext;
#endif

static rmtU32 rmtGetNbProcessors()
{
#ifdef RMT_PLATFORM_WINDOWS
    SYSTEM_INFO system_info;
    GetSystemInfo(&system_info);
    return system_info.dwNumberOfProcessors;
#else
    // TODO: get_nprocs_conf / get_nprocs
    return 0;
#endif
}

static rmtThreadId rmtGetCurrentThreadId()
{
#ifdef RMT_PLATFORM_WINDOWS
    return GetCurrentThreadId();
#else
    return (rmtThreadId)pthread_self();
#endif
}

static rmtBool rmtSuspendThread(rmtThreadHandle thread_handle)
{
#ifdef RMT_PLATFORM_WINDOWS
    // SuspendThread is an async call to the scheduler and upon return the thread is not guaranteed to be suspended.
    // Calling GetThreadContext will serialise that.
    // See: https://github.com/mono/mono/blob/master/mono/utils/mono-threads-windows.c#L203
    return SuspendThread(thread_handle) == 0 ? RMT_TRUE : RMT_FALSE;
#else
    return RMT_FALSE;
#endif
}

static void rmtResumeThread(rmtThreadHandle thread_handle)
{
#ifdef RMT_PLATFORM_WINDOWS
    ResumeThread(thread_handle);
#endif
}

#ifdef RMT_PLATFORM_WINDOWS
#ifndef CONTEXT_EXCEPTION_REQUEST
// These seem to be guarded by a _AMD64_ macro in winnt.h, which doesn't seem to be defined in older MSVC compilers.
// Which makes sense given this was a post-Vista/Windows 7 patch around errors in the WoW64 context switch.
// This bug was never fixed in the OS so defining these will only get this code to compile on Old Windows systems, with no
// guarantee of being stable at runtime.
#define CONTEXT_EXCEPTION_ACTIVE 0x8000000L
#define CONTEXT_SERVICE_ACTIVE 0x10000000L
#define CONTEXT_EXCEPTION_REQUEST 0x40000000L
#define CONTEXT_EXCEPTION_REPORTING 0x80000000L
#endif
#endif

static rmtBool rmtGetUserModeThreadContext(rmtThreadHandle thread, rmtCpuContext* context)
{
#ifdef RMT_PLATFORM_WINDOWS
    DWORD kernel_mode_mask;

    // Request thread context with exception reporting
    context->ContextFlags = CONTEXT_CONTROL | CONTEXT_INTEGER | CONTEXT_EXCEPTION_REQUEST;
    if (GetThreadContext(thread, context) == 0)
    {
        return RMT_FALSE;
    }

    // Context on WoW64 is only valid and can only be set if the thread isn't in kernel mode
    // Typical reference to this appears to be: http://zachsaw.blogspot.com/2010/11/wow64-bug-getthreadcontext-may-return.html
    // Confirmed by MS here: https://social.msdn.microsoft.com/Forums/vstudio/en-US/aa176c36-6624-4776-9380-1c9cf37a314e/getthreadcontext-returns-stale-register-values-on-wow64?forum=windowscompatibility
    kernel_mode_mask = CONTEXT_EXCEPTION_REPORTING | CONTEXT_EXCEPTION_ACTIVE | CONTEXT_SERVICE_ACTIVE;
    return (context->ContextFlags & kernel_mode_mask) == CONTEXT_EXCEPTION_REPORTING ? RMT_TRUE : RMT_FALSE;
#else
    return RMT_FALSE;
#endif
}

static void rmtSetThreadContext(rmtThreadHandle thread_handle, rmtCpuContext* context)
{
#ifdef RMT_PLATFORM_WINDOWS
    SetThreadContext(thread_handle, context);
#endif
}

static rmtError rmtOpenThreadHandle(rmtThreadId thread_id, rmtThreadHandle* out_thread_handle)
{
#ifdef RMT_PLATFORM_WINDOWS
    // Open the thread with required access rights to get the thread handle
    *out_thread_handle = OpenThread(THREAD_QUERY_INFORMATION | THREAD_SUSPEND_RESUME | THREAD_SET_CONTEXT | THREAD_GET_CONTEXT, FALSE, thread_id);
    if (*out_thread_handle == NULL)
    {
        return RMT_ERROR_OPEN_THREAD_HANDLE_FAIL;
    }
#endif

    return RMT_ERROR_NONE;
}

static void rmtCloseThreadHandle(rmtThreadHandle thread_handle)
{
#ifdef RMT_PLATFORM_WINDOWS
    if (thread_handle != NULL)
    {
        CloseHandle(thread_handle);
    }
#endif
}

#ifdef RMT_PLATFORM_WINDOWS
DWORD_PTR GetThreadStartAddress(rmtThreadHandle thread_handle)
{
    // Get NtQueryInformationThread from ntdll
    HMODULE ntdll = GetModuleHandleA("ntdll.dll");
    if (ntdll != NULL)
    {
        typedef NTSTATUS (WINAPI *NTQUERYINFOMATIONTHREAD)(HANDLE, LONG, PVOID, ULONG, PULONG);
        NTQUERYINFOMATIONTHREAD NtQueryInformationThread = (NTQUERYINFOMATIONTHREAD)GetProcAddress(ntdll, "NtQueryInformationThread");

        // Use it to query the start address
        DWORD_PTR start_address;
        NTSTATUS status = NtQueryInformationThread(thread_handle, 9, &start_address, sizeof(DWORD), NULL);
        if (status == 0)
        {
            return start_address;
        }
    }

    return 0;
}

const char* GetStartAddressModuleName(DWORD_PTR start_address)
{
    BOOL success;
    MODULEENTRY32 module_entry;

    // Snapshot the modules
    HANDLE handle = CreateToolhelp32Snapshot(TH32CS_SNAPMODULE, 0);
    if (handle == INVALID_HANDLE_VALUE)
    {
        return NULL;
    }

    module_entry.dwSize = sizeof(MODULEENTRY32);
    module_entry.th32ModuleID = 1;

    // Enumerate modules checking start address against their loaded address range
    success = Module32First(handle, &module_entry);
    while (success == TRUE)
    {
        if (start_address >= (DWORD_PTR)module_entry.modBaseAddr && start_address <= ((DWORD_PTR)module_entry.modBaseAddr + module_entry.modBaseSize))
        {
            static char name[MAX_MODULE_NAME32 + 1];
#ifdef UNICODE
            int size = WideCharToMultiByte(CP_ACP, 0, module_entry.szModule, -1, name, MAX_MODULE_NAME32, NULL, NULL);
            if (size < 1)
            {
                name[0] = '\0';
            }
#else
            strcpy_s(name, sizeof(name), module_entry.szModule);
#endif
            CloseHandle(handle);
            return name;
        }

        success = Module32Next(handle, &module_entry);
    }

    CloseHandle(handle);

    return NULL;
}
#endif

static void rmtGetThreadNameFallback(char* out_thread_name, rmtU32 thread_name_size)
{
    // In cases where we can't get a thread name from the OS
    static rmtS32 countThreads = 0;
    out_thread_name[0] = 0;
    strncat_s(out_thread_name, thread_name_size, "Thread", 6);
    itoahex_s(out_thread_name + 6, thread_name_size - 6, AtomicAdd(&countThreads, 1));
}

static void rmtGetThreadName(rmtThreadId thread_id, rmtThreadHandle thread_handle, char* out_thread_name, rmtU32 thread_name_size)
{
#ifdef RMT_PLATFORM_WINDOWS
    DWORD_PTR address;
    const char* module_name;
    rmtU32 len;

    // Use the new Windows 10 GetThreadDescription function
    HMODULE kernel32 = GetModuleHandleA("Kernel32.dll");
    if (kernel32 != NULL)
    {
        typedef HRESULT(WINAPI* GETTHREADDESCRIPTION)(HANDLE hThread, PWSTR *ppszThreadDescription);
        GETTHREADDESCRIPTION GetThreadDescription = (GETTHREADDESCRIPTION)GetProcAddress(kernel32, "GetThreadDescription");
        if (GetThreadDescription != NULL)
        {
            int size;

            WCHAR* thread_name_w;
            GetThreadDescription(thread_handle, &thread_name_w);

            // Returned size is the byte size, so will be 1 for a null-terminated strings
            size = WideCharToMultiByte(CP_ACP, 0, thread_name_w, -1, out_thread_name, thread_name_size, NULL, NULL);
            if (size > 1)
            {
                return;
            }
        }
    }

    // At this point GetThreadDescription hasn't returned anything so let's get the thread module name and use that
    address = GetThreadStartAddress(thread_handle);
    if (address == 0)
    {
        rmtGetThreadNameFallback(out_thread_name, thread_name_size);
        return;
    }
    module_name = GetStartAddressModuleName(address);
    if (module_name == NULL)
    {
        rmtGetThreadNameFallback(out_thread_name, thread_name_size);
        return;
    }

    // Concatenate thread name with then thread ID as that will be unique, whereas the start address won't be
    memset(out_thread_name, 0, thread_name_size);
    strcpy_s(out_thread_name, thread_name_size, module_name);
    strncat_s(out_thread_name, thread_name_size, "!", 1);
    len = strnlen_s(out_thread_name, thread_name_size);
    itoahex_s(out_thread_name + len, thread_name_size - len, thread_id);

#elif defined(RMT_PLATFORM_LINUX) && RMT_USE_POSIX_THREADNAMES && !defined(__FreeBSD__) && !defined(__OpenBSD__)

    prctl(PR_GET_NAME, out_thread_name, 0, 0, 0);

#else

    rmtGetThreadNameFallback(out_thread_name, thread_name_size);

#endif
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @THREADS: Cross-platform thread object
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

typedef struct Thread_t rmtThread;
typedef rmtError (*ThreadProc)(rmtThread* thread);

struct Thread_t
{
    rmtThreadHandle handle;

    // Callback executed when the thread is created
    ThreadProc callback;

    // Caller-specified parameter passed to Thread_Create
    void* param;

    // Error state returned from callback
    rmtError error;

    // External threads can set this to request an exit
    volatile rmtBool request_exit;
};

#if defined(RMT_PLATFORM_WINDOWS)

static DWORD WINAPI ThreadProcWindows(LPVOID lpParameter)
{
    rmtThread* thread = (rmtThread*)lpParameter;
    assert(thread != NULL);
    thread->error = thread->callback(thread);
    return thread->error == RMT_ERROR_NONE ? 0 : 1;
}

#else
static void* StartFunc(void* pArgs)
{
    rmtThread* thread = (rmtThread*)pArgs;
    assert(thread != NULL);
    thread->error = thread->callback(thread);
    return NULL; // returned error not use, check thread->error.
}
#endif

static int rmtThread_Valid(rmtThread* thread)
{
    assert(thread != NULL);

#if defined(RMT_PLATFORM_WINDOWS)
    return thread->handle != NULL;
#else
    return !pthread_equal(thread->handle, pthread_self());
#endif
}

static rmtError rmtThread_Constructor(rmtThread* thread, ThreadProc callback, void* param)
{
    assert(thread != NULL);

    thread->callback = callback;
    thread->param = param;
    thread->error = RMT_ERROR_NONE;
    thread->request_exit = RMT_FALSE;

    // OS-specific thread creation

#if defined(RMT_PLATFORM_WINDOWS)

    thread->handle = CreateThread(NULL,              // lpThreadAttributes
                                  0,                 // dwStackSize
                                  ThreadProcWindows, // lpStartAddress
                                  thread,            // lpParameter
                                  0,                 // dwCreationFlags
                                  NULL);             // lpThreadId

    if (thread->handle == NULL)
        return RMT_ERROR_CREATE_THREAD_FAIL;

#else

    int32_t error = pthread_create(&thread->handle, NULL, StartFunc, thread);
    if (error)
    {
        // Contents of 'thread' parameter to pthread_create() are undefined after
        // failure call so can't pre-set to invalid value before hand.
        thread->handle = pthread_self();
        return RMT_ERROR_CREATE_THREAD_FAIL;
    }

#endif

    return RMT_ERROR_NONE;
}

static void rmtThread_RequestExit(rmtThread* thread)
{
    // Not really worried about memory barriers or delayed visibility to the target thread
    assert(thread != NULL);
    thread->request_exit = RMT_TRUE;
}

static void rmtThread_Join(rmtThread* thread)
{
    assert(rmtThread_Valid(thread));

#if defined(RMT_PLATFORM_WINDOWS)
    WaitForSingleObject(thread->handle, INFINITE);
#else
    pthread_join(thread->handle, NULL);
#endif
}

static void rmtThread_Destructor(rmtThread* thread)
{
    assert(thread != NULL);

    if (rmtThread_Valid(thread))
    {
        // Shutdown the thread
        rmtThread_RequestExit(thread);
        rmtThread_Join(thread);

        // OS-specific release of thread resources

#if defined(RMT_PLATFORM_WINDOWS)
        CloseHandle(thread->handle);
        thread->handle = NULL;
#endif
    }
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @OBJALLOC: Reusable Object Allocator
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

//
// All objects that require free-list-backed allocation need to inherit from this type.
//
typedef struct ObjectLink_s
{
    struct ObjectLink_s* volatile next;
} ObjectLink;

static void ObjectLink_Constructor(ObjectLink* link)
{
    assert(link != NULL);
    link->next = NULL;
}

typedef rmtError (*ObjConstructor)(void*);
typedef void (*ObjDestructor)(void*);

typedef struct
{
    // Object create/destroy parameters
    rmtU32 object_size;
    ObjConstructor constructor;
    ObjDestructor destructor;

    // Number of objects in the free list
    volatile rmtS32 nb_free;

    // Number of objects used by callers
    volatile rmtS32 nb_inuse;

    // Total allocation count
    volatile rmtS32 nb_allocated;

    ObjectLink* first_free;
} ObjectAllocator;

static rmtError ObjectAllocator_Constructor(ObjectAllocator* allocator, rmtU32 object_size, ObjConstructor constructor,
                                            ObjDestructor destructor)
{
    allocator->object_size = object_size;
    allocator->constructor = constructor;
    allocator->destructor = destructor;
    allocator->nb_free = 0;
    allocator->nb_inuse = 0;
    allocator->nb_allocated = 0;
    allocator->first_free = NULL;
    return RMT_ERROR_NONE;
}

static void ObjectAllocator_Destructor(ObjectAllocator* allocator)
{
    // Ensure everything has been released to the allocator
    assert(allocator != NULL);
    assert(allocator->nb_inuse == 0);

    // Destroy all objects released to the allocator
    while (allocator->first_free != NULL)
    {
        ObjectLink* next = allocator->first_free->next;
        assert(allocator->destructor != NULL);
        allocator->destructor(allocator->first_free);
        rmtFree(allocator->first_free);
        allocator->first_free = next;
    }
}

static void ObjectAllocator_Push(ObjectAllocator* allocator, ObjectLink* start, ObjectLink* end)
{
    assert(allocator != NULL);
    assert(start != NULL);
    assert(end != NULL);

    // CAS pop add range to the front of the list
    for (;;)
    {
        ObjectLink* old_link = (ObjectLink*)allocator->first_free;
        end->next = old_link;
        if (AtomicCompareAndSwapPointer((long* volatile*)&allocator->first_free, (long*)old_link, (long*)start) ==
            RMT_TRUE)
            break;
    }
}

static ObjectLink* ObjectAllocator_Pop(ObjectAllocator* allocator)
{
    ObjectLink* link;

    assert(allocator != NULL);

    // CAS pop from the front of the list
    for (;;)
    {
        ObjectLink* old_link = (ObjectLink*)allocator->first_free;
        if (old_link == NULL)
        {
            return NULL;
        }
        ObjectLink* next_link = old_link->next;
        if (AtomicCompareAndSwapPointer((long* volatile*)&allocator->first_free, (long*)old_link, (long*)next_link) ==
            RMT_TRUE)
        {
            link = old_link;
            break;
        }
    }

    link->next = NULL;

    return link;
}

static rmtError ObjectAllocator_Alloc(ObjectAllocator* allocator, void** object)
{
    // This function only calls the object constructor on initial malloc of an object

    assert(allocator != NULL);
    assert(object != NULL);

    // Pull available objects from the free list
    *object = ObjectAllocator_Pop(allocator);

    // Has the free list run out?
    if (*object == NULL)
    {
        rmtError error;

        // Allocate/construct a new object
        *object = rmtMalloc(allocator->object_size);
        if (*object == NULL)
            return RMT_ERROR_MALLOC_FAIL;
        assert(allocator->constructor != NULL);
        error = allocator->constructor(*object);
        if (error != RMT_ERROR_NONE)
        {
            // Auto-teardown on failure
            assert(allocator->destructor != NULL);
            allocator->destructor(*object);
            rmtFree(*object);
            return error;
        }

        AtomicAdd(&allocator->nb_allocated, 1);
    }
    else
    {
        AtomicSub(&allocator->nb_free, 1);
    }

    AtomicAdd(&allocator->nb_inuse, 1);

    return RMT_ERROR_NONE;
}

static void ObjectAllocator_Free(ObjectAllocator* allocator, void* object)
{
    // Add back to the free-list
    assert(allocator != NULL);
    ObjectAllocator_Push(allocator, (ObjectLink*)object, (ObjectLink*)object);
    AtomicSub(&allocator->nb_inuse, 1);
    AtomicAdd(&allocator->nb_free, 1);
}

static void ObjectAllocator_FreeRange(ObjectAllocator* allocator, void* start, void* end, rmtU32 count)
{
    assert(allocator != NULL);
    ObjectAllocator_Push(allocator, (ObjectLink*)start, (ObjectLink*)end);
    AtomicSub(&allocator->nb_inuse, count);
    AtomicAdd(&allocator->nb_free, count);
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @DYNBUF: Dynamic Buffer
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

typedef struct
{
    rmtU32 alloc_granularity;

    rmtU32 bytes_allocated;
    rmtU32 bytes_used;

    rmtU8* data;
} Buffer;

static rmtError Buffer_Constructor(Buffer* buffer, rmtU32 alloc_granularity)
{
    assert(buffer != NULL);
    buffer->alloc_granularity = alloc_granularity;
    buffer->bytes_allocated = 0;
    buffer->bytes_used = 0;
    buffer->data = NULL;
    return RMT_ERROR_NONE;
}

static void Buffer_Destructor(Buffer* buffer)
{
    assert(buffer != NULL);

    if (buffer->data != NULL)
    {
        rmtFree(buffer->data);
        buffer->data = NULL;
    }
}

static rmtError Buffer_Grow(Buffer* buffer, rmtU32 length)
{
    // Calculate size increase rounded up to the requested allocation granularity
    rmtU32 granularity = buffer->alloc_granularity;
    rmtU32 allocate = buffer->bytes_allocated + length;
    allocate = allocate + ((granularity - 1) - ((allocate - 1) % granularity));

    buffer->bytes_allocated = allocate;
    buffer->data = (rmtU8*)rmtRealloc(buffer->data, buffer->bytes_allocated);
    if (buffer->data == NULL)
        return RMT_ERROR_MALLOC_FAIL;

    return RMT_ERROR_NONE;
}

static rmtError Buffer_Write(Buffer* buffer, const void* data, rmtU32 length)
{
    assert(buffer != NULL);

    // Reallocate the buffer on overflow
    if (buffer->bytes_used + length > buffer->bytes_allocated)
    {
        rmtError error = Buffer_Grow(buffer, length);
        if (error != RMT_ERROR_NONE)
            return error;
    }

    // Copy all bytes
    memcpy(buffer->data + buffer->bytes_used, data, length);
    buffer->bytes_used += length;

    return RMT_ERROR_NONE;
}

static rmtError Buffer_WriteStringZ(Buffer* buffer, rmtPStr string)
{
    assert(string != NULL);
    return Buffer_Write(buffer, (void*)string, (rmtU32)strnlen_s(string, 2048) + 1);
}

static void U32ToByteArray(rmtU8* dest, rmtU32 value)
{
    // Commit as little-endian
    dest[0] = value & 255;
    dest[1] = (value >> 8) & 255;
    dest[2] = (value >> 16) & 255;
    dest[3] = value >> 24;
}

static rmtError Buffer_WriteU32(Buffer* buffer, rmtU32 value)
{
    assert(buffer != NULL);

    // Reallocate the buffer on overflow
    if (buffer->bytes_used + sizeof(value) > buffer->bytes_allocated)
    {
        rmtError error = Buffer_Grow(buffer, sizeof(value));
        if (error != RMT_ERROR_NONE)
            return error;
    }

// Copy all bytes
#if RMT_ASSUME_LITTLE_ENDIAN
    *(rmtU32*)(buffer->data + buffer->bytes_used) = value;
#else
    U32ToByteArray(buffer->data + buffer->bytes_used, value);
#endif

    buffer->bytes_used += sizeof(value);

    return RMT_ERROR_NONE;
}

static rmtBool IsLittleEndian()
{
    // Not storing this in a global variable allows the compiler to more easily optimise
    // this away altogether.
    union {
        unsigned int i;
        unsigned char c[sizeof(unsigned int)];
    } u;
    u.i = 1;
    return u.c[0] == 1 ? RMT_TRUE : RMT_FALSE;
}

static rmtError Buffer_WriteU64(Buffer* buffer, rmtU64 value)
{
    // Write as a double as Javascript DataView doesn't have a 64-bit integer read

    assert(buffer != NULL);

    // Reallocate the buffer on overflow
    if (buffer->bytes_used + sizeof(value) > buffer->bytes_allocated)
    {
        rmtError error = Buffer_Grow(buffer, sizeof(value));
        if (error != RMT_ERROR_NONE)
            return error;
    }

// Copy all bytes
#if RMT_ASSUME_LITTLE_ENDIAN
    *(double*)(buffer->data + buffer->bytes_used) = (double)value;
#else
    {
        union {
            double d;
            unsigned char c[sizeof(double)];
        } u;
        rmtU8* dest = buffer->data + buffer->bytes_used;
        u.d = (double)value;
        if (IsLittleEndian())
        {
            dest[0] = u.c[0];
            dest[1] = u.c[1];
            dest[2] = u.c[2];
            dest[3] = u.c[3];
            dest[4] = u.c[4];
            dest[5] = u.c[5];
            dest[6] = u.c[6];
            dest[7] = u.c[7];
        }
        else
        {
            dest[0] = u.c[7];
            dest[1] = u.c[6];
            dest[2] = u.c[5];
            dest[3] = u.c[4];
            dest[4] = u.c[3];
            dest[5] = u.c[2];
            dest[6] = u.c[1];
            dest[7] = u.c[0];
        }
    }
#endif

    buffer->bytes_used += sizeof(value);

    return RMT_ERROR_NONE;
}

static rmtError Buffer_WriteStringWithLength(Buffer* buffer, rmtPStr string)
{
    rmtU32 length = (rmtU32)strnlen_s(string, 2048);
    rmtError error;

    error = Buffer_WriteU32(buffer, length);
    if (error != RMT_ERROR_NONE)
        return error;

    return Buffer_Write(buffer, (void*)string, length);
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @HASHTABLE: Integer pair hash map for inserts/finds. No removes for added simplicity.
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

#define RMT_NOT_FOUND 0xffffffffffffffff

typedef struct
{
    // Non-zero, pre-hashed key
    rmtU32 key;

    // Value that's not equal to RMT_NOT_FOUND
    rmtU64 value;
} HashSlot;

typedef struct
{
    // Stats
    rmtU32 maxNbSlots;
    rmtU32 nbSlots;

    // Data
    HashSlot* slots;
} rmtHashTable;

static rmtError rmtHashTable_Constructor(rmtHashTable* table, rmtU32 max_nb_slots)
{
    // Default initialise
    assert(table != NULL);
    table->maxNbSlots = max_nb_slots;
    table->nbSlots = 0;

    // Allocate and clear the hash slots
    table->slots = (HashSlot*)rmtMalloc(table->maxNbSlots * sizeof(HashSlot));
    if (table->slots == NULL)
    {
        return RMT_ERROR_MALLOC_FAIL;
    }
    memset(table->slots, 0, table->maxNbSlots * sizeof(HashSlot));

    return RMT_ERROR_NONE;
}

static void rmtHashTable_Destructor(rmtHashTable* table)
{
    assert(table != NULL);

    if (table->slots != NULL)
    {
        rmtFree(table->slots);
        table->slots = NULL;
    }
}

static rmtError rmtHashTable_Resize(rmtHashTable* table);

static rmtError rmtHashTable_Insert(rmtHashTable* table, rmtU32 key, rmtU64 value)
{
    HashSlot* slot = NULL;
    rmtError error = RMT_ERROR_NONE;

    // Calculate initial slot location for this key
    rmtU32 index_mask = table->maxNbSlots - 1;
    rmtU32 index = key & index_mask;

    assert(key != 0);
    assert(value != RMT_NOT_FOUND);

    // Linear probe for free slot, reusing any existing key matches
    // There will always be at least one free slot due to load factor management
    while (table->slots[index].key)
    {
        if (table->slots[index].key == key)
        {
            // Counter occupied slot increments below
            table->nbSlots--;
            break;
        }

        index = (index + 1) & index_mask;
    }

    // Just verify that I've got no errors in the code above
    assert(index < table->maxNbSlots);

    // Add to the table
    slot = table->slots + index;
    slot->key = key;
    slot->value = value;
    table->nbSlots++;

    // Resize when load factor is greater than 2/3
    if (table->nbSlots > (table->maxNbSlots * 2) / 3)
    {
        error = rmtHashTable_Resize(table);
    }

    return error;
}

static rmtError rmtHashTable_Resize(rmtHashTable* table)
{
    rmtU32 old_max_nb_slots = table->maxNbSlots;
    HashSlot* new_slots = NULL;
    HashSlot* old_slots = table->slots;
    rmtU32 i;

    // Increase the table size
    rmtU32 new_max_nb_slots = table->maxNbSlots;
    if (new_max_nb_slots < 8192 * 4)
    {
        new_max_nb_slots *= 4;
    }
    else
    {
        new_max_nb_slots *= 2;
    }

    // Allocate and clear a new table
    new_slots = (HashSlot*)rmtMalloc(new_max_nb_slots * sizeof(HashSlot));
    if (new_slots == NULL)
    {
        return RMT_ERROR_MALLOC_FAIL;
    }
    memset(new_slots, 0, new_max_nb_slots * sizeof(HashSlot));

    // Update fields of the table after successful allocation only
    table->slots = new_slots;
    table->maxNbSlots = new_max_nb_slots;
    table->nbSlots = 0;

    // Reinsert all objects into the new table
    for (i = 0; i < old_max_nb_slots; i++)
    {
        HashSlot* slot = old_slots + i;
        if (slot->key != 0)
        {
            rmtHashTable_Insert(table, slot->key, slot->value);
        }
    }

    rmtFree(old_slots);

    return RMT_ERROR_NONE;
}

static rmtU64 rmtHashTable_Find(rmtHashTable* table, rmtU32 key)
{
    // Calculate initial slot location for this key
    rmtU32 index_mask = table->maxNbSlots - 1;
    rmtU32 index = key & index_mask;

    // Linear probe for matching hash
    while (table->slots[index].key)
    {
        HashSlot* slot = table->slots + index;

        if (slot->key == key)
        {
            return slot->value;
        }

        index = (index + 1) & index_mask;
    }

    return RMT_NOT_FOUND;
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @STRINGTABLE: Map from string hash to string offset in local buffer
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

typedef struct
{
    // Growable dynamic array of strings added so far
    Buffer* text;

    // Map from text hash to text location in the buffer
    rmtHashTable* text_map;
} StringTable;

static rmtError StringTable_Constructor(StringTable* table)
{
    rmtError error;

    // Default initialise
    assert(table != NULL);
    table->text = NULL;
    table->text_map = NULL;

    // Allocate reasonably storage for initial sample names

    New_1(Buffer, table->text, 8 * 1024);
    if (error != RMT_ERROR_NONE)
        return error;

    New_1(rmtHashTable, table->text_map, 1 * 1024);
    if (error != RMT_ERROR_NONE)
        return error;

    return RMT_ERROR_NONE;
}

static void StringTable_Destructor(StringTable* table)
{
    assert(table != NULL);

    Delete(rmtHashTable, table->text_map);
    Delete(Buffer, table->text);
}

static rmtPStr StringTable_Find(StringTable* table, rmtU32 name_hash)
{
    rmtU64 text_offset = rmtHashTable_Find(table->text_map, name_hash);
    if (text_offset != RMT_NOT_FOUND)
    {
        return (rmtPStr)(table->text->data + text_offset);
    }
    return NULL;
}

static void StringTable_Insert(StringTable* table, rmtU32 name_hash, rmtPStr name)
{
    // Only add to the buffer if the string isn't already there
    rmtU64 text_offset = rmtHashTable_Find(table->text_map, name_hash);
    if (text_offset == RMT_NOT_FOUND)
    {
        // TODO: Allocation errors aren't being passed on to the caller
        text_offset = table->text->bytes_used;
        Buffer_WriteStringZ(table->text, name);
        rmtHashTable_Insert(table->text_map, name_hash, text_offset);
    }
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @SOCKETS: Sockets TCP/IP Wrapper
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

#ifndef RMT_PLATFORM_WINDOWS
typedef int SOCKET;
#define INVALID_SOCKET -1
#define SOCKET_ERROR -1
#define SD_SEND SHUT_WR
#define closesocket close
#endif

typedef struct
{
    SOCKET socket;
} TCPSocket;

typedef struct
{
    rmtBool can_read;
    rmtBool can_write;
    rmtError error_state;
} SocketStatus;

//
// Function prototypes
//
static void TCPSocket_Close(TCPSocket* tcp_socket);

static rmtError InitialiseNetwork()
{
#ifdef RMT_PLATFORM_WINDOWS

    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data))
        return RMT_ERROR_SOCKET_INIT_NETWORK_FAIL;
    if (LOBYTE(wsa_data.wVersion) != 2 || HIBYTE(wsa_data.wVersion) != 2)
        return RMT_ERROR_SOCKET_INIT_NETWORK_FAIL;

    return RMT_ERROR_NONE;

#else

    return RMT_ERROR_NONE;

#endif
}

static void ShutdownNetwork()
{
#ifdef RMT_PLATFORM_WINDOWS
    WSACleanup();
#endif
}

static rmtError TCPSocket_Constructor(TCPSocket* tcp_socket)
{
    assert(tcp_socket != NULL);
    tcp_socket->socket = INVALID_SOCKET;
    return InitialiseNetwork();
}

static void TCPSocket_Destructor(TCPSocket* tcp_socket)
{
    assert(tcp_socket != NULL);
    TCPSocket_Close(tcp_socket);
    ShutdownNetwork();
}

static rmtError TCPSocket_RunServer(TCPSocket* tcp_socket, rmtU16 port, rmtBool reuse_open_port,
                                    rmtBool limit_connections_to_localhost)
{
    SOCKET s = INVALID_SOCKET;
    struct sockaddr_in sin;
#ifdef RMT_PLATFORM_WINDOWS
    u_long nonblock = 1;
#endif

    memset(&sin, 0, sizeof(sin));
    assert(tcp_socket != NULL);

    // Try to create the socket
    s = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (s == SOCKET_ERROR)
        return RMT_ERROR_SOCKET_CREATE_FAIL;

    if (reuse_open_port)
    {
        int enable = 1;

// set SO_REUSEADDR so binding doesn't fail when restarting the application
// (otherwise the same port can't be reused within TIME_WAIT)
// I'm not checking for errors because if this fails (unlikely) we might still
// be able to bind to the socket anyway
#ifdef RMT_PLATFORM_POSIX
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, &enable, sizeof(enable));
#elif defined(RMT_PLATFORM_WINDOWS)
        // windows also needs SO_EXCLUSEIVEADDRUSE,
        // see http://www.andy-pearce.com/blog/posts/2013/Feb/so_reuseaddr-on-windows/
        setsockopt(s, SOL_SOCKET, SO_REUSEADDR, (char*)&enable, sizeof(enable));
        enable = 1;
        setsockopt(s, SOL_SOCKET, SO_EXCLUSIVEADDRUSE, (char*)&enable, sizeof(enable));
#endif
    }

    // Bind the socket to the incoming port
    sin.sin_family = AF_INET;
    sin.sin_addr.s_addr = htonl(limit_connections_to_localhost ? INADDR_LOOPBACK : INADDR_ANY);
    sin.sin_port = htons(port);
    if (bind(s, (struct sockaddr*)&sin, sizeof(sin)) == SOCKET_ERROR)
        return RMT_ERROR_SOCKET_BIND_FAIL;

    // Connection is valid, remaining code is socket state modification
    tcp_socket->socket = s;

    // Enter a listening state with a backlog of 1 connection
    if (listen(s, 1) == SOCKET_ERROR)
        return RMT_ERROR_SOCKET_LISTEN_FAIL;

// Set as non-blocking
#ifdef RMT_PLATFORM_WINDOWS
    if (ioctlsocket(tcp_socket->socket, FIONBIO, &nonblock) == SOCKET_ERROR)
        return RMT_ERROR_SOCKET_SET_NON_BLOCKING_FAIL;
#else
    if (fcntl(tcp_socket->socket, F_SETFL, O_NONBLOCK) == SOCKET_ERROR)
        return RMT_ERROR_SOCKET_SET_NON_BLOCKING_FAIL;
#endif

    return RMT_ERROR_NONE;
}

static void TCPSocket_Close(TCPSocket* tcp_socket)
{
    assert(tcp_socket != NULL);

    if (tcp_socket->socket != INVALID_SOCKET)
    {
        // Shutdown the connection, stopping all sends
        int result = shutdown(tcp_socket->socket, SD_SEND);
        if (result != SOCKET_ERROR)
        {
            // Keep receiving until the peer closes the connection
            int total = 0;
            char temp_buf[128];
            while (result > 0)
            {
                result = (int)recv(tcp_socket->socket, temp_buf, sizeof(temp_buf), 0);
                total += result;
            }
        }

        // Close the socket and issue a network shutdown request
        closesocket(tcp_socket->socket);
        tcp_socket->socket = INVALID_SOCKET;
    }
}

static SocketStatus TCPSocket_PollStatus(TCPSocket* tcp_socket)
{
    SocketStatus status;
    fd_set fd_read, fd_write, fd_errors;
    struct timeval tv;

    status.can_read = RMT_FALSE;
    status.can_write = RMT_FALSE;
    status.error_state = RMT_ERROR_NONE;

    assert(tcp_socket != NULL);
    if (tcp_socket->socket == INVALID_SOCKET)
    {
        status.error_state = RMT_ERROR_SOCKET_INVALID_POLL;
        return status;
    }

    // Set read/write/error markers for the socket
    FD_ZERO(&fd_read);
    FD_ZERO(&fd_write);
    FD_ZERO(&fd_errors);
#ifdef _MSC_VER
#pragma warning(push)
#pragma warning(disable : 4127) // warning C4127: conditional expression is constant
#endif // _MSC_VER
    FD_SET(tcp_socket->socket, &fd_read);
    FD_SET(tcp_socket->socket, &fd_write);
    FD_SET(tcp_socket->socket, &fd_errors);
#ifdef _MSC_VER
#pragma warning(pop)
#endif // _MSC_VER

    // Poll socket status without blocking
    tv.tv_sec = 0;
    tv.tv_usec = 0;
    if (select(((int)tcp_socket->socket) + 1, &fd_read, &fd_write, &fd_errors, &tv) == SOCKET_ERROR)
    {
        status.error_state = RMT_ERROR_SOCKET_SELECT_FAIL;
        return status;
    }

    status.can_read = FD_ISSET(tcp_socket->socket, &fd_read) != 0 ? RMT_TRUE : RMT_FALSE;
    status.can_write = FD_ISSET(tcp_socket->socket, &fd_write) != 0 ? RMT_TRUE : RMT_FALSE;
    status.error_state = FD_ISSET(tcp_socket->socket, &fd_errors) != 0 ? RMT_ERROR_SOCKET_POLL_ERRORS : RMT_ERROR_NONE;
    return status;
}

static rmtError TCPSocket_AcceptConnection(TCPSocket* tcp_socket, TCPSocket** client_socket)
{
    SocketStatus status;
    SOCKET s;
    rmtError error;

    // Ensure there is an incoming connection
    assert(tcp_socket != NULL);
    status = TCPSocket_PollStatus(tcp_socket);
    if (status.error_state != RMT_ERROR_NONE || !status.can_read)
        return status.error_state;

    // Accept the connection
    s = accept(tcp_socket->socket, 0, 0);
    if (s == SOCKET_ERROR)
        return RMT_ERROR_SOCKET_ACCEPT_FAIL;

#ifdef SO_NOSIGPIPE
    // On POSIX systems, send() may send a SIGPIPE signal when writing to an
    // already closed connection. By setting this option, we prevent the
    // signal from being emitted and send will instead return an error and set
    // errno to EPIPE.
    //
    // This is supported on BSD platforms and not on Linux.
    {
        int flag = 1;
        setsockopt(s, SOL_SOCKET, SO_NOSIGPIPE, &flag, sizeof(flag));
    }
#endif
    // Create a client socket for the new connection
    assert(client_socket != NULL);
    New_0(TCPSocket, *client_socket);
    if (error != RMT_ERROR_NONE)
        return error;
    (*client_socket)->socket = s;

    return RMT_ERROR_NONE;
}

static int TCPTryAgain()
{
#ifdef RMT_PLATFORM_WINDOWS
    DWORD error = WSAGetLastError();
    return error == WSAEWOULDBLOCK;
#else
#if EAGAIN == EWOULDBLOCK
    return errno == EAGAIN;
#else
    return errno == EAGAIN || errno == EWOULDBLOCK;
#endif
#endif
}

static rmtError TCPSocket_Send(TCPSocket* tcp_socket, const void* data, rmtU32 length, rmtU32 timeout_ms)
{
    SocketStatus status;
    char* cur_data = NULL;
    char* end_data = NULL;
    rmtU32 start_ms = 0;
    rmtU32 cur_ms = 0;

    assert(tcp_socket != NULL);

    start_ms = msTimer_Get();

    // Loop until timeout checking whether data can be written
    status.can_write = RMT_FALSE;
    while (!status.can_write)
    {
        status = TCPSocket_PollStatus(tcp_socket);
        if (status.error_state != RMT_ERROR_NONE)
            return status.error_state;

        cur_ms = msTimer_Get();
        if (cur_ms - start_ms > timeout_ms)
            return RMT_ERROR_SOCKET_SEND_TIMEOUT;
    }

    cur_data = (char*)data;
    end_data = cur_data + length;

    while (cur_data < end_data)
    {
        // Attempt to send the remaining chunk of data
        int bytes_sent;
        int send_flags = 0;
#ifdef MSG_NOSIGNAL
        // On Linux this prevents send from emitting a SIGPIPE signal
        // Equivalent on BSD to the SO_NOSIGPIPE option.
        send_flags = MSG_NOSIGNAL;
#endif
        bytes_sent = (int)send(tcp_socket->socket, cur_data, (int)(end_data - cur_data), send_flags);

        if (bytes_sent == SOCKET_ERROR || bytes_sent == 0)
        {
            // Close the connection if sending fails for any other reason other than blocking
            if (bytes_sent != 0 && !TCPTryAgain())
                return RMT_ERROR_SOCKET_SEND_FAIL;

            // First check for tick-count overflow and reset, giving a slight hitch every 49.7 days
            cur_ms = msTimer_Get();
            if (cur_ms < start_ms)
            {
                start_ms = cur_ms;
                continue;
            }

            //
            // Timeout can happen when:
            //
            //    1) endpoint is no longer there
            //    2) endpoint can't consume quick enough
            //    3) local buffers overflow
            //
            // As none of these are actually errors, we have to pass this timeout back to the caller.
            //
            // TODO: This strategy breaks down if a send partially completes and then times out!
            //
            if (cur_ms - start_ms > timeout_ms)
            {
                return RMT_ERROR_SOCKET_SEND_TIMEOUT;
            }
        }
        else
        {
            // Jump over the data sent
            cur_data += bytes_sent;
        }
    }

    return RMT_ERROR_NONE;
}

static rmtError TCPSocket_Receive(TCPSocket* tcp_socket, void* data, rmtU32 length, rmtU32 timeout_ms)
{
    SocketStatus status;
    char* cur_data = NULL;
    char* end_data = NULL;
    rmtU32 start_ms = 0;
    rmtU32 cur_ms = 0;

    assert(tcp_socket != NULL);

    // Ensure there is data to receive
    status = TCPSocket_PollStatus(tcp_socket);
    if (status.error_state != RMT_ERROR_NONE)
        return status.error_state;
    if (!status.can_read)
        return RMT_ERROR_SOCKET_RECV_NO_DATA;

    cur_data = (char*)data;
    end_data = cur_data + length;

    // Loop until all data has been received
    start_ms = msTimer_Get();
    while (cur_data < end_data)
    {
        int bytes_received = (int)recv(tcp_socket->socket, cur_data, (int)(end_data - cur_data), 0);

        if (bytes_received == SOCKET_ERROR || bytes_received == 0)
        {
            // Close the connection if receiving fails for any other reason other than blocking
            if (bytes_received != 0 && !TCPTryAgain())
                return RMT_ERROR_SOCKET_RECV_FAILED;

            // First check for tick-count overflow and reset, giving a slight hitch every 49.7 days
            cur_ms = msTimer_Get();
            if (cur_ms < start_ms)
            {
                start_ms = cur_ms;
                continue;
            }

            //
            // Timeout can happen when:
            //
            //    1) data is delayed by sender
            //    2) sender fails to send a complete set of packets
            //
            // As not all of these scenarios are errors, we need to pass this information back to the caller.
            //
            // TODO: This strategy breaks down if a receive partially completes and then times out!
            //
            if (cur_ms - start_ms > timeout_ms)
            {
                return RMT_ERROR_SOCKET_RECV_TIMEOUT;
            }
        }
        else
        {
            // Jump over the data received
            cur_data += bytes_received;
        }
    }

    return RMT_ERROR_NONE;
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @SHA1: SHA-1 Cryptographic Hash Function
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

//
// Typed to allow enforced data size specification
//
typedef struct
{
    rmtU8 data[20];
} SHA1;

/*
 Copyright (c) 2011, Micael Hildenborg
 All rights reserved.

 Redistribution and use in source and binary forms, with or without
 modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of Micael Hildenborg nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

 THIS SOFTWARE IS PROVIDED BY Micael Hildenborg ''AS IS'' AND ANY
 EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
 WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 DISCLAIMED. IN NO EVENT SHALL Micael Hildenborg BE LIABLE FOR ANY
 DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
 LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
 ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
 (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/*
 Contributors:
 Gustav
 Several members in the gamedev.se forum.
 Gregory Petrosyan
 */

// Rotate an integer value to left.
static unsigned int rol(const unsigned int value, const unsigned int steps)
{
    return ((value << steps) | (value >> (32 - steps)));
}

// Sets the first 16 integers in the buffert to zero.
// Used for clearing the W buffert.
static void clearWBuffert(unsigned int* buffert)
{
    int pos;
    for (pos = 16; --pos >= 0;)
    {
        buffert[pos] = 0;
    }
}

static void innerHash(unsigned int* result, unsigned int* w)
{
    unsigned int a = result[0];
    unsigned int b = result[1];
    unsigned int c = result[2];
    unsigned int d = result[3];
    unsigned int e = result[4];

    int round = 0;

#define sha1macro(func, val)                                            \
    {                                                                   \
        const unsigned int t = rol(a, 5) + (func) + e + val + w[round]; \
        e = d;                                                          \
        d = c;                                                          \
        c = rol(b, 30);                                                 \
        b = a;                                                          \
        a = t;                                                          \
    }

    while (round < 16)
    {
        sha1macro((b & c) | (~b & d), 0x5a827999);
        ++round;
    }
    while (round < 20)
    {
        w[round] = rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);
        sha1macro((b & c) | (~b & d), 0x5a827999);
        ++round;
    }
    while (round < 40)
    {
        w[round] = rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);
        sha1macro(b ^ c ^ d, 0x6ed9eba1);
        ++round;
    }
    while (round < 60)
    {
        w[round] = rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);
        sha1macro((b & c) | (b & d) | (c & d), 0x8f1bbcdc);
        ++round;
    }
    while (round < 80)
    {
        w[round] = rol((w[round - 3] ^ w[round - 8] ^ w[round - 14] ^ w[round - 16]), 1);
        sha1macro(b ^ c ^ d, 0xca62c1d6);
        ++round;
    }

#undef sha1macro

    result[0] += a;
    result[1] += b;
    result[2] += c;
    result[3] += d;
    result[4] += e;
}

static void calc(const void* src, const int bytelength, unsigned char* hash)
{
    int roundPos;
    int lastBlockBytes;
    int hashByte;

    // Init the result array.
    unsigned int result[5] = {0x67452301, 0xefcdab89, 0x98badcfe, 0x10325476, 0xc3d2e1f0};

    // Cast the void src pointer to be the byte array we can work with.
    const unsigned char* sarray = (const unsigned char*)src;

    // The reusable round buffer
    unsigned int w[80];

    // Loop through all complete 64byte blocks.
    const int endOfFullBlocks = bytelength - 64;
    int endCurrentBlock;
    int currentBlock = 0;

    while (currentBlock <= endOfFullBlocks)
    {
        endCurrentBlock = currentBlock + 64;

        // Init the round buffer with the 64 byte block data.
        for (roundPos = 0; currentBlock < endCurrentBlock; currentBlock += 4)
        {
            // This line will swap endian on big endian and keep endian on little endian.
            w[roundPos++] = (unsigned int)sarray[currentBlock + 3] | (((unsigned int)sarray[currentBlock + 2]) << 8) |
                            (((unsigned int)sarray[currentBlock + 1]) << 16) |
                            (((unsigned int)sarray[currentBlock]) << 24);
        }
        innerHash(result, w);
    }

    // Handle the last and not full 64 byte block if existing.
    endCurrentBlock = bytelength - currentBlock;
    clearWBuffert(w);
    lastBlockBytes = 0;
    for (; lastBlockBytes < endCurrentBlock; ++lastBlockBytes)
    {
        w[lastBlockBytes >> 2] |= (unsigned int)sarray[lastBlockBytes + currentBlock]
                                  << ((3 - (lastBlockBytes & 3)) << 3);
    }
    w[lastBlockBytes >> 2] |= 0x80U << ((3 - (lastBlockBytes & 3)) << 3);
    if (endCurrentBlock >= 56)
    {
        innerHash(result, w);
        clearWBuffert(w);
    }
    w[15] = bytelength << 3;
    innerHash(result, w);

    // Store hash in result pointer, and make sure we get in in the correct order on both endian models.
    for (hashByte = 20; --hashByte >= 0;)
    {
        hash[hashByte] = (result[hashByte >> 2] >> (((3 - hashByte) & 0x3) << 3)) & 0xff;
    }
}

static SHA1 SHA1_Calculate(const void* src, unsigned int length)
{
    SHA1 hash;
    assert((int)length >= 0);
    calc(src, length, hash.data);
    return hash;
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @BASE64: Base-64 encoder
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

static const char* b64_encoding_table = "ABCDEFGHIJKLMNOPQRSTUVWXYZ"
                                        "abcdefghijklmnopqrstuvwxyz"
                                        "0123456789+/";

static rmtU32 Base64_CalculateEncodedLength(rmtU32 length)
{
    // ceil(l * 4/3)
    return 4 * ((length + 2) / 3);
}

static void Base64_Encode(const rmtU8* in_bytes, rmtU32 length, rmtU8* out_bytes)
{
    rmtU32 i;
    rmtU32 encoded_length;
    rmtU32 remaining_bytes;

    rmtU8* optr = out_bytes;

    for (i = 0; i < length;)
    {
        // Read input 3 values at a time, null terminating
        rmtU32 c0 = i < length ? in_bytes[i++] : 0;
        rmtU32 c1 = i < length ? in_bytes[i++] : 0;
        rmtU32 c2 = i < length ? in_bytes[i++] : 0;

        // Encode 4 bytes for ever 3 input bytes
        rmtU32 triple = (c0 << 0x10) + (c1 << 0x08) + c2;
        *optr++ = b64_encoding_table[(triple >> 3 * 6) & 0x3F];
        *optr++ = b64_encoding_table[(triple >> 2 * 6) & 0x3F];
        *optr++ = b64_encoding_table[(triple >> 1 * 6) & 0x3F];
        *optr++ = b64_encoding_table[(triple >> 0 * 6) & 0x3F];
    }

    // Pad output to multiple of 3 bytes with terminating '='
    encoded_length = Base64_CalculateEncodedLength(length);
    remaining_bytes = (3 - ((length + 2) % 3)) - 1;
    for (i = 0; i < remaining_bytes; i++)
        out_bytes[encoded_length - 1 - i] = '=';

    // Null terminate
    out_bytes[encoded_length] = 0;
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @MURMURHASH: MurmurHash3
   https://code.google.com/p/smhasher
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

//-----------------------------------------------------------------------------
// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.
//-----------------------------------------------------------------------------

static rmtU32 rotl32(rmtU32 x, rmtS8 r)
{
    return (x << r) | (x >> (32 - r));
}

// Block read - if your platform needs to do endian-swapping, do the conversion here
static rmtU32 getblock32(const rmtU32* p, int i)
{
    rmtU32 result;
    const rmtU8* src = ((const rmtU8*)p) + i * sizeof(rmtU32);
    memcpy(&result, src, sizeof(result));
    return result;
}

// Finalization mix - force all bits of a hash block to avalanche
static rmtU32 fmix32(rmtU32 h)
{
    h ^= h >> 16;
    h *= 0x85ebca6b;
    h ^= h >> 13;
    h *= 0xc2b2ae35;
    h ^= h >> 16;
    return h;
}

static rmtU32 MurmurHash3_x86_32(const void* key, int len, rmtU32 seed)
{
    const rmtU8* data = (const rmtU8*)key;
    const int nblocks = len / 4;

    rmtU32 h1 = seed;

    const rmtU32 c1 = 0xcc9e2d51;
    const rmtU32 c2 = 0x1b873593;

    int i;

    const rmtU32* blocks = (const rmtU32*)(data + nblocks * 4);
    const rmtU8* tail = (const rmtU8*)(data + nblocks * 4);

    rmtU32 k1 = 0;

    //----------
    // body

    for (i = -nblocks; i; i++)
    {
        rmtU32 k2 = getblock32(blocks, i);

        k2 *= c1;
        k2 = rotl32(k2, 15);
        k2 *= c2;

        h1 ^= k2;
        h1 = rotl32(h1, 13);
        h1 = h1 * 5 + 0xe6546b64;
    }

    //----------
    // tail

    switch (len & 3)
    {
        case 3:
            k1 ^= tail[2] << 16; // fallthrough
        case 2:
            k1 ^= tail[1] << 8; // fallthrough
        case 1:
            k1 ^= tail[0];
            k1 *= c1;
            k1 = rotl32(k1, 15);
            k1 *= c2;
            h1 ^= k1;
    };

    //----------
    // finalization

    h1 ^= len;

    h1 = fmix32(h1);

    return h1;
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @WEBSOCKETS: WebSockets
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

enum WebSocketMode
{
    WEBSOCKET_NONE = 0,
    WEBSOCKET_TEXT = 1,
    WEBSOCKET_BINARY = 2,
};

typedef struct
{
    TCPSocket* tcp_socket;

    enum WebSocketMode mode;

    rmtU32 frame_bytes_remaining;
    rmtU32 mask_offset;

    union {
        rmtU8 mask[4];
        rmtU32 mask_u32;
    } data;

} WebSocket;

static void WebSocket_Close(WebSocket* web_socket);

static char* GetField(char* buffer, r_size_t buffer_length, rmtPStr field_name)
{
    char* field = NULL;
    char* buffer_end = buffer + buffer_length - 1;

    r_size_t field_length = strnlen_s(field_name, buffer_length);
    if (field_length == 0)
        return NULL;

    // Search for the start of the field
    if (strstr_s(buffer, buffer_length, field_name, field_length, &field) != EOK)
        return NULL;

    // Field name is now guaranteed to be in the buffer so its safe to jump over it without hitting the bounds
    field += strlen(field_name);

    // Skip any trailing whitespace
    while (*field == ' ')
    {
        if (field >= buffer_end)
            return NULL;
        field++;
    }

    return field;
}

static const char websocket_guid[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";
static const char websocket_response[] = "HTTP/1.1 101 Switching Protocols\r\n"
                                         "Upgrade: websocket\r\n"
                                         "Connection: Upgrade\r\n"
                                         "Sec-WebSocket-Accept: ";

static rmtError WebSocketHandshake(TCPSocket* tcp_socket, rmtPStr limit_host)
{
    rmtU32 start_ms, now_ms;

    // Parsing scratchpad
    char buffer[1024];
    char* buffer_ptr = buffer;
    int buffer_len = sizeof(buffer) - 1;
    char* buffer_end = buffer + buffer_len;

    char response_buffer[256];
    int response_buffer_len = sizeof(response_buffer) - 1;

    char* version;
    char* host;
    char* key;
    char* key_end;
    SHA1 hash;

    assert(tcp_socket != NULL);

    start_ms = msTimer_Get();

    // Really inefficient way of receiving the handshake data from the browser
    // Not really sure how to do this any better, as the termination requirement is \r\n\r\n
    while (buffer_ptr - buffer < buffer_len)
    {
        rmtError error = TCPSocket_Receive(tcp_socket, buffer_ptr, 1, 20);
        if (error == RMT_ERROR_SOCKET_RECV_FAILED)
            return error;

        // If there's a stall receiving the data, check for a handshake timeout
        if (error == RMT_ERROR_SOCKET_RECV_NO_DATA || error == RMT_ERROR_SOCKET_RECV_TIMEOUT)
        {
            now_ms = msTimer_Get();
            if (now_ms - start_ms > 1000)
                return RMT_ERROR_SOCKET_RECV_TIMEOUT;

            continue;
        }

        // Just in case new enums are added...
        assert(error == RMT_ERROR_NONE);

        if (buffer_ptr - buffer >= 4)
        {
            if (*(buffer_ptr - 3) == '\r' && *(buffer_ptr - 2) == '\n' && *(buffer_ptr - 1) == '\r' &&
                *(buffer_ptr - 0) == '\n')
                break;
        }

        buffer_ptr++;
    }
    *buffer_ptr = 0;

    // HTTP GET instruction
    if (memcmp(buffer, "GET", 3) != 0)
        return RMT_ERROR_WEBSOCKET_HANDSHAKE_NOT_GET;

    // Look for the version number and verify that it's supported
    version = GetField(buffer, buffer_len, "Sec-WebSocket-Version:");
    if (version == NULL)
        return RMT_ERROR_WEBSOCKET_HANDSHAKE_NO_VERSION;
    if (buffer_end - version < 2 || (version[0] != '8' && (version[0] != '1' || version[1] != '3')))
        return RMT_ERROR_WEBSOCKET_HANDSHAKE_BAD_VERSION;

    // Make sure this connection comes from a known host
    host = GetField(buffer, buffer_len, "Host:");
    if (host == NULL)
        return RMT_ERROR_WEBSOCKET_HANDSHAKE_NO_HOST;
    if (limit_host != NULL)
    {
        r_size_t limit_host_len = strnlen_s(limit_host, 128);
        char* found = NULL;
        if (strstr_s(host, buffer_end - host, limit_host, limit_host_len, &found) != EOK)
            return RMT_ERROR_WEBSOCKET_HANDSHAKE_BAD_HOST;
    }

    // Look for the key start and null-terminate it within the receive buffer
    key = GetField(buffer, buffer_len, "Sec-WebSocket-Key:");
    if (key == NULL)
        return RMT_ERROR_WEBSOCKET_HANDSHAKE_NO_KEY;
    if (strstr_s(key, buffer_end - key, "\r\n", 2, &key_end) != EOK)
        return RMT_ERROR_WEBSOCKET_HANDSHAKE_BAD_KEY;
    *key_end = 0;

    // Concatenate the browser's key with the WebSocket Protocol GUID and base64 encode
    // the hash, to prove to the browser that this is a bonafide WebSocket server
    buffer[0] = 0;
    if (strncat_s(buffer, buffer_len, key, key_end - key) != EOK)
        return RMT_ERROR_WEBSOCKET_HANDSHAKE_STRING_FAIL;
    if (strncat_s(buffer, buffer_len, websocket_guid, sizeof(websocket_guid)) != EOK)
        return RMT_ERROR_WEBSOCKET_HANDSHAKE_STRING_FAIL;
    hash = SHA1_Calculate(buffer, (rmtU32)strnlen_s(buffer, buffer_len));
    Base64_Encode(hash.data, sizeof(hash.data), (rmtU8*)buffer);

    // Send the response back to the server with a longer timeout than usual
    response_buffer[0] = 0;
    if (strncat_s(response_buffer, response_buffer_len, websocket_response, sizeof(websocket_response)) != EOK)
        return RMT_ERROR_WEBSOCKET_HANDSHAKE_STRING_FAIL;
    if (strncat_s(response_buffer, response_buffer_len, buffer, buffer_len) != EOK)
        return RMT_ERROR_WEBSOCKET_HANDSHAKE_STRING_FAIL;
    if (strncat_s(response_buffer, response_buffer_len, "\r\n\r\n", 4) != EOK)
        return RMT_ERROR_WEBSOCKET_HANDSHAKE_STRING_FAIL;

    return TCPSocket_Send(tcp_socket, response_buffer, (rmtU32)strnlen_s(response_buffer, response_buffer_len), 1000);
}

static rmtError WebSocket_Constructor(WebSocket* web_socket, TCPSocket* tcp_socket)
{
    rmtError error = RMT_ERROR_NONE;

    assert(web_socket != NULL);
    web_socket->tcp_socket = tcp_socket;
    web_socket->mode = WEBSOCKET_NONE;
    web_socket->frame_bytes_remaining = 0;
    web_socket->mask_offset = 0;
    web_socket->data.mask[0] = 0;
    web_socket->data.mask[1] = 0;
    web_socket->data.mask[2] = 0;
    web_socket->data.mask[3] = 0;

    // Caller can optionally specify which TCP socket to use
    if (web_socket->tcp_socket == NULL)
        New_0(TCPSocket, web_socket->tcp_socket);

    return error;
}

static void WebSocket_Destructor(WebSocket* web_socket)
{
    WebSocket_Close(web_socket);
}

static rmtError WebSocket_RunServer(WebSocket* web_socket, rmtU16 port, rmtBool reuse_open_port,
                                    rmtBool limit_connections_to_localhost, enum WebSocketMode mode)
{
    // Create the server's listening socket
    assert(web_socket != NULL);
    web_socket->mode = mode;
    return TCPSocket_RunServer(web_socket->tcp_socket, port, reuse_open_port, limit_connections_to_localhost);
}

static void WebSocket_Close(WebSocket* web_socket)
{
    assert(web_socket != NULL);
    Delete(TCPSocket, web_socket->tcp_socket);
}

static SocketStatus WebSocket_PollStatus(WebSocket* web_socket)
{
    assert(web_socket != NULL);
    return TCPSocket_PollStatus(web_socket->tcp_socket);
}

static rmtError WebSocket_AcceptConnection(WebSocket* web_socket, WebSocket** client_socket)
{
    TCPSocket* tcp_socket = NULL;
    rmtError error;

    // Is there a waiting connection?
    assert(web_socket != NULL);
    error = TCPSocket_AcceptConnection(web_socket->tcp_socket, &tcp_socket);
    if (error != RMT_ERROR_NONE || tcp_socket == NULL)
        return error;

    // Need a successful handshake between client/server before allowing the connection
    // TODO: Specify limit_host
    error = WebSocketHandshake(tcp_socket, NULL);
    if (error != RMT_ERROR_NONE)
        return error;

    // Allocate and return a new client socket
    assert(client_socket != NULL);
    New_1(WebSocket, *client_socket, tcp_socket);
    if (error != RMT_ERROR_NONE)
        return error;

    (*client_socket)->mode = web_socket->mode;

    return RMT_ERROR_NONE;
}

static void WriteSize(rmtU32 size, rmtU8* dest, rmtU32 dest_size, rmtU32 dest_offset)
{
    int size_size = dest_size - dest_offset;
    rmtU32 i;
    for (i = 0; i < dest_size; i++)
    {
        int j = i - dest_offset;
        dest[i] = (j < 0) ? 0 : (size >> ((size_size - j - 1) * 8)) & 0xFF;
    }
}

// For send buffers to preallocate
#define WEBSOCKET_MAX_FRAME_HEADER_SIZE 10

static void WebSocket_PrepareBuffer(Buffer* buffer)
{
    char empty_frame_header[WEBSOCKET_MAX_FRAME_HEADER_SIZE];

    assert(buffer != NULL);

    // Reset to start
    buffer->bytes_used = 0;

    // Allocate enough space for a maximum-sized frame header
    Buffer_Write(buffer, empty_frame_header, sizeof(empty_frame_header));
}

static rmtU32 WebSocket_FrameHeaderSize(rmtU32 length)
{
    if (length <= 125)
        return 2;
    if (length <= 65535)
        return 4;
    return 10;
}

static void WebSocket_WriteFrameHeader(WebSocket* web_socket, rmtU8* dest, rmtU32 length)
{
    rmtU8 final_fragment = 0x1 << 7;
    rmtU8 frame_type = (rmtU8)web_socket->mode;

    dest[0] = final_fragment | frame_type;

    // Construct the frame header, correctly applying the narrowest size
    if (length <= 125)
    {
        dest[1] = (rmtU8)length;
    }
    else if (length <= 65535)
    {
        dest[1] = 126;
        WriteSize(length, dest + 2, 2, 0);
    }
    else
    {
        dest[1] = 127;
        WriteSize(length, dest + 2, 8, 4);
    }
}

static rmtError WebSocket_Send(WebSocket* web_socket, const void* data, rmtU32 length, rmtU32 timeout_ms)
{
    rmtError error;
    SocketStatus status;
    rmtU32 payload_length, frame_header_size, delta;

    assert(web_socket != NULL);
    assert(data != NULL);

    // Can't send if there are socket errors
    status = WebSocket_PollStatus(web_socket);
    if (status.error_state != RMT_ERROR_NONE)
        return status.error_state;

    // Assume space for max frame header has been allocated in the incoming data
    payload_length = length - WEBSOCKET_MAX_FRAME_HEADER_SIZE;
    frame_header_size = WebSocket_FrameHeaderSize(payload_length);
    delta = WEBSOCKET_MAX_FRAME_HEADER_SIZE - frame_header_size;
    data = (void*)((rmtU8*)data + delta);
    length -= delta;
    WebSocket_WriteFrameHeader(web_socket, (rmtU8*)data, payload_length);

    // Send frame header and data together
    error = TCPSocket_Send(web_socket->tcp_socket, data, length, timeout_ms);
    return error;
}

static rmtError ReceiveFrameHeader(WebSocket* web_socket)
{
    // TODO: Specify infinite timeout?

    rmtError error;
    rmtU8 msg_header[2] = {0, 0};
    int msg_length, size_bytes_remaining, i;
    rmtBool mask_present;

    assert(web_socket != NULL);

    // Get message header
    error = TCPSocket_Receive(web_socket->tcp_socket, msg_header, 2, 20);
    if (error != RMT_ERROR_NONE)
        return error;

    // Check for WebSocket Protocol disconnect
    if (msg_header[0] == 0x88)
        return RMT_ERROR_WEBSOCKET_DISCONNECTED;

    // Check that the client isn't sending messages we don't understand
    if (msg_header[0] != 0x81 && msg_header[0] != 0x82)
        return RMT_ERROR_WEBSOCKET_BAD_FRAME_HEADER;

    // Get message length and check to see if it's a marker for a wider length
    msg_length = msg_header[1] & 0x7F;
    size_bytes_remaining = 0;
    switch (msg_length)
    {
        case 126:
            size_bytes_remaining = 2;
            break;
        case 127:
            size_bytes_remaining = 8;
            break;
    }

    if (size_bytes_remaining > 0)
    {
        // Receive the wider bytes of the length
        rmtU8 size_bytes[8];
        error = TCPSocket_Receive(web_socket->tcp_socket, size_bytes, size_bytes_remaining, 20);
        if (error != RMT_ERROR_NONE)
            return RMT_ERROR_WEBSOCKET_BAD_FRAME_HEADER_SIZE;

        // Calculate new length, MSB first
        msg_length = 0;
        for (i = 0; i < size_bytes_remaining; i++)
            msg_length |= size_bytes[i] << ((size_bytes_remaining - 1 - i) * 8);
    }

    // Receive any message data masks
    mask_present = (msg_header[1] & 0x80) != 0 ? RMT_TRUE : RMT_FALSE;
    if (mask_present)
    {
        error = TCPSocket_Receive(web_socket->tcp_socket, web_socket->data.mask, 4, 20);
        if (error != RMT_ERROR_NONE)
            return error;
    }

    web_socket->frame_bytes_remaining = msg_length;
    web_socket->mask_offset = 0;

    return RMT_ERROR_NONE;
}

static rmtError WebSocket_Receive(WebSocket* web_socket, void* data, rmtU32* msg_len, rmtU32 length, rmtU32 timeout_ms)
{
    SocketStatus status;
    char* cur_data;
    char* end_data;
    rmtU32 start_ms, now_ms;
    rmtU32 bytes_to_read;
    rmtError error;

    assert(web_socket != NULL);

    // Can't read with any socket errors
    status = WebSocket_PollStatus(web_socket);
    if (status.error_state != RMT_ERROR_NONE)
        return status.error_state;

    cur_data = (char*)data;
    end_data = cur_data + length;

    start_ms = msTimer_Get();
    while (cur_data < end_data)
    {
        // Get next WebSocket frame if we've run out of data to read from the socket
        if (web_socket->frame_bytes_remaining == 0)
        {
            error = ReceiveFrameHeader(web_socket);
            if (error != RMT_ERROR_NONE)
                return error;

            // Set output message length only on initial receive
            if (msg_len != NULL)
                *msg_len = web_socket->frame_bytes_remaining;
        }

        // Read as much required data as possible
        bytes_to_read = web_socket->frame_bytes_remaining < length ? web_socket->frame_bytes_remaining : length;
        error = TCPSocket_Receive(web_socket->tcp_socket, cur_data, bytes_to_read, 20);
        if (error == RMT_ERROR_SOCKET_RECV_FAILED)
            return error;

        // If there's a stall receiving the data, check for timeout
        if (error == RMT_ERROR_SOCKET_RECV_NO_DATA || error == RMT_ERROR_SOCKET_RECV_TIMEOUT)
        {
            now_ms = msTimer_Get();
            if (now_ms - start_ms > timeout_ms)
                return RMT_ERROR_SOCKET_RECV_TIMEOUT;
            continue;
        }

        // Apply data mask
        if (web_socket->data.mask_u32 != 0)
        {
            rmtU32 i;
            for (i = 0; i < bytes_to_read; i++)
            {
                *((rmtU8*)cur_data + i) ^= web_socket->data.mask[web_socket->mask_offset & 3];
                web_socket->mask_offset++;
            }
        }

        cur_data += bytes_to_read;
        web_socket->frame_bytes_remaining -= bytes_to_read;
    }

    return RMT_ERROR_NONE;
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @MESSAGEQ: Multiple producer, single consumer message queue
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

typedef enum MessageID
{
    MsgID_NotReady,
    MsgID_AddToStringTable,
    MsgID_LogText,
    MsgID_SampleTree,
    MsgID_ProcessorThreads,
    MsgID_ThreadName,
    MsgID_None,
    MsgID_Force32Bits = 0xFFFFFFFF,
} MessageID;

typedef struct Message
{
    MessageID id;

    rmtU32 payload_size;

    // For telling which thread the message came from in the debugger
    struct ThreadProfiler* threadProfiler;

    rmtU8 payload[1];
} Message;

// Multiple producer, single consumer message queue that uses its own data buffer
// to store the message data.
typedef struct rmtMessageQueue
{
    rmtU32 size;

    // The physical address of this data buffer is pointed to by two sequential
    // virtual memory pages, allowing automatic wrap-around of any reads or writes
    // that exceed the limits of the buffer.
    VirtualMirrorBuffer* data;

    // Read/write position never wrap allowing trivial overflow checks
    // with easier debugging
    rmtU32 read_pos;
    rmtU32 write_pos;

} rmtMessageQueue;

static rmtError rmtMessageQueue_Constructor(rmtMessageQueue* queue, rmtU32 size)
{
    rmtError error;

    assert(queue != NULL);

    // Set defaults
    queue->size = 0;
    queue->data = NULL;
    queue->read_pos = 0;
    queue->write_pos = 0;

    New_2(VirtualMirrorBuffer, queue->data, size, 10);
    if (error != RMT_ERROR_NONE)
        return error;

    // The mirror buffer needs to be page-aligned and will change the requested
    // size to match that.
    queue->size = queue->data->size;

    // Set the entire buffer to not ready message
    memset(queue->data->ptr, MsgID_NotReady, queue->size);

    return RMT_ERROR_NONE;
}

static void rmtMessageQueue_Destructor(rmtMessageQueue* queue)
{
    assert(queue != NULL);
    Delete(VirtualMirrorBuffer, queue->data);
}

static rmtU32 rmtMessageQueue_SizeForPayload(rmtU32 payload_size)
{
    // Add message header and align for ARM platforms
    rmtU32 size = sizeof(Message) + payload_size;
    size = (size + 3) & ~3U;
    return size;
}

static Message* rmtMessageQueue_AllocMessage(rmtMessageQueue* queue, rmtU32 payload_size,
                                             struct ThreadProfiler* thread_profiler)
{
    Message* msg;

    rmtU32 write_size = rmtMessageQueue_SizeForPayload(payload_size);

    assert(queue != NULL);

    for (;;)
    {
        // Check for potential overflow
        // Order of loads means allocation failure can happen when enough space has just been freed
        // However, incorrect overflows are not possible
        rmtU32 s = queue->size;
        rmtU32 w = LoadAcquire(&queue->write_pos);
        rmtU32 r = LoadAcquire(&queue->read_pos);
        if ((int)(w - r) > ((int)(s - write_size)))
            return NULL;

        // Point to the newly allocated space
        msg = (Message*)(queue->data->ptr + (w & (s - 1)));

        // Increment the write position, leaving the loop if this is the thread that succeeded
        if (AtomicCompareAndSwap(&queue->write_pos, w, w + write_size) == RMT_TRUE)
        {
            // Safe to set payload size after thread claims ownership of this allocated range
            msg->payload_size = payload_size;
            msg->threadProfiler = thread_profiler;
            break;
        }
    }

    return msg;
}

static void rmtMessageQueue_CommitMessage(Message* message, MessageID id)
{
    assert(message != NULL);

    // Setting the message ID signals to the consumer that the message is ready
    assert(LoadAcquire((rmtU32*)&message->id) == MsgID_NotReady);
    StoreRelease((rmtU32*)&message->id, id);
}

Message* rmtMessageQueue_PeekNextMessage(rmtMessageQueue* queue)
{
    Message* ptr;
    rmtU32 r, w;
    MessageID id;

    assert(queue != NULL);

    // First check that there are bytes queued
    w = LoadAcquire(&queue->write_pos);
    r = queue->read_pos;
    if (w - r == 0)
        return NULL;

    // Messages are in the queue but may not have been commit yet
    // Messages behind this one may have been commit but it's not reachable until
    // the next one in the queue is ready.
    r = r & (queue->size - 1);
    ptr = (Message*)(queue->data->ptr + r);
    id = (MessageID)LoadAcquire((rmtU32*)&ptr->id);
    if (id != MsgID_NotReady)
        return ptr;

    return NULL;
}

static void rmtMessageQueue_ConsumeNextMessage(rmtMessageQueue* queue, Message* message)
{
    rmtU32 message_size, read_pos;

    assert(queue != NULL);
    assert(message != NULL);

    // Setting the message ID to "not ready" serves as a marker to the consumer that even though
    // space has been allocated for a message, the message isn't ready to be consumed
    // yet.
    //
    // We can't do that when allocating the message because multiple threads will be fighting for
    // the same location. Instead, clear out any messages just read by the consumer before advancing
    // the read position so that a winning thread's allocation will inherit the "not ready" state.
    //
    // This costs some write bandwidth and has the potential to flush cache to other cores.
    message_size = rmtMessageQueue_SizeForPayload(message->payload_size);
    memset(message, MsgID_NotReady, message_size);

    // Advance read position
    read_pos = queue->read_pos + message_size;
    StoreRelease(&queue->read_pos, read_pos);
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @NETWORK: Network Server
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

typedef rmtError (*Server_ReceiveHandler)(void*, char*, rmtU32);

typedef struct
{
    WebSocket* listen_socket;

    WebSocket* client_socket;

    rmtU32 last_ping_time;

    rmtU16 port;

    rmtBool reuse_open_port;
    rmtBool limit_connections_to_localhost;

    // A dynamically-sized buffer used for binary-encoding messages and sending to the client
    Buffer* bin_buf;

    // Handler for receiving messages from the client
    Server_ReceiveHandler receive_handler;
    void* receive_handler_context;
} Server;

static rmtError Server_CreateListenSocket(Server* server, rmtU16 port, rmtBool reuse_open_port,
                                          rmtBool limit_connections_to_localhost)
{
    rmtError error = RMT_ERROR_NONE;

    New_1(WebSocket, server->listen_socket, NULL);
    if (error == RMT_ERROR_NONE)
        error = WebSocket_RunServer(server->listen_socket, port, reuse_open_port, limit_connections_to_localhost,
                                    WEBSOCKET_BINARY);

    return error;
}

static rmtError Server_Constructor(Server* server, rmtU16 port, rmtBool reuse_open_port,
                                   rmtBool limit_connections_to_localhost)
{
    rmtError error;

    assert(server != NULL);
    server->listen_socket = NULL;
    server->client_socket = NULL;
    server->last_ping_time = 0;
    server->port = port;
    server->reuse_open_port = reuse_open_port;
    server->limit_connections_to_localhost = limit_connections_to_localhost;
    server->bin_buf = NULL;
    server->receive_handler = NULL;
    server->receive_handler_context = NULL;

    // Create the binary serialisation buffer
    New_1(Buffer, server->bin_buf, 4096);
    if (error != RMT_ERROR_NONE)
        return error;

    // Create the listening WebSocket
    return Server_CreateListenSocket(server, port, reuse_open_port, limit_connections_to_localhost);
}

static void Server_Destructor(Server* server)
{
    assert(server != NULL);
    Delete(WebSocket, server->client_socket);
    Delete(WebSocket, server->listen_socket);
    Delete(Buffer, server->bin_buf);
}

static rmtBool Server_IsClientConnected(Server* server)
{
    assert(server != NULL);
    return server->client_socket != NULL ? RMT_TRUE : RMT_FALSE;
}

static void Server_DisconnectClient(Server* server)
{
    WebSocket* client_socket;

    assert(server != NULL);

    // NULL the variable before destroying the socket
    client_socket = server->client_socket;
    server->client_socket = NULL;
    CompilerWriteFence();
    Delete(WebSocket, client_socket);
}

static rmtError Server_Send(Server* server, const void* data, rmtU32 length, rmtU32 timeout)
{
    assert(server != NULL);
    if (Server_IsClientConnected(server))
    {
        rmtError error = WebSocket_Send(server->client_socket, data, length, timeout);
        if (error == RMT_ERROR_SOCKET_SEND_FAIL)
            Server_DisconnectClient(server);

        return error;
    }

    return RMT_ERROR_NONE;
}

static rmtError Server_ReceiveMessage(Server* server, char message_first_byte, rmtU32 message_length)
{
    char message_data[1024];
    rmtError error;

    // Check for potential message data overflow
    if (message_length >= sizeof(message_data) - 1)
    {
        rmt_LogText("Ignoring console input bigger than internal receive buffer (1024 bytes)");
        return RMT_ERROR_NONE;
    }

    // Receive the rest of the message
    message_data[0] = message_first_byte;
    error = WebSocket_Receive(server->client_socket, message_data + 1, NULL, message_length - 1, 100);
    if (error != RMT_ERROR_NONE)
        return error;
    message_data[message_length] = 0;

    // Each message must have a descriptive 4 byte header
    if (message_length < 4)
        return RMT_ERROR_NONE;

    // Dispatch to handler
    if (server->receive_handler)
        error = server->receive_handler(server->receive_handler_context, message_data, message_length);

    return error;
}

static void Server_Update(Server* server)
{
    rmtU32 cur_time;

    assert(server != NULL);

    // Recreate the listening socket if it's been destroyed earlier
    if (server->listen_socket == NULL)
        Server_CreateListenSocket(server, server->port, server->reuse_open_port,
                                  server->limit_connections_to_localhost);

    if (server->listen_socket != NULL && server->client_socket == NULL)
    {
        // Accept connections as long as there is no client connected
        WebSocket* client_socket = NULL;
        rmtError error = WebSocket_AcceptConnection(server->listen_socket, &client_socket);
        if (error == RMT_ERROR_NONE)
        {
            server->client_socket = client_socket;
        }
        else
        {
            // Destroy the listen socket on failure to accept
            // It will get recreated in another update
            Delete(WebSocket, server->listen_socket);
        }
    }

    else
    {
        // Loop checking for incoming messages
        for (;;)
        {
            // Inspect first byte to see if a message is there
            char message_first_byte;
            rmtU32 message_length;
            rmtError error = WebSocket_Receive(server->client_socket, &message_first_byte, &message_length, 1, 0);
            if (error == RMT_ERROR_NONE)
            {
                // Parse remaining message
                error = Server_ReceiveMessage(server, message_first_byte, message_length);
                if (error != RMT_ERROR_NONE)
                {
                    Server_DisconnectClient(server);
                    break;
                }

                // Check for more...
                continue;
            }

            // Passable errors...
            if (error == RMT_ERROR_SOCKET_RECV_NO_DATA)
            {
                // No data available
                break;
            }

            if (error == RMT_ERROR_SOCKET_RECV_TIMEOUT)
            {
                // Data not available yet, can afford to ignore as we're only reading the first byte
                break;
            }

            // Anything else is an error that may have closed the connection
            Server_DisconnectClient(server);
            break;
        }
    }

    // Send pings to the client every second
    cur_time = msTimer_Get();
    if (cur_time - server->last_ping_time > 1000)
    {
        Buffer* bin_buf = server->bin_buf;
        WebSocket_PrepareBuffer(bin_buf);
        Buffer_WriteStringZ(bin_buf, "PING");
        Server_Send(server, bin_buf->data, bin_buf->bytes_used, 10);
        server->last_ping_time = cur_time;
    }
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @SAMPLE: Base Sample Description for CPU by default
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

#define SAMPLE_NAME_LEN 128

typedef struct Sample
{
    // Inherit so that samples can be quickly allocated
    ObjectLink Link;

    enum rmtSampleType type;

    // Hash generated from sample name
    rmtU32 name_hash;

    // Unique, persistent ID among all samples
    rmtU32 unique_id;

    // Null-terminated string storing the hash-prefixed 6-digit colour
    rmtU8 unique_id_html_colour[8];

    // Links to related samples in the tree
    struct Sample* parent;
    struct Sample* first_child;
    struct Sample* last_child;
    struct Sample* next_sibling;

    // Keep track of child count to distinguish from repeated calls to the same function at the same stack level
    // This is also mixed with the callstack hash to allow consistent addressing of any point in the tree
    rmtU32 nb_children;

    // Sample end points and length in microseconds
    rmtU64 us_start;
    rmtU64 us_end;
    rmtU64 us_length;

    // Total sampled length of all children
    rmtU64 us_sampled_length;

    // Number of times this sample was used in a call in aggregate mode, 1 otherwise
    rmtU32 call_count;

    // Current and maximum sample recursion depths
    rmtU16 recurse_depth;
    rmtU16 max_recurse_depth;

} Sample;

static rmtError Sample_Constructor(Sample* sample)
{
    assert(sample != NULL);

    ObjectLink_Constructor((ObjectLink*)sample);

    sample->type = RMT_SampleType_CPU;
    sample->name_hash = 0;
    sample->unique_id = 0;
    sample->unique_id_html_colour[0] = '#';
    sample->unique_id_html_colour[1] = 0;
    sample->unique_id_html_colour[7] = 0;
    sample->parent = NULL;
    sample->first_child = NULL;
    sample->last_child = NULL;
    sample->next_sibling = NULL;
    sample->nb_children = 0;
    sample->us_start = 0;
    sample->us_end = 0;
    sample->us_length = 0;
    sample->us_sampled_length = 0;
    sample->call_count = 0;
    sample->recurse_depth = 0;
    sample->max_recurse_depth = 0;

    return RMT_ERROR_NONE;
}

static void Sample_Destructor(Sample* sample)
{
    RMT_UNREFERENCED_PARAMETER(sample);
}

static void Sample_Prepare(Sample* sample, rmtU32 name_hash, Sample* parent)
{
    sample->name_hash = name_hash;
    sample->unique_id = 0;
    sample->parent = parent;
    sample->first_child = NULL;
    sample->last_child = NULL;
    sample->next_sibling = NULL;
    sample->nb_children = 0;
    sample->us_start = 0;
    sample->us_end = 0;
    sample->us_length = 0;
    sample->us_sampled_length = 0;
    sample->call_count = 1;
    sample->recurse_depth = 0;
    sample->max_recurse_depth = 0;
}

static void Sample_Close(Sample* sample, rmtU64 us_end)
{
    // Aggregate samples use us_end to store start so that us_start is preserved
    rmtU64 us_length = 0;
    if (sample->call_count > 1 && sample->max_recurse_depth == 0)
    {
        us_length = (us_end - sample->us_end);
    }
    else
    {
        us_length = (us_end - sample->us_start);
    }

    sample->us_length += us_length;

    // Sum length on the parent to track un-sampled time in the parent
    if (sample->parent != NULL)
    {
        sample->parent->us_sampled_length += us_length;
    }
}

static void Sample_CopyState(Sample* dst_sample, const Sample* src_sample)
{
    // Copy fields that don't override destination allocator links or transfer source sample tree positioning
    // Also ignoring unique_id_html_colour as that's calculated in the Remotery thread
    dst_sample->type = src_sample->type;
    dst_sample->name_hash = src_sample->name_hash;
    dst_sample->unique_id = src_sample->unique_id;
    dst_sample->nb_children = src_sample->nb_children;
    dst_sample->us_start = src_sample->us_start;
    dst_sample->us_end = src_sample->us_end;
    dst_sample->us_length = src_sample->us_length;
    dst_sample->us_sampled_length = src_sample->us_sampled_length;
    dst_sample->call_count = src_sample->call_count;
    dst_sample->recurse_depth = src_sample->recurse_depth;
    dst_sample->max_recurse_depth = src_sample->max_recurse_depth;

    // Prepare empty tree links
    dst_sample->parent = NULL;
    dst_sample->first_child = NULL;
    dst_sample->last_child = NULL;
    dst_sample->next_sibling = NULL;
}

#define BIN_ERROR_CHECK(stmt)        \
    {                                \
        error = stmt;                \
        if (error != RMT_ERROR_NONE) \
            return error;            \
    }

static rmtError bin_SampleArray(Buffer* buffer, Sample* parent_sample);

static rmtError bin_Sample(Buffer* buffer, Sample* sample)
{
    rmtError error;

    assert(sample != NULL);

    BIN_ERROR_CHECK(Buffer_WriteU32(buffer, sample->name_hash));
    BIN_ERROR_CHECK(Buffer_WriteU32(buffer, sample->unique_id));
    BIN_ERROR_CHECK(Buffer_Write(buffer, sample->unique_id_html_colour, 7));
    BIN_ERROR_CHECK(Buffer_WriteU64(buffer, sample->us_start));
    BIN_ERROR_CHECK(Buffer_WriteU64(buffer, sample->us_length));
    BIN_ERROR_CHECK(Buffer_WriteU64(buffer, maxS64(sample->us_length - sample->us_sampled_length, 0)));
    BIN_ERROR_CHECK(Buffer_WriteU32(buffer, sample->call_count));
    BIN_ERROR_CHECK(Buffer_WriteU32(buffer, sample->max_recurse_depth));
    BIN_ERROR_CHECK(bin_SampleArray(buffer, sample));

    return RMT_ERROR_NONE;
}

static rmtError bin_SampleArray(Buffer* buffer, Sample* parent_sample)
{
    rmtError error;
    Sample* sample;

    BIN_ERROR_CHECK(Buffer_WriteU32(buffer, parent_sample->nb_children));
    for (sample = parent_sample->first_child; sample != NULL; sample = sample->next_sibling)
        BIN_ERROR_CHECK(bin_Sample(buffer, sample));

    return RMT_ERROR_NONE;
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @SAMPLETREE: A tree of samples with their allocator
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

typedef struct SampleTree
{
    // Allocator for all samples
    ObjectAllocator* allocator;

    // Root sample for all samples created by this thread
    Sample* root;

    // Most recently pushed sample
    Sample* currentParent;

    // Last time this sample tree was completed and sent to listeners, for stall detection
    rmtU32 msLastTreeSendTime;

    // Lightweight flag, changed with release/acquire semantics to inform the stall detector the state of the tree is unreliable
    rmtU32 treeBeingModified;

} SampleTree;

// Notify tree watchers that its structure is in the process of being changed
#define ModifySampleTree(tree, statements)      \
    StoreRelease(&tree->treeBeingModified, 1);  \
    statements;                                 \
    StoreRelease(&tree->treeBeingModified, 0);

static rmtError SampleTree_Constructor(SampleTree* tree, rmtU32 sample_size, ObjConstructor constructor,
                                       ObjDestructor destructor)
{
    rmtError error;

    assert(tree != NULL);

    tree->allocator = NULL;
    tree->root = NULL;
    tree->currentParent = NULL;
    StoreRelease(&tree->msLastTreeSendTime, 0);
    StoreRelease(&tree->treeBeingModified, 0);

    // Create the sample allocator
    New_3(ObjectAllocator, tree->allocator, sample_size, constructor, destructor);
    if (error != RMT_ERROR_NONE)
    {
        return error;
    }

    // Create a root sample that's around for the lifetime of the thread
    error = ObjectAllocator_Alloc(tree->allocator, (void**)&tree->root);
    if (error != RMT_ERROR_NONE)
    {
        return error;
    }
    Sample_Prepare(tree->root, 0, NULL);
    tree->currentParent = tree->root;

    return RMT_ERROR_NONE;
}

static void SampleTree_Destructor(SampleTree* tree)
{
    assert(tree != NULL);

    if (tree->root != NULL)
    {
        ObjectAllocator_Free(tree->allocator, tree->root);
        tree->root = NULL;
    }

    Delete(ObjectAllocator, tree->allocator);
}

static rmtU32 HashCombine(rmtU32 hash_a, rmtU32 hash_b)
{
    // A sequence of 32 uniformly random bits so that each bit of the combined hash is changed on application
    // Derived from the golden ratio: UINT_MAX / ((1 + sqrt(5)) / 2)
    // In reality it's just an arbitrary value which happens to work well, avoiding mapping all zeros to zeros.
    // http://burtleburtle.net/bob/hash/doobs.html
    static rmtU32 random_bits = 0x9E3779B9;
    hash_a ^= hash_b + random_bits + (hash_a << 6) + (hash_a >> 2);
    return hash_a;
}

static rmtError SampleTree_Push(SampleTree* tree, rmtU32 name_hash, rmtU32 flags, Sample** sample)
{
    Sample* parent;
    rmtError error;
    rmtU32 unique_id;

    // As each tree has a root sample node allocated, a parent must always be present
    assert(tree != NULL);
    assert(tree->currentParent != NULL);
    parent = tree->currentParent;

    // Assume no flags is the common case and predicate branch checks
    if (flags != 0)
    {
        // Check root status
        if ((flags & RMTSF_Root) != 0)
        {
            assert(parent->parent == NULL);
        }

        if ((flags & RMTSF_Aggregate) != 0)
        {
            // Linear search for previous instance of this sample name
            Sample* sibling;
            for (sibling = parent->first_child; sibling != NULL; sibling = sibling->next_sibling)
            {
                if (sibling->name_hash == name_hash)
                {
                    tree->currentParent = sibling;
                    sibling->call_count++;
                    *sample = sibling;
                    return RMT_ERROR_NONE;
                }
            }
        }

        // Collapse sample on recursion
        if ((flags & RMTSF_Recursive) != 0 && parent->name_hash == name_hash)
        {
            parent->recurse_depth++;
            parent->max_recurse_depth = maxU16(parent->max_recurse_depth, parent->recurse_depth);
            parent->call_count++;
            *sample = parent;
            return RMT_ERROR_RECURSIVE_SAMPLE;
        }
    }

    // Allocate a new sample
    error = ObjectAllocator_Alloc(tree->allocator, (void**)sample);
    if (error != RMT_ERROR_NONE)
    {
        return error;
    }
    Sample_Prepare(*sample, name_hash, parent);

    // Generate a unique ID for this sample in the tree
    unique_id = parent->unique_id;
    unique_id = HashCombine(unique_id, (*sample)->name_hash);
    unique_id = HashCombine(unique_id, parent->nb_children);
    (*sample)->unique_id = unique_id;

    // Add sample to its parent
    parent->nb_children++;
    if (parent->first_child == NULL)
    {
        parent->first_child = *sample;
        parent->last_child = *sample;
    }
    else
    {
        assert(parent->last_child != NULL);
        parent->last_child->next_sibling = *sample;
        parent->last_child = *sample;
    }

    // Make this sample the new parent of any newly created samples
    tree->currentParent = *sample;

    return RMT_ERROR_NONE;
}

static void SampleTree_Pop(SampleTree* tree, Sample* sample)
{
    assert(tree != NULL);
    assert(sample != NULL);
    assert(sample != tree->root);
    tree->currentParent = sample->parent;
}

static ObjectLink* FlattenSamples(Sample* sample, rmtU32* nb_samples)
{
    Sample* child;
    ObjectLink* cur_link = &sample->Link;

    assert(sample != NULL);
    assert(nb_samples != NULL);

    *nb_samples += 1;
    sample->Link.next = (ObjectLink*)sample->first_child;

    // Link all children together
    for (child = sample->first_child; child != NULL; child = child->next_sibling)
    {
        ObjectLink* last_link = FlattenSamples(child, nb_samples);
        last_link->next = (ObjectLink*)child->next_sibling;
        cur_link = last_link;
    }

    // Clear child info
    sample->first_child = NULL;
    sample->last_child = NULL;
    sample->nb_children = 0;

    return cur_link;
}

static void FreeSamples(Sample* sample, ObjectAllocator* allocator)
{
    // Chain all samples together in a flat list
    rmtU32 nb_cleared_samples = 0;
    ObjectLink* last_link = FlattenSamples(sample, &nb_cleared_samples);

    // Release the complete sample memory range
    if (sample->Link.next != NULL)
    {
        ObjectAllocator_FreeRange(allocator, sample, last_link, nb_cleared_samples);
    }
    else
    {
        ObjectAllocator_Free(allocator, sample);
    }
}

static rmtError SampleTree_CopySample(Sample** out_dst_sample, Sample* dst_parent_sample, ObjectAllocator* allocator, const Sample* src_sample)
{
    Sample* src_child;

    // Allocate a copy of the sample
    Sample* dst_sample;
    rmtError error = ObjectAllocator_Alloc(allocator, (void**)&dst_sample);
    if (error != RMT_ERROR_NONE)
    {
        return error;
    }
    Sample_CopyState(dst_sample, src_sample);

    // Link the newly created/copied sample to its parent
    // Note that metrics including nb_children have already been copied by the Sample_CopyState call
    if (dst_parent_sample != NULL)
    {
        if (dst_parent_sample->first_child == NULL)
        {
            dst_parent_sample->first_child = dst_sample;
            dst_parent_sample->last_child = dst_sample;
        }
        else
        {
            assert(dst_parent_sample->last_child != NULL);
            dst_parent_sample->last_child->next_sibling = dst_sample;
            dst_parent_sample->last_child = dst_sample;
        }
    }

    // Copy all children
    for (src_child = src_sample->first_child; src_child != NULL; src_child = src_child->next_sibling)
    {
        Sample* dst_child;
        error = SampleTree_CopySample(&dst_child, dst_sample, allocator, src_child);
        if (error != RMT_ERROR_NONE)
        {
            return error;
        }
    }

    *out_dst_sample = dst_sample;

    return RMT_ERROR_NONE;
}

static rmtError SampleTree_Copy(SampleTree* dst_tree, const SampleTree* src_tree)
{
    rmtError error;

    // Sample trees are allocated at startup and their allocators are persistent for the lifetime of the Remotery object.
    // It's safe to reference the allocator and use it for sample lifetime.
    ObjectAllocator* allocator = src_tree->allocator;
    dst_tree->allocator = allocator;

    // Copy from the root
    error = SampleTree_CopySample(&dst_tree->root, NULL, allocator, src_tree->root);
    if (error != RMT_ERROR_NONE)
    {
        return error;
    }
    dst_tree->currentParent = dst_tree->root;

    return RMT_ERROR_NONE;
}

typedef struct Msg_SampleTree
{
    Sample* rootSample;

    ObjectAllocator* allocator;

    rmtPStr threadName;

    rmtBool partialTree;
} Msg_SampleTree;

static void QueueSampleTree(rmtMessageQueue* queue, Sample* sample, ObjectAllocator* allocator, rmtPStr thread_name,
                            struct ThreadProfiler* thread_profiler, rmtBool partial_tree)
{
    Msg_SampleTree* payload;

    // Attempt to allocate a message for sending the tree to the viewer
    Message* message = rmtMessageQueue_AllocMessage(queue, sizeof(Msg_SampleTree), thread_profiler);
    if (message == NULL)
    {
        // Discard tree samples on failure
        FreeSamples(sample, allocator);
        return;
    }

    // Populate and commit
    payload = (Msg_SampleTree*)message->payload;
    payload->rootSample = sample;
    payload->allocator = allocator;
    payload->threadName = thread_name;
    payload->partialTree = partial_tree;
    rmtMessageQueue_CommitMessage(message, MsgID_SampleTree);
}

typedef struct Msg_AddToStringTable
{
    rmtU32 hash;
    rmtU32 length;
} Msg_AddToStringTable;

static rmtBool QueueAddToStringTable(rmtMessageQueue* queue, rmtU32 hash, const char* string, size_t length, struct ThreadProfiler* thread_profiler)
{
    Msg_AddToStringTable* payload;

    // Attempt to allocate a message om the queue
    size_t nb_string_bytes = length + 1;
    Message* message = rmtMessageQueue_AllocMessage(queue, sizeof(Msg_AddToStringTable) + nb_string_bytes, thread_profiler);
    if (message == NULL)
    {
        return RMT_FALSE;
    }

    // Populate and commit
    payload = (Msg_AddToStringTable*)message->payload;
    payload->hash = hash;
    payload->length = length;
    memcpy(payload + 1, string, nb_string_bytes);
    rmtMessageQueue_CommitMessage(message, MsgID_AddToStringTable);

    return RMT_TRUE;
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @TPROFILER: Thread Profiler data, storing both sampling and instrumentation results
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

#if RMT_USE_D3D11
typedef struct D3D11 D3D11;
static rmtError D3D11_Create(D3D11** d3d11);
static void D3D11_Destructor(D3D11* d3d11);
#endif

typedef struct ThreadProfiler
{
    // Storage for backing up initial register values when modifying a thread's context
    rmtU64 registerBackup0;                                                                         // 0
    rmtU64 registerBackup1;                                                                         // 8
    rmtU64 registerBackup2;                                                                         // 16

    // Used to schedule callbacks taking into account some threads may be sleeping
    rmtS32 nbSamplesWithoutCallback;                                                                // 24

    // Index of the processor the thread was last seen running on
    rmtU32 processorIndex;                                                                          // 28
    rmtU32 lastProcessorIndex;

    // OS thread ID/handle
    rmtThreadId threadId;
    rmtThreadHandle threadHandle;

    // Thread name stored for sending to the viewer
    char threadName[64];
    rmtU32 threadNameHash;

    // Store a unique sample tree for each type
    SampleTree* sampleTrees[RMT_SampleType_Count];

#if RMT_USE_D3D11
    D3D11* d3d11;
#endif
} ThreadProfiler;

static rmtError QueueThreadName(rmtMessageQueue* queue, const char* name, ThreadProfiler* thread_profiler)
{
    Message* message;
    rmtU32 name_length;

    assert(queue != NULL);

    // Allocate some space for the message
    name_length = strnlen_s(name, 64);
    message = rmtMessageQueue_AllocMessage(queue, sizeof(rmtU32) * 2 + name_length, thread_profiler);
    if (message == NULL)
    {
        return RMT_ERROR_UNKNOWN;
    }

    // Copy and commit
    U32ToByteArray(message->payload, MurmurHash3_x86_32(name, name_length, 0));
    U32ToByteArray(message->payload + sizeof(rmtU32), name_length);
    memcpy(message->payload + sizeof(rmtU32) * 2, name, name_length);
    rmtMessageQueue_CommitMessage(message, MsgID_ThreadName);

    return RMT_ERROR_NONE;
}

static rmtError ThreadProfiler_Constructor(rmtMessageQueue* mq_to_rmt, ThreadProfiler* thread_profiler, rmtThreadId thread_id)
{
    rmtU32 name_length;
    rmtError error;

    // Set defaults
    thread_profiler->nbSamplesWithoutCallback = 0;
    thread_profiler->processorIndex = (rmtU32)-1;
    thread_profiler->lastProcessorIndex = (rmtU32)-1;
    thread_profiler->threadId = thread_id;
    memset(thread_profiler->sampleTrees, 0, sizeof(thread_profiler->sampleTrees));

#if RMT_USE_D3D11
    thread_profiler->d3d11 = NULL;
#endif

    // Pre-open the thread handle
    error = rmtOpenThreadHandle(thread_id, &thread_profiler->threadHandle);
    if (error != RMT_ERROR_NONE)
    {
        return error;
    }

    // Name the thread and send a thread name notification immediately
    // Users can override this at a later point with the Remotery thread name API
    rmtGetThreadName(thread_id, thread_profiler->threadHandle, thread_profiler->threadName, sizeof(thread_profiler->threadName));
    name_length = strnlen_s(thread_profiler->threadName, 64);
    thread_profiler->threadNameHash = MurmurHash3_x86_32(thread_profiler->threadName, name_length, 0);
    QueueThreadName(mq_to_rmt, thread_profiler->threadName, thread_profiler);

    // Create the CPU sample tree only. The rest are created on-demand as they need extra context to function correctly.
    New_3(SampleTree, thread_profiler->sampleTrees[RMT_SampleType_CPU], sizeof(Sample), (ObjConstructor)Sample_Constructor,
          (ObjDestructor)Sample_Destructor);
    if (error != RMT_ERROR_NONE)
    {
        return error;
    }

#if RMT_USE_D3D11
    error = D3D11_Create(&thread_profiler->d3d11);
    if (error != RMT_ERROR_NONE)
    {
        return error;
    }
#endif

    return RMT_ERROR_NONE;
}

static void ThreadProfiler_Destructor(ThreadProfiler* thread_profiler)
{
    rmtU32 index;

#if RMT_USE_D3D11
    Delete(D3D11, thread_profiler->d3d11);
#endif

    for (index = 0; index < RMT_SampleType_Count; index++)
    {
        Delete(SampleTree, thread_profiler->sampleTrees[index]);
    }

    rmtCloseThreadHandle(thread_profiler->threadHandle);
}

static rmtError ThreadProfiler_Push(SampleTree* tree, rmtU32 name_hash, rmtU32 flags, Sample** sample)
{
    rmtError error;
    ModifySampleTree(tree,
        error = SampleTree_Push(tree, name_hash, flags, sample);
    );
    return error;
}

static rmtBool ThreadProfiler_Pop(ThreadProfiler* thread_profiler, rmtMessageQueue* queue, Sample* sample)
{
    SampleTree* tree = thread_profiler->sampleTrees[sample->type];
    SampleTree_Pop(tree, sample);

    // Are we back at the root?
    if (tree->currentParent == tree->root)
    {
        Sample* root;

        // Disconnect all samples from the root and pack in the chosen message queue
        ModifySampleTree(tree,
        root = tree->root;
        root->first_child = NULL;
        root->last_child = NULL;
        root->nb_children = 0;
        );
        QueueSampleTree(queue, sample, tree->allocator, thread_profiler->threadName, thread_profiler, RMT_FALSE);

        // Update the last send time for this tree, for stall detection
        StoreRelease(&tree->msLastTreeSendTime, (rmtU32)(sample->us_end / 1000));

        return RMT_TRUE;
    }

    return RMT_FALSE;
}

static rmtU32 ThreadProfiler_GetNameHash(ThreadProfiler* thread_profiler, rmtMessageQueue* queue, rmtPStr name, rmtU32* hash_cache)
{
    size_t name_len;
    rmtU32 name_hash;

    // Hash cache provided?
    if (hash_cache != NULL)
    {
        // Calculate the hash first time round only
        name_hash = *hash_cache;
        if (name_hash == 0)
        {
            assert(name != NULL);
            name_len = strnlen_s(name, 256);
            name_hash = MurmurHash3_x86_32(name, name_len, 0);

            // Queue the string for the string table and only cache the hash if it succeeds
            if (QueueAddToStringTable(queue, name_hash, name, name_len, thread_profiler) == RMT_TRUE)
            {
                *hash_cache = name_hash;
            }
        }

        return name_hash;
    }

    // Have to recalculate and speculatively insert the name every time when no cache storage exists
    name_len = strnlen_s(name, 256);
    name_hash = MurmurHash3_x86_32(name, name_len, 0);
    QueueAddToStringTable(queue, name_hash, name, name_len, thread_profiler);
    return name_hash;
}

typedef struct ThreadProfilers
{
    // Timer shared with Remotery threads
    usTimer* timer;

    // Queue between clients and main remotery thread
    rmtMessageQueue* mqToRmtThread;

    // On x64 machines this points to the sample function
    void* compiledSampleFn;
    rmtU32 compiledSampleFnSize;

    // Used to store thread profilers bound to an OS thread
    rmtTLS threadProfilerTlsHandle;

    // Array of preallocated ThreadProfiler objects
    // Read iteration is safe given that no incomplete ThreadProfiler objects will be encountered during iteration.
    // The ThreadProfiler count is only incremented once a new ThreadProfiler is fully defined and ready to be used.
    // Do not use this list to verify if a ThreadProfiler exists for a given thread. Use the mutex-guarded Get functions instead.
    ThreadProfiler threadProfilers[256];
    rmtU32 nbThreadProfilers;
    rmtU32 maxNbThreadProfilers;

    // Guards creation and existence-testing of the ThreadProfiler list
    rmtMutex threadProfilerMutex;

    // Periodic thread sampling thread
    rmtThread* threadSampleThread;

    // Periodic thread to processor gatherer
    rmtThread* threadGatherThread;
} ThreadProfilers;

static rmtError SampleThreadsLoop(rmtThread* rmt_thread);

#ifdef RMT_PLATFORM_WINDOWS
#ifdef RMT_ARCH_64BIT
static void* CreateSampleCallback(rmtU32* out_size);
#endif
#endif

static rmtError ThreadProfilers_Constructor(ThreadProfilers* thread_profilers, usTimer* timer, rmtMessageQueue* mq_to_rmt_thread)
{
    rmtError error;

    // Set to default
    thread_profilers->timer = timer;
    thread_profilers->mqToRmtThread = mq_to_rmt_thread;
    thread_profilers->compiledSampleFn = NULL;
    thread_profilers->compiledSampleFnSize = 0;
    thread_profilers->threadProfilerTlsHandle = TLS_INVALID_HANDLE;
    thread_profilers->nbThreadProfilers = 0;
    thread_profilers->maxNbThreadProfilers = sizeof(thread_profilers->threadProfilers) / sizeof(thread_profilers->threadProfilers[0]);
    mtxInit(&thread_profilers->threadProfilerMutex);
    thread_profilers->threadSampleThread = NULL;
    thread_profilers->threadGatherThread = NULL;

#ifdef RMT_PLATFORM_WINDOWS
#ifdef RMT_ARCH_64BIT
    thread_profilers->compiledSampleFn = CreateSampleCallback(&thread_profilers->compiledSampleFnSize);
    if (thread_profilers->compiledSampleFn == NULL)
    {
        return RMT_ERROR_MALLOC_FAIL;
    }
#endif
#endif

    // Allocate a TLS handle for the thread profilers
    error = tlsAlloc(&thread_profilers->threadProfilerTlsHandle);
    if (error != RMT_ERROR_NONE)
    {
        return error;
    }

    // Kick-off the thread sampler
    if (g_Settings.enableThreadSampler == RMT_TRUE)
    {
        New_2(rmtThread, thread_profilers->threadSampleThread, SampleThreadsLoop, thread_profilers);
        if (error != RMT_ERROR_NONE)
        {
            return error;
        }
    }

    return RMT_ERROR_NONE;
}

static void ThreadProfilers_Destructor(ThreadProfilers* thread_profilers)
{
    rmtU32 thread_index;

    Delete(rmtThread, thread_profilers->threadGatherThread);
    Delete(rmtThread, thread_profilers->threadSampleThread);

    // Delete all profilers
    for (thread_index = 0; thread_index < thread_profilers->nbThreadProfilers; thread_index++)
    {
        ThreadProfiler* thread_profiler = thread_profilers->threadProfilers + thread_index;
        ThreadProfiler_Destructor(thread_profiler);
    }

    if (thread_profilers->threadProfilerTlsHandle != TLS_INVALID_HANDLE)
    {
        tlsFree(thread_profilers->threadProfilerTlsHandle);
    }

#ifdef RMT_PLATFORM_WINDOWS
#ifdef RMT_ARCH_64BIT
    if (thread_profilers->compiledSampleFn != NULL)
    {
        VirtualFree(thread_profilers->compiledSampleFn, 0, MEM_RELEASE);
    }
#endif
#endif

    mtxDelete(&thread_profilers->threadProfilerMutex);
}

static rmtError ThreadProfilers_GetThreadProfiler(ThreadProfilers* thread_profilers, rmtThreadId thread_id, ThreadProfiler** out_thread_profiler)
{
    rmtU32 profiler_index;
    ThreadProfiler* thread_profiler;
    rmtError error;

    mtxLock(&thread_profilers->threadProfilerMutex);

    // Linear search for a matching thread id
    for (profiler_index = 0; profiler_index < thread_profilers->nbThreadProfilers; profiler_index++)
    {
        thread_profiler = thread_profilers->threadProfilers + profiler_index;
        if (thread_profiler->threadId == thread_id)
        {
            *out_thread_profiler = thread_profiler;
            mtxUnlock(&thread_profilers->threadProfilerMutex);
            return RMT_ERROR_NONE;
        }
    }

    // Thread info not found so create a new one at the end
    thread_profiler = thread_profilers->threadProfilers + thread_profilers->nbThreadProfilers;
    error = ThreadProfiler_Constructor(thread_profilers->mqToRmtThread, thread_profiler, thread_id);
    if (error != RMT_ERROR_NONE)
    {
        ThreadProfiler_Destructor(thread_profiler);
        mtxUnlock(&thread_profilers->threadProfilerMutex);
        return error;
    }
    *out_thread_profiler = thread_profiler;

    // Increment count for consume by read iterators
    // Within the mutex so that there are no race conditions creating thread profilers
    // Using release semantics to ensure a memory barrier for read iterators
    StoreRelease(&thread_profilers->nbThreadProfilers, thread_profilers->nbThreadProfilers + 1);

    mtxUnlock(&thread_profilers->threadProfilerMutex);

    return RMT_ERROR_NONE;
}

static rmtError ThreadProfilers_GetCurrentThreadProfiler(ThreadProfilers* thread_profilers, ThreadProfiler** out_thread_profiler)
{
    // Is there a thread profiler associated with this thread yet?
    *out_thread_profiler = (ThreadProfiler*)tlsGet(thread_profilers->threadProfilerTlsHandle);
    if (*out_thread_profiler == NULL)
    {
        // Allocate on-demand
        rmtError error = ThreadProfilers_GetThreadProfiler(thread_profilers, rmtGetCurrentThreadId(), out_thread_profiler);
        if (error != RMT_ERROR_NONE)
        {
            return error;
        }

        // Bind to the curren thread
        tlsSet(thread_profilers->threadProfilerTlsHandle, *out_thread_profiler);
    }

    return RMT_ERROR_NONE;
}

static rmtBool ThreadProfilers_ThreadInCallback(ThreadProfilers* thread_profilers, rmtCpuContext* context)
{
#ifdef RMT_PLATFORM_WINDOWS
#ifdef RMT_ARCH_32BIT
    if (context->Eip >= (DWORD)thread_profilers->compiledSampleFn &&
        context->Eip < (DWORD)((char*)thread_profilers->compiledSampleFn + thread_profilers->compiledSampleFnSize))
    {
        return RMT_TRUE;
    }
#else
    if (context->Rip >= (DWORD64)thread_profilers->compiledSampleFn &&
        context->Rip < (DWORD64)((char*)thread_profilers->compiledSampleFn + thread_profilers->compiledSampleFnSize))
    {
        return RMT_TRUE;
    }
#endif
#endif
    return RMT_FALSE;
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @TGATHER: Thread Gatherer, periodically polling for newly created threads
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

static void GatherThreads(ThreadProfilers* thread_profilers)
{
    rmtThreadHandle handle;

    assert(thread_profilers != NULL);

#ifdef RMT_PLATFORM_WINDOWS

    // Create the snapshot - this is a slow call
    handle = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (handle != INVALID_HANDLE_VALUE)
    {
        BOOL success;

        THREADENTRY32 thread_entry;
        thread_entry.dwSize = sizeof(thread_entry);

        // Loop through all threads owned by this process
        success = Thread32First(handle, &thread_entry);
        while (success == TRUE)
        {
            if (thread_entry.th32OwnerProcessID == GetCurrentProcessId())
            {
                // Create thread profilers on-demand if there're not already there
                ThreadProfiler* thread_profiler;
                rmtError error = ThreadProfilers_GetThreadProfiler(thread_profilers, thread_entry.th32ThreadID, &thread_profiler);
                if (error != RMT_ERROR_NONE)
                {
                    // Not really worth bringing the whole profiler down here
                    rmt_LogText("REMOTERY ERROR: Failed to create Thread Profiler");
                }
            }

            success = Thread32Next(handle, &thread_entry);
        }

        CloseHandle(handle);
    }

#endif
}

static rmtError GatherThreadsLoop(rmtThread* thread)
{
    ThreadProfilers* thread_profilers = (ThreadProfilers*)thread->param;
    rmtU32 sleep_time = 100;

    assert(thread_profilers != NULL);

    rmt_SetCurrentThreadName("RemoteryGatherThreads");

    while (thread->request_exit == RMT_FALSE)
    {
        // We want a long period of time between scanning for new threads as the process is a little expensive (~30ms here).
        // However not too long so as to miss potentially detailed process startup data.
        // Use reduced sleep time at startup to catch as many early thread creations as possible.
        // TODO(don): We could get processes to register themselves to ensure no startup data is lost but the scan must still
        // be present, to catch threads in a process that the user doesn't create (e.g. graphics driver threads).
        GatherThreads(thread_profilers);
        msSleep(sleep_time);
        sleep_time = minU32(sleep_time * 2, 2000);
    }

    return RMT_ERROR_NONE;
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @TSAMPLER: Sampling thread contexts
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

typedef struct Processor
{
    // Current thread profiler sampling this processor
    ThreadProfiler* threadProfiler;

    rmtU32 sampleCount;
    rmtU64 sampleTime;
} Processor;

typedef struct Msg_ProcessorThreads
{
    // Running index of processor messages
    rmtU64 messageIndex;

    // Processor array, leaking into the memory behind the struct
    rmtU32 nbProcessors;
    Processor processors[1];
} Msg_ProcessorThreads;

static void QueueProcessorThreads(rmtMessageQueue* queue, rmtU64 message_index, rmtU32 nb_processors, Processor* processors)
{
    Msg_ProcessorThreads* payload;

    // Attempt to allocate a message for sending processors to the viewer
    rmtU32 array_size = (nb_processors - 1) * sizeof(Processor);
    Message* message = rmtMessageQueue_AllocMessage(queue, sizeof(Msg_ProcessorThreads) + array_size, NULL);
    if (message == NULL)
    {
        return;
    }

    // Populate and commit
    payload = (Msg_ProcessorThreads*)message->payload;
    payload->messageIndex = message_index;
    payload->nbProcessors = nb_processors;
    memcpy(payload->processors, processors, nb_processors * sizeof(Processor));
    rmtMessageQueue_CommitMessage(message, MsgID_ProcessorThreads);
}

#ifdef RMT_ARCH_32BIT
__declspec(naked) static void SampleCallback()
{
    //
    // It's important to realise that this call can be pre-empted by the scheduler and shifted to another processor *while we are
    // sampling which processor this thread is on*.
    //
    // This has two very important implications:
    //
    //    * What we are sampling here is an *approximation* of the path of threads across processors.
    //    * These samples can't be used to "open" and "close" sample periods on a processor as it's highly likely you'll get many
    //      open events without a close, or vice versa.
    //
    // As such, we can only choose a sampling period and for each sample register which threads are on which processor.
    //
    // This is very different to hooking up the Event Tracing API (requiring Administrator elevation), which raises events for
    // each context switch, directly from the kernel.
    //

    __asm
    {
        // Push the EIP return address used by the final ret instruction
        push ebx

        // We might be in the middle of something like a cmp/jmp instruction pair so preserve EFLAGS
        // (Classic example which seems to pop up regularly is _RTC_CheckESP, with cmp/call/jne)
        pushfd

        // Push all volatile registers as we don't know what the function calls below will destroy
        push eax
        push ecx
        push edx

        // Retrieve and store the current processor index
        call esi
        mov [edi].processorIndex, eax

        // Mark as ready for scheduling another callback
        // Intel x86 store release
        mov [edi].nbSamplesWithoutCallback, 0

        // Restore preserved register state
        pop edx
        pop ecx
        pop eax

        // Restore registers used to provide parameters to the callback
        mov ebx, dword ptr [edi].registerBackup0
        mov esi, dword ptr [edi].registerBackup1
        mov edi, dword ptr [edi].registerBackup2

        // Restore EFLAGS
        popfd

        // Pops the original EIP off the stack and jmps to origin suspend point in the thread
        ret
    }
}
#elif defined(RMT_ARCH_64BIT)
// Generated with https://defuse.ca/online-x86-assembler.htm
static rmtU8 SampleCallbackBytes[] =
{
    // Push the RIP return address used by the final ret instruction
    0x53,                                           // push rbx

    // We might be in the middle of something like a cmp/jmp instruction pair so preserve RFLAGS
    // (Classic example which seems to pop up regularly is _RTC_CheckESP, with cmp/call/jne)
    0x9C,                                           // pushfq

    // Push all volatile registers as we don't know what the function calls below will destroy
    0x50,                                           // push rax
    0x51,                                           // push rcx
    0x52,                                           // push rdx
    0x41, 0x50,                                     // push r8
    0x41, 0x51,                                     // push r9
    0x41, 0x52,                                     // push r10
    0x41, 0x53,                                     // push r11

    // Retrieve and store the current processor index
    0xFF, 0xD6,                                     // call rsi
    0x89, 0x47, 0x1C,                               // mov dword ptr [rdi + 28], eax

    // Mark as ready for scheduling another callback
    // Intel x64 store release
    0xC7, 0x47, 0x18, 0x00, 0x00, 0x00, 0x00,       // mov dword ptr [rdi + 24], 0

    // Restore preserved register state
    0x41, 0x5B,                                     // pop r11
    0x41, 0x5A,                                     // pop r10
    0x41, 0x59,                                     // pop r9
    0x41, 0x58,                                     // pop r8
    0x5A,                                           // pop rdx
    0x59,                                           // pop rcx
    0x58,                                           // pop rax

    // Restore registers used to provide parameters to the callback
    0x48, 0x8B, 0x1F,                               // mov rbx, qword ptr [rdi + 0]
    0x48, 0x8B, 0x77, 0x08,                         // mov rsi, qword ptr [rdi + 8]
    0x48, 0x8B, 0x7F, 0x10,                         // mov rdi, qword ptr [rdi + 16]

    // Restore RFLAGS
    0x9D,                                           // popfq

    // Pops the original EIP off the stack and jmps to origin suspend point in the thread
    0xC3                                            // ret
};
#ifdef RMT_PLATFORM_WINDOWS
static void* CreateSampleCallback(rmtU32* out_size)
{
    // Allocate page for the generated code
    DWORD size = 4096;
    DWORD old_protect;
    void* function = VirtualAlloc(NULL, size, MEM_RESERVE | MEM_COMMIT, PAGE_READWRITE);
    if (function == NULL)
    {
        return NULL;
    }

    // Clear whole allocation to int 3h
    memset(function, 0xCC, size);

    // Copy over the generated code
    memcpy(function, SampleCallbackBytes, sizeof(SampleCallbackBytes));
    *out_size = sizeof(SampleCallbackBytes);

    // Enable execution
    VirtualProtect(function, size, PAGE_EXECUTE_READ, &old_protect);
    return function;
}
#endif
#endif

#if defined(__cplusplus) && __cplusplus >= 201103L
static_assert(offsetof(ThreadProfiler, nbSamplesWithoutCallback) == 24, "");
static_assert(offsetof(ThreadProfiler, processorIndex) == 28, "");
#endif

static void CloseOpenSamples(Sample* sample, rmtU64 sample_time_us, rmtU32 parents_are_last)
{
    Sample* child_sample;

    // Depth-first search into children as we want to close child samples before their parents
    for (child_sample = sample->first_child; child_sample != NULL; child_sample = child_sample->next_sibling)
    {
        rmtU32 is_last = parents_are_last & (child_sample == sample->last_child ? 1 : 0);
        CloseOpenSamples(child_sample, sample_time_us, is_last);
    }

    // A chain of open samples will be linked from the root to the deepest, currently open sample
    if (parents_are_last > 0)
    {
        Sample_Close(sample, sample_time_us);
    }
}

static rmtError CheckForStallingSamples(SampleTree* stalling_sample_tree, ThreadProfiler* thread_profiler, rmtU64 sample_time_us)
{
    SampleTree* sample_tree;
    rmtU32 sample_time_s = (rmtU32)(sample_time_us / 1000);

    // Initialise to empty
    stalling_sample_tree->root = NULL;
    stalling_sample_tree->allocator = NULL;

    // Skip the stall check if the tree is being modified
    sample_tree = thread_profiler->sampleTrees[RMT_SampleType_CPU];
    if (LoadAcquire(&sample_tree->treeBeingModified) != 0)
    {
        return RMT_ERROR_NONE;
    }

    if (sample_tree != NULL)
    {
        // The root is a dummy root inserted on tree creation so check that for children
        rmtBool send = RMT_FALSE;
        Sample* root_sample = sample_tree->root;
        if (root_sample != NULL && root_sample->nb_children > 0)
        {
            if (sample_time_s - LoadAcquire(&sample_tree->msLastTreeSendTime) > 1000)
            {
                send = RMT_TRUE;
                StoreRelease(&sample_tree->msLastTreeSendTime, sample_time_s);
            }
        }

        if (send == RMT_TRUE)
        {
            // Make a local copy of the tree as we want to keep the current tree for active profiling
            rmtError error = SampleTree_Copy(stalling_sample_tree, sample_tree);
            if (error != RMT_ERROR_NONE)
            {
                return error;
            }

            // Close all samples from the deepest open sample, right back to the root
            CloseOpenSamples(stalling_sample_tree->root, sample_time_us, 1);
        }
    }

    return RMT_ERROR_NONE;
}

static rmtError InitThreadSampling(ThreadProfilers* thread_profilers)
{
    rmtError error;

    rmt_SetCurrentThreadName("RemoterySampleThreads");

    // Make an initial gather so that we have something to work with
    GatherThreads(thread_profilers);

    // Kick-off the background thread that watches for new threads
    New_2(rmtThread, thread_profilers->threadGatherThread, GatherThreadsLoop, thread_profilers);
    if (error != RMT_ERROR_NONE)
    {
        return error;
    }

#ifdef RMT_PLATFORM_WINDOWS
    // Ensure we can wake up every millisecond
    if (timeBeginPeriod(1) != TIMERR_NOERROR)
    {
        return RMT_ERROR_UNKNOWN;
    }
#endif

    // We're going to be shuffling thread visits to avoid the scheduler trying to predict a work-load based on sampling
    // Use the global RNG with a random seed to start the shuffle
    Well512_Init((rmtU32)time(NULL));

    return RMT_ERROR_NONE;
}

static rmtError SampleThreadsLoop(rmtThread* rmt_thread)
{
    rmtCpuContext context;
    rmtU32 processor_message_index = 0;
    rmtU32 nb_processors;
    Processor* processors;
    rmtU32 processor_index;

    ThreadProfilers* thread_profilers = (ThreadProfilers*)rmt_thread->param;

    rmtError error = InitThreadSampling(thread_profilers);
    if (error != RMT_ERROR_NONE)
    {
        return error;
    }

    // If we can't figure out how many processors there are then we are running on an unsupported platform
    nb_processors = rmtGetNbProcessors();
    if (nb_processors == 0)
    {
        return RMT_ERROR_UNKNOWN;
    }

    // An array entry for each processor
    processors = (Processor*)rmtMalloc(nb_processors * sizeof(Processor));
    for (processor_index = 0; processor_index < nb_processors; processor_index++)
    {
        processors[processor_index].threadProfiler = NULL;
        processors[processor_index].sampleTime = 0;
    }

    while (rmt_thread->request_exit == RMT_FALSE)
    {
        rmtU32 lfsr_seed;
        rmtU32 lfsr_value;

        // Query how many threads the gather knows about this time round
        rmtU32 nb_thread_profilers = LoadAcquire(&thread_profilers->nbThreadProfilers);

        // Calculate table size log2 required to fit count entries. Normally we would adjust the log2 input by -1 so that
        // power-of-2 counts map to their exact bit offset and don't require a twice larger table. You can iterate indices
        // 0 to (1<<b)-1 and efficiently store the whole range.
        //
        // However, the LFSR will never return 0. If you have a table count of 7 then it will return indices 1 to 7 within the
        // 3=bit range required. Normally this is fine as subtracting one for your index iterates the complete indices 0 to 6.
        //
        // This becomes a problem when your table size is 8 (or any other power-of-2). With the log2 -1 adjustment a count of 8
        // calculates a table size of 8. LFSR iteration will only return 1-7, visiting indices 0-6, missing out the last one.
        //
        // So to counter that let an input count of 8 require a table size of 16. LFSR iteration will return 1-15, visiting
        // indices 0-14 and the required range test will limit that visit to 0-7.
        //
        // Implementation playground: https://godbolt.org/z/qG8T4E
        rmtU32 highest_bit_set = Log2i(nb_thread_profilers);
        rmtU32 table_size_log2 = highest_bit_set + 1;
        rmtU32 xor_mask = GaloisLFSRMask(table_size_log2);

        // Use a LFSR to visit threads in shuffled order
        lfsr_seed = Well512_RandomOpenLimit(nb_thread_profilers);
        lfsr_value = lfsr_seed;
        do
        {
            rmtU32 thread_index;
            rmtU32 thread_id;
            ThreadProfiler* thread_profiler;
            rmtThreadHandle thread_handle;
            rmtU64 sample_time_us;
            rmtU32 sample_count;
            SampleTree stalling_sample_tree;

            lfsr_value = GaloisLFSRNext(lfsr_value, xor_mask);

            // Apply the value-to-index bias and see if this index is within range before processing
            thread_index = lfsr_value - 1;
            if (thread_index >= nb_thread_profilers)
            {
                continue;
            }

            // Ignore our own thread
            thread_id = rmtGetCurrentThreadId();
            thread_profiler = thread_profilers->threadProfilers + thread_index;
            if (thread_profiler->threadId == thread_id)
            {
                continue;
            }

            // Suspend the thread so we can insert a callback
            thread_handle = thread_profiler->threadHandle;
            if (rmtSuspendThread(thread_handle) == RMT_FALSE)
            {
                continue;
            }

            // Mark the processor this thread was last recorded as running on.
            // Note that a thread might be pre-empted multiple times in-between sampling. Given a sampling rate equal to the
            // scheduling quantum, this doesn't happen too often. However in such cases, whoever marks the processor last is
            // the one that gets recorded.
            sample_time_us = usTimer_Get(thread_profilers->timer);
            sample_count = AtomicAdd(&thread_profiler->nbSamplesWithoutCallback, 1);
            processor_index = thread_profiler->processorIndex;
            if (processor_index != (rmtU32)-1)
            {
                assert(processor_index < nb_processors);
                processors[processor_index].threadProfiler = thread_profiler;
                processors[processor_index].sampleCount = sample_count;
                processors[processor_index].sampleTime = sample_time_us;
            }

            // Swap in a new context with our callback if one is not already scheduled on this thread
            if (sample_count == 0)
            {
                if (rmtGetUserModeThreadContext(thread_handle, &context) == RMT_TRUE &&
                    // There is a slight window of opportunity, after which the callback sets nbSamplesWithoutCallback=0,
                    // for this loop to suspend a thread while it's executing the last instructions of the callback.
                    ThreadProfilers_ThreadInCallback(thread_profilers, &context) == RMT_FALSE)
                {
                #ifdef RMT_PLATFORM_WINDOWS
                #ifdef RMT_ARCH_64BIT
                    thread_profiler->registerBackup0 = context.Rbx;
                    thread_profiler->registerBackup1 = context.Rsi;
                    thread_profiler->registerBackup2 = context.Rdi;
                    context.Rbx = context.Rip;
                    context.Rsi = (rmtU64)GetCurrentProcessorNumber;
                    context.Rdi = (rmtU64)thread_profiler;
                    context.Rip = (DWORD64)thread_profilers->compiledSampleFn;
                #endif
                 #ifdef RMT_ARCH_32BIT
                    thread_profiler->registerBackup0 = context.Ebx;
                    thread_profiler->registerBackup1 = context.Esi;
                    thread_profiler->registerBackup2 = context.Edi;
                    context.Ebx = context.Eip;
                    context.Esi = (rmtU32)GetCurrentProcessorNumber;
                    context.Edi = (rmtU32)thread_profiler;
                    context.Eip = (DWORD)&SampleCallback;
                #endif
                #endif

                    rmtSetThreadContext(thread_handle, &context);
                }
                else
                {
                    AtomicAdd(&thread_profiler->nbSamplesWithoutCallback, -1);
                }
            }

            // While the thread is suspended take the chance to check for samples trees that may never complete
            // Because SuspendThread on Windows is an async request, this needs to be placed at a point where the request completes
            // Calling GetThreadContext will ensure the request is completed so this stall check is placed after that
            if (RMT_ERROR_NONE != CheckForStallingSamples(&stalling_sample_tree, thread_profiler, sample_time_us))
            {
                assert(stalling_sample_tree.allocator != NULL);
                if (stalling_sample_tree.root != NULL)
                {
                    FreeSamples(stalling_sample_tree.root, stalling_sample_tree.allocator);
                }
            }

            rmtResumeThread(thread_handle);

            if (stalling_sample_tree.root != NULL)
            {
                // If there is stalling sample tree on this thread then send it to listeners.
                // Do the send *outside* of all Suspend/Resume calls as we have no way of knowing who is reading/writing the queue
                // Mark this as partial so that the listeners know it will be overwritten.
                Sample* sample = stalling_sample_tree.root->first_child;
                assert(sample != NULL);
                QueueSampleTree(thread_profilers->mqToRmtThread, sample, stalling_sample_tree.allocator, thread_profiler->threadName, thread_profiler, RMT_TRUE);

                // The stalling_sample_tree.root->first_child has been sent to the main Remotery thread. This will get released later
                // when the Remotery thread has processed it. This leaves the stalling_sample_tree.root here that must be freed.
                // Before freeing the root sample we have to detach the children though.
                stalling_sample_tree.root->first_child = NULL;
                stalling_sample_tree.root->last_child = NULL;
                stalling_sample_tree.root->nb_children = 0;
                assert(stalling_sample_tree.allocator != NULL);
                FreeSamples(stalling_sample_tree.root, stalling_sample_tree.allocator);
            }


        } while (lfsr_value != lfsr_seed);

        // Filter all processor samples made in this pass
        for (processor_index = 0; processor_index < nb_processors; processor_index++)
        {
            Processor* processor = processors + processor_index;
            ThreadProfiler* thread_profiler = processor->threadProfiler;

            if (thread_profiler != NULL)
            {
                // If this thread was on another processor on a previous pass and that processor is still tracking that thread,
                // remove the thread from it.
                rmtU32 last_processor_index = thread_profiler->lastProcessorIndex;
                if (last_processor_index != (rmtU32)-1 && last_processor_index != processor_index)
                {
                    assert(last_processor_index < nb_processors);
                    if (processors[last_processor_index].threadProfiler == thread_profiler)
                    {
                        processors[last_processor_index].threadProfiler = NULL;
                    }
                }

                // When the thread is still on the same processor, check to see if it hasn't triggered the callback within another
                // pass. This suggests the thread has gone to sleep and is no longer assigned to any thread.
                else if (processor->sampleCount > 1)
                {
                    processor->threadProfiler = NULL;
                }

                thread_profiler->lastProcessorIndex = thread_profiler->processorIndex;
            }
        }

        // Send current processor state off to remotery
        QueueProcessorThreads(thread_profilers->mqToRmtThread, processor_message_index++, nb_processors, processors);
    }

#ifdef RMT_PLATFORM_WINDOWS
    timeEndPeriod(1);
#endif

    rmtFree(processors);

    return RMT_ERROR_NONE;
}

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @REMOTERY: Remotery
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

#if RMT_USE_OPENGL
typedef struct OpenGL_t OpenGL;
static rmtError OpenGL_Create(OpenGL** opengl);
static void OpenGL_Destructor(OpenGL* opengl);
#endif

#if RMT_USE_METAL
typedef struct Metal_t Metal;
static rmtError Metal_Create(Metal** metal);
static void Metal_Destructor(Metal* metal);
#endif

struct Remotery
{
    Server* server;

    // Microsecond accuracy timer for CPU timestamps
    usTimer timer;

    // Queue between clients and main remotery thread
    rmtMessageQueue* mq_to_rmt_thread;

    // The main server thread
    rmtThread* thread;

    // String table shared by all threads
    StringTable* string_table;

    // Open logfile handle to append events to
    FILE* logfile;

    // Set to trigger a map of each message on the remotery thread message queue
    void (*map_message_queue_fn)(Remotery* rmt, Message*);
    void* map_message_queue_data;

#if RMT_USE_CUDA
    rmtCUDABind cuda;
#endif

#if RMT_USE_OPENGL
    OpenGL* opengl;
#endif

#if RMT_USE_METAL
    Metal* metal;
#endif

    ThreadProfilers* threadProfilers;
};

//
// Global remotery context
//
static Remotery* g_Remotery = NULL;

//
// This flag marks the EXE/DLL that created the global remotery instance. We want to allow
// only the creating EXE/DLL to destroy the remotery instance.
//
static rmtBool g_RemoteryCreated = RMT_FALSE;

static const rmtU8 g_DecimalToHex[17] = "0123456789abcdef";

static void GetSampleDigest(Sample* sample, rmtU32* digest_hash, rmtU32* nb_samples)
{
    Sample* child;

    assert(sample != NULL);
    assert(digest_hash != NULL);
    assert(nb_samples != NULL);

    // Concatenate this sample
    (*nb_samples)++;
    *digest_hash = HashCombine(*digest_hash, sample->unique_id);

    {
        rmtU8 shift = 4;

        // Get 6 nibbles for lower 3 bytes of the name hash
        rmtU8* sample_id = (rmtU8*)&sample->name_hash;
        rmtU8 hex_sample_id[6];
        hex_sample_id[0] = sample_id[0] & 15;
        hex_sample_id[1] = sample_id[0] >> 4;
        hex_sample_id[2] = sample_id[1] & 15;
        hex_sample_id[3] = sample_id[1] >> 4;
        hex_sample_id[4] = sample_id[2] & 15;
        hex_sample_id[5] = sample_id[2] >> 4;

        // As the nibbles will be used as hex colour digits, shift them up to make pastel colours
        hex_sample_id[0] = minU8(hex_sample_id[0] + shift, 15);
        hex_sample_id[1] = minU8(hex_sample_id[1] + shift, 15);
        hex_sample_id[2] = minU8(hex_sample_id[2] + shift, 15);
        hex_sample_id[3] = minU8(hex_sample_id[3] + shift, 15);
        hex_sample_id[4] = minU8(hex_sample_id[4] + shift, 15);
        hex_sample_id[5] = minU8(hex_sample_id[5] + shift, 15);

        // Convert the nibbles to hex for the final colour
        sample->unique_id_html_colour[1] = g_DecimalToHex[hex_sample_id[0]];
        sample->unique_id_html_colour[2] = g_DecimalToHex[hex_sample_id[1]];
        sample->unique_id_html_colour[3] = g_DecimalToHex[hex_sample_id[2]];
        sample->unique_id_html_colour[4] = g_DecimalToHex[hex_sample_id[3]];
        sample->unique_id_html_colour[5] = g_DecimalToHex[hex_sample_id[4]];
        sample->unique_id_html_colour[6] = g_DecimalToHex[hex_sample_id[5]];
    }

    // Concatenate children
    for (child = sample->first_child; child != NULL; child = child->next_sibling)
        GetSampleDigest(child, digest_hash, nb_samples);
}

static rmtError Remotery_SendLogTextMessage(Remotery* rmt, Message* message)
{
    rmtError error = RMT_ERROR_NONE;
    Buffer* bin_buf;

    // Build the buffer as if it's being sent to the server
    assert(rmt != NULL);
    assert(message != NULL);
    bin_buf = rmt->server->bin_buf;
    WebSocket_PrepareBuffer(bin_buf);
    Buffer_Write(bin_buf, message->payload, message->payload_size);

    // Pass to either the server or the log file
    if (Server_IsClientConnected(rmt->server) == RMT_TRUE)
    {
        error = Server_Send(rmt->server, bin_buf->data, bin_buf->bytes_used, 20);
    }
    if (rmt->logfile != NULL)
    {
        rmtWriteFile(rmt->logfile, bin_buf->data + WEBSOCKET_MAX_FRAME_HEADER_SIZE, bin_buf->bytes_used - WEBSOCKET_MAX_FRAME_HEADER_SIZE);
    }

    return error;
}

static rmtError bin_SampleName(Buffer* buffer, const char* name, rmtU32 name_hash, rmtU32 name_length)
{
    rmtError error;

    BIN_ERROR_CHECK(Buffer_Write(buffer, "SSMP", 4));
    BIN_ERROR_CHECK(Buffer_WriteU32(buffer, name_hash));
    BIN_ERROR_CHECK(Buffer_WriteU32(buffer, name_length));
    BIN_ERROR_CHECK(Buffer_Write(buffer, (void*)name, name_length));

    return RMT_ERROR_NONE;
}

static rmtError Remotery_AddToStringTable(Remotery* rmt, Message* message)
{
    // Add to the string table
    Msg_AddToStringTable* payload = (Msg_AddToStringTable*)message->payload;
    const char* name = (const char*)(payload + 1);
    StringTable_Insert(rmt->string_table, payload->hash, name);

    // Emit to log file if one is open
    if (rmt->logfile != NULL)
    {
        rmtError error;

        Buffer* bin_buf = rmt->server->bin_buf;
        bin_buf->bytes_used = 0;
        BIN_ERROR_CHECK(bin_SampleName(bin_buf, name, payload->hash, payload->length));

        rmtWriteFile(rmt->logfile, bin_buf->data, bin_buf->bytes_used);
    }

    return RMT_ERROR_NONE;
}

static rmtError bin_SampleTree(Buffer* buffer, Msg_SampleTree* msg)
{
    Sample* root_sample;
    char thread_name[256];
    rmtU32 digest_hash = 0;
    rmtU32 nb_samples = 0;
    rmtError error;

    assert(buffer != NULL);
    assert(msg != NULL);

    // Get the message root sample
    root_sample = msg->rootSample;
    assert(root_sample != NULL);

    // Add any sample types as a thread name post-fix to ensure they get their own viewer
    thread_name[0] = 0;
    strncat_s(thread_name, sizeof(thread_name), msg->threadName, strnlen_s(msg->threadName, 255));
    if (root_sample->type == RMT_SampleType_CUDA)
    {
        strncat_s(thread_name, sizeof(thread_name), " (CUDA)", 7);
    }
    if (root_sample->type == RMT_SampleType_D3D11)
    {
        strncat_s(thread_name, sizeof(thread_name), " (D3D11)", 8);
    }
    if (root_sample->type == RMT_SampleType_OpenGL)
    {
        strncat_s(thread_name, sizeof(thread_name), " (OpenGL)", 9);
    }
    if (root_sample->type == RMT_SampleType_Metal)
    {
        strncat_s(thread_name, sizeof(thread_name), " (Metal)", 8);
    }

    // Get digest hash of samples so that viewer can efficiently rebuild its tables
    GetSampleDigest(root_sample, &digest_hash, &nb_samples);

    // Write global message header
    BIN_ERROR_CHECK(Buffer_Write(buffer, (void*)"SMPL    ", 8));

    // Write sample message header
    BIN_ERROR_CHECK(Buffer_WriteStringWithLength(buffer, thread_name));
    BIN_ERROR_CHECK(Buffer_WriteU32(buffer, nb_samples));
    BIN_ERROR_CHECK(Buffer_WriteU32(buffer, digest_hash));
    BIN_ERROR_CHECK(Buffer_WriteU32(buffer, msg->partialTree ? 1 : 0));

    // Write entire sample tree
    BIN_ERROR_CHECK(bin_Sample(buffer, root_sample));

    // Patch message size
    U32ToByteArray(buffer->data + 4, buffer->bytes_used);

    return RMT_ERROR_NONE;
}

#if RMT_USE_CUDA
static rmtBool AreCUDASamplesReady(Sample* sample);
static rmtBool GetCUDASampleTimes(Sample* root_sample, Sample* sample);
#endif

static rmtError Remotery_SendSampleTreeMessage(Remotery* rmt, Message* message)
{
    rmtError error = RMT_ERROR_NONE;

    Msg_SampleTree* sample_tree;
    Sample* sample;
    Buffer* bin_buf;

    assert(rmt != NULL);
    assert(message != NULL);

    // Get the message root sample
    sample_tree = (Msg_SampleTree*)message->payload;
    sample = sample_tree->rootSample;
    assert(sample != NULL);

#if RMT_USE_CUDA
    if (sample->type == RMT_SampleType_CUDA)
    {
        // If these CUDA samples aren't ready yet, stick them to the back of the queue and continue
        rmtBool are_samples_ready;
        rmt_BeginCPUSample(AreCUDASamplesReady, 0);
        are_samples_ready = AreCUDASamplesReady(sample);
        rmt_EndCPUSample();
        if (!are_samples_ready)
        {
            QueueSampleTree(rmt->mq_to_rmt_thread, sample, sample_tree->allocator, sample_tree->threadName,
                                message->threadProfiler, RMT_FALSE);
            return RMT_ERROR_NONE;
        }

        // Retrieve timing of all CUDA samples
        rmt_BeginCPUSample(GetCUDASampleTimes, 0);
        GetCUDASampleTimes(sample->parent, sample);
        rmt_EndCPUSample();
    }
#endif

    // Reset the buffer for sending a websocket message
    bin_buf = rmt->server->bin_buf;
    WebSocket_PrepareBuffer(bin_buf);

    // Serialise the sample tree
    rmt_BeginCPUSample(bin_SampleTree, RMTSF_Aggregate);
    error = bin_SampleTree(bin_buf, sample_tree);
    rmt_EndCPUSample();

    if (g_Settings.sampletree_handler != NULL)
    {
        g_Settings.sampletree_handler(g_Settings.sampletree_context, sample_tree);
    }

    // Release sample tree samples back to their allocator
    FreeSamples(sample, sample_tree->allocator);

    if (error != RMT_ERROR_NONE)
    {
        return error;
    }

    if (Server_IsClientConnected(rmt->server) == RMT_TRUE)
    {
        // Send to the viewer with a reasonably long timeout as the size of the sample data may be large
        rmt_BeginCPUSample(Server_Send, RMTSF_Aggregate);
        error = Server_Send(rmt->server, bin_buf->data, bin_buf->bytes_used, 50000);
        rmt_EndCPUSample();
    }

    if (rmt->logfile != NULL)
    {
        // Write the data after the websocket header
        rmtWriteFile(rmt->logfile, bin_buf->data + WEBSOCKET_MAX_FRAME_HEADER_SIZE, bin_buf->bytes_used - WEBSOCKET_MAX_FRAME_HEADER_SIZE);
    }

    return error;
}

static rmtError Remotery_SendProcessorThreads(Remotery* rmt, Message* message)
{
    rmtU32 processor_index;
    rmtError error = RMT_ERROR_NONE;

    Msg_ProcessorThreads* processor_threads = (Msg_ProcessorThreads*)message->payload;

    Buffer* bin_buf;

    // Reset the buffer for sending a websocket message
    bin_buf = rmt->server->bin_buf;
    WebSocket_PrepareBuffer(bin_buf);

    // Serialise the message
    BIN_ERROR_CHECK(Buffer_Write(bin_buf, (void*)"PRTH", 4));
    BIN_ERROR_CHECK(Buffer_WriteU32(bin_buf, processor_threads->nbProcessors));
    BIN_ERROR_CHECK(Buffer_WriteU64(bin_buf, processor_threads->messageIndex));
    for (processor_index = 0; processor_index < processor_threads->nbProcessors; processor_index++)
    {
        Processor* processor = processor_threads->processors + processor_index;
        if (processor->threadProfiler != NULL)
        {
            BIN_ERROR_CHECK(Buffer_WriteU32(bin_buf, processor->threadProfiler->threadId));
            BIN_ERROR_CHECK(Buffer_WriteU32(bin_buf, processor->threadProfiler->threadNameHash));
            BIN_ERROR_CHECK(Buffer_WriteU64(bin_buf, processor->sampleTime));
        }
        else
        {
            BIN_ERROR_CHECK(Buffer_WriteU32(bin_buf, (rmtU32)-1));
            BIN_ERROR_CHECK(Buffer_WriteU32(bin_buf, 0));
            BIN_ERROR_CHECK(Buffer_WriteU64(bin_buf, 0));
        }
    }

    if (Server_IsClientConnected(rmt->server) == RMT_TRUE)
    {
        // Send to the viewer
        error = Server_Send(rmt->server, bin_buf->data, bin_buf->bytes_used, 50);
    }

    if (rmt->logfile != NULL)
    {
        // Write the data after the websocket header
        rmtWriteFile(rmt->logfile, bin_buf->data + WEBSOCKET_MAX_FRAME_HEADER_SIZE, bin_buf->bytes_used - WEBSOCKET_MAX_FRAME_HEADER_SIZE);
    }

    return error;
}

static rmtError Remotery_SendThreadName(Remotery* rmt, Message* message)
{
    rmtU32 name_length;
    rmtError error;

    Buffer* bin_buf;

    // Reset the buffer for sending a websocket message
    bin_buf = rmt->server->bin_buf;
    WebSocket_PrepareBuffer(bin_buf);

    // Serialise the message
    BIN_ERROR_CHECK(Buffer_Write(bin_buf, (void*)"THRN", 4));
    BIN_ERROR_CHECK(Buffer_Write(bin_buf, message->payload, 8));
    name_length = *(rmtU32*)(message->payload + 4);
    BIN_ERROR_CHECK(Buffer_Write(bin_buf, message->payload + 8, name_length));

    if (Server_IsClientConnected(rmt->server) == RMT_TRUE)
    {
        // Send to the viewer
        error = Server_Send(rmt->server, bin_buf->data, bin_buf->bytes_used, 50);
    }

    if (rmt->logfile != NULL)
    {
        // Write the data after the websocket header
        rmtWriteFile(rmt->logfile, bin_buf->data + WEBSOCKET_MAX_FRAME_HEADER_SIZE, bin_buf->bytes_used - WEBSOCKET_MAX_FRAME_HEADER_SIZE);
    }

    return RMT_ERROR_NONE;
}

static rmtError Remotery_ConsumeMessageQueue(Remotery* rmt)
{
    rmtU32 nb_messages_sent = 0;
    const rmtU32 maxNbMessagesPerUpdate = g_Settings.maxNbMessagesPerUpdate;

    assert(rmt != NULL);

    // Loop reading the max number of messages for this update
    // Note some messages don't consume the sent message count as they are small enough to not cause performance issues
    while (nb_messages_sent < maxNbMessagesPerUpdate)
    {
        rmtError error = RMT_ERROR_NONE;
        Message* message = rmtMessageQueue_PeekNextMessage(rmt->mq_to_rmt_thread);
        if (message == NULL)
            break;

        switch (message->id)
        {
            // This shouldn't be possible
            case MsgID_NotReady:
                assert(RMT_FALSE);
                break;

            // Dispatch to message handler
            case MsgID_AddToStringTable:
                error = Remotery_AddToStringTable(rmt, message);
                break;
            case MsgID_LogText:
                error = Remotery_SendLogTextMessage(rmt, message);
                nb_messages_sent++;
                break;
            case MsgID_SampleTree:
                rmt_BeginCPUSample(SendSampleTreeMessage, RMTSF_Aggregate);
                error = Remotery_SendSampleTreeMessage(rmt, message);
                nb_messages_sent++;
                rmt_EndCPUSample();
                break;
            case MsgID_ProcessorThreads:
                Remotery_SendProcessorThreads(rmt, message);
                nb_messages_sent++;
                break;
            case MsgID_ThreadName:
                Remotery_SendThreadName(rmt, message);
                break;

            default:
                break;
        }

        // Consume the message before reacting to any errors
        rmtMessageQueue_ConsumeNextMessage(rmt->mq_to_rmt_thread, message);
        if (error != RMT_ERROR_NONE)
            return error;
    }

    return RMT_ERROR_NONE;
}

static void Remotery_FlushMessageQueue(Remotery* rmt)
{
    assert(rmt != NULL);

    // Loop reading all remaining messages
    for (;;)
    {
        Message* message = rmtMessageQueue_PeekNextMessage(rmt->mq_to_rmt_thread);
        if (message == NULL)
            break;

        switch (message->id)
        {
            // These can be safely ignored
            case MsgID_NotReady:
            case MsgID_AddToStringTable:
            case MsgID_LogText:
                break;

            // Release all samples back to their allocators
            case MsgID_SampleTree: {
                Msg_SampleTree* sample_tree = (Msg_SampleTree*)message->payload;
                FreeSamples(sample_tree->rootSample, sample_tree->allocator);
                break;
            }

            default:
                break;
        }

        rmtMessageQueue_ConsumeNextMessage(rmt->mq_to_rmt_thread, message);
    }
}

static void Remotery_MapMessageQueue(Remotery* rmt)
{
    rmtU32 read_pos, write_pos;
    rmtMessageQueue* queue;

    assert(rmt != NULL);

    // Wait until the caller sets the custom data
    while (LoadAcquirePointer((long* volatile*)&rmt->map_message_queue_data) == NULL)
        msSleep(1);

    // Snapshot the current write position so that we're not constantly chasing other threads
    // that can have no effect on the thread requesting the map.
    queue = rmt->mq_to_rmt_thread;
    write_pos = LoadAcquire(&queue->write_pos);

    // Walk every message in the queue and call the map function
    read_pos = queue->read_pos;
    while (read_pos < write_pos)
    {
        rmtU32 r = read_pos & (queue->size - 1);
        Message* message = (Message*)(queue->data->ptr + r);
        rmtU32 message_size = rmtMessageQueue_SizeForPayload(message->payload_size);
        rmt->map_message_queue_fn(rmt, message);
        read_pos += message_size;
    }

    StoreReleasePointer((long* volatile*)&rmt->map_message_queue_data, NULL);
}

static rmtError Remotery_ThreadMain(rmtThread* thread)
{
    Remotery* rmt = (Remotery*)thread->param;
    assert(rmt != NULL);

    rmt_SetCurrentThreadName("Remotery");

    while (thread->request_exit == RMT_FALSE)
    {
        rmt_BeginCPUSample(Wakeup, 0);

        rmt_BeginCPUSample(ServerUpdate, 0);
        Server_Update(rmt->server);
        rmt_EndCPUSample();

        rmt_BeginCPUSample(ConsumeMessageQueue, 0);
        Remotery_ConsumeMessageQueue(rmt);
        rmt_EndCPUSample();

        rmt_EndCPUSample();

        // Process any queue map requests
        if (LoadAcquirePointer((long* volatile*)&rmt->map_message_queue_fn) != NULL)
        {
            Remotery_MapMessageQueue(rmt);
            StoreReleasePointer((long* volatile*)&rmt->map_message_queue_fn, NULL);
        }

        //
        // [NOTE-A]
        //
        // Possible sequence of user events at this point:
        //
        //    1. Add samples to the queue.
        //    2. Shutdown remotery.
        //
        // This loop will exit with unrelease samples.
        //

        msSleep(g_Settings.msSleepBetweenServerUpdates);
    }

    // Release all samples to their allocators as a consequence of [NOTE-A]
    Remotery_FlushMessageQueue(rmt);

    return RMT_ERROR_NONE;
}

static rmtError Remotery_ReceiveMessage(void* context, char* message_data, rmtU32 message_length)
{
    Remotery* rmt = (Remotery*)context;

// Manual dispatch on 4-byte message headers (message ID is little-endian encoded)
#define FOURCC(a, b, c, d) (rmtU32)(((d) << 24) | ((c) << 16) | ((b) << 8) | (a))
    rmtU32 message_id = *(rmtU32*)message_data;

    switch (message_id)
    {
        case FOURCC('C', 'O', 'N', 'I'): {
            rmt_LogText("Console message received...");
            rmt_LogText(message_data + 4);

            // Pass on to any registered handler
            if (g_Settings.input_handler != NULL)
                g_Settings.input_handler(message_data + 4, g_Settings.input_handler_context);

            break;
        }

        case FOURCC('G', 'S', 'M', 'P'): {
            rmtPStr name;

            // Convert name hash to integer
            rmtU32 name_hash = 0;
            const char* cur = message_data + 4;
            const char* end = cur + message_length - 4;
            while (cur < end)
                name_hash = name_hash * 10 + *cur++ - '0';

            // Search for a matching string hash
            name = StringTable_Find(rmt->string_table, name_hash);
            if (name != NULL)
            {
                rmtU32 name_length = (rmtU32)strnlen_s_safe_c(name, 256 - 12);

                // Construct a response message containing the matching name
                Buffer* bin_buf = rmt->server->bin_buf;
                WebSocket_PrepareBuffer(bin_buf);
                bin_SampleName(bin_buf, name, name_hash, name_length);

                // Send back immediately as we're on the server thread
                return Server_Send(rmt->server, bin_buf->data, bin_buf->bytes_used, 10);
            }

            break;
        }
    }

#undef FOURCC

    return RMT_ERROR_NONE;
}

static rmtError Remotery_Constructor(Remotery* rmt)
{
    rmtError error;

    assert(rmt != NULL);

    // Set default state
    rmt->server = NULL;
    rmt->mq_to_rmt_thread = NULL;
    rmt->thread = NULL;
    rmt->string_table = NULL;
    rmt->logfile = NULL;
    rmt->map_message_queue_fn = NULL;
    rmt->map_message_queue_data = NULL;
    rmt->threadProfilers = NULL;

#if RMT_USE_CUDA
    rmt->cuda.CtxSetCurrent = NULL;
    rmt->cuda.EventCreate = NULL;
    rmt->cuda.EventDestroy = NULL;
    rmt->cuda.EventElapsedTime = NULL;
    rmt->cuda.EventQuery = NULL;
    rmt->cuda.EventRecord = NULL;
#endif

#if RMT_USE_OPENGL
    rmt->opengl = NULL;
#endif

#if RMT_USE_METAL
    rmt->metal = NULL;
#endif

    // Kick-off the timer
    usTimer_Init(&rmt->timer);

    // Create the server
    New_3(Server, rmt->server, g_Settings.port, g_Settings.reuse_open_port, g_Settings.limit_connections_to_localhost);
    if (error != RMT_ERROR_NONE)
        return error;

    // Setup incoming message handler
    rmt->server->receive_handler = Remotery_ReceiveMessage;
    rmt->server->receive_handler_context = rmt;

    // Create the main message thread with only one page
    New_1(rmtMessageQueue, rmt->mq_to_rmt_thread, g_Settings.messageQueueSizeInBytes);
    if (error != RMT_ERROR_NONE)
        return error;

    // Create sample name string table
    New_0(StringTable, rmt->string_table);
    if (error != RMT_ERROR_NONE)
        return error;

    if (g_Settings.logPath != NULL)
    {
        // Get current date/time
        struct tm* now_tm = TimeDateNow();

        // Start the log path off
        char filename[512] = { 0 };
        strncat_s(filename, sizeof(filename), g_Settings.logPath, 512);
        strncat_s(filename, sizeof(filename), "/remotery-log-", 14);

        // Append current date and time
        strncat_s(filename, sizeof(filename), itoa_s(now_tm->tm_year + 1900), 11);
        strncat_s(filename, sizeof(filename), "-", 1);
        strncat_s(filename, sizeof(filename), itoa_s(now_tm->tm_mon + 1), 11);
        strncat_s(filename, sizeof(filename), "-", 1);
        strncat_s(filename, sizeof(filename), itoa_s(now_tm->tm_mday), 11);
        strncat_s(filename, sizeof(filename), "-", 1);
        strncat_s(filename, sizeof(filename), itoa_s(now_tm->tm_hour), 11);
        strncat_s(filename, sizeof(filename), "-", 1);
        strncat_s(filename, sizeof(filename), itoa_s(now_tm->tm_min), 11);
        strncat_s(filename, sizeof(filename), "-", 1);
        strncat_s(filename, sizeof(filename), itoa_s(now_tm->tm_sec), 11);

        // Just append a custom extension
        strncat_s(filename, sizeof(filename), ".rbin", 5);

        // Open and assume any failure simply sets NULL and the file isn't written
        rmt->logfile = rmtOpenFile(filename, "w");

        // Write the header
        if (rmt->logfile != NULL)
        {
            rmtWriteFile(rmt->logfile, "RMTBLOGF", 8);
        }
    }

#if RMT_USE_OPENGL
    error = OpenGL_Create(&rmt->opengl);
    if (error != RMT_ERROR_NONE)
        return error;
#endif

#if RMT_USE_METAL
    error = Metal_Create(&rmt->metal);
    if (error != RMT_ERROR_NONE)
        return error;
#endif

    // Create the thread profilers container
    New_2(ThreadProfilers, rmt->threadProfilers, &rmt->timer, rmt->mq_to_rmt_thread);
    if (error != RMT_ERROR_NONE)
    {
        return error;
    }

    // Set as the global instance before creating any threads that uses it for sampling itself
    assert(g_Remotery == NULL);
    g_Remotery = rmt;
    g_RemoteryCreated = RMT_TRUE;

    // Ensure global instance writes complete before other threads get a chance to use it
    CompilerWriteFence();

    // Create the main update thread once everything has been defined for the global remotery object
    New_2(rmtThread, rmt->thread, Remotery_ThreadMain, rmt);
    return error;
}

static void Remotery_Destructor(Remotery* rmt)
{
    assert(rmt != NULL);

    // Join the remotery thread before clearing the global object as the thread is profiling itself
    Delete(rmtThread, rmt->thread);

    if (g_RemoteryCreated)
    {
        g_Remotery = NULL;
        g_RemoteryCreated = RMT_FALSE;
    }

    Delete(ThreadProfilers, rmt->threadProfilers);

#if RMT_USE_OPENGL
    Delete(OpenGL, rmt->opengl);
#endif

#if RMT_USE_METAL
    Delete(Metal, rmt->metal);
#endif

    rmtCloseFile(rmt->logfile);

    Delete(StringTable, rmt->string_table);
    Delete(rmtMessageQueue, rmt->mq_to_rmt_thread);

    Delete(Server, rmt->server);
}

static void* CRTMalloc(void* mm_context, rmtU32 size)
{
    RMT_UNREFERENCED_PARAMETER(mm_context);
    return malloc((size_t)size);
}

static void CRTFree(void* mm_context, void* ptr)
{
    RMT_UNREFERENCED_PARAMETER(mm_context);
    free(ptr);
}

static void* CRTRealloc(void* mm_context, void* ptr, rmtU32 size)
{
    RMT_UNREFERENCED_PARAMETER(mm_context);
    return realloc(ptr, size);
}

RMT_API rmtSettings* _rmt_Settings(void)
{
    // Default-initialize on first call
    if (g_SettingsInitialized == RMT_FALSE)
    {
        g_Settings.port = 0x4597;
        g_Settings.reuse_open_port = RMT_FALSE;
        g_Settings.limit_connections_to_localhost = RMT_FALSE;
        g_Settings.enableThreadSampler = RMT_TRUE;
        g_Settings.msSleepBetweenServerUpdates = 4;
        g_Settings.messageQueueSizeInBytes = 1024 * 1024;
        g_Settings.maxNbMessagesPerUpdate = 50;
        g_Settings.malloc = CRTMalloc;
        g_Settings.free = CRTFree;
        g_Settings.realloc = CRTRealloc;
        g_Settings.input_handler = NULL;
        g_Settings.input_handler_context = NULL;
        g_Settings.logPath = NULL;
        g_Settings.sampletree_handler = NULL;
        g_Settings.sampletree_context = NULL;

        g_SettingsInitialized = RMT_TRUE;
    }

    return &g_Settings;
}

RMT_API rmtError _rmt_CreateGlobalInstance(Remotery** remotery)
{
    rmtError error;

    // Ensure load/acquire store/release operations match this enum size
    assert(sizeof(MessageID) == sizeof(rmtU32));

    // Default-initialise if user has not set values
    rmt_Settings();

    // Creating the Remotery instance also records it as the global instance
    assert(remotery != NULL);
    New_0(Remotery, *remotery);
    return error;
}

RMT_API void _rmt_DestroyGlobalInstance(Remotery* remotery)
{
    // Ensure this is the module that created it
    assert(g_RemoteryCreated == RMT_TRUE);
    assert(g_Remotery == remotery);
    Delete(Remotery, remotery);
}

RMT_API void _rmt_SetGlobalInstance(Remotery* remotery)
{
    // Default-initialise if user has not set values
    rmt_Settings();

    g_Remotery = remotery;
}

RMT_API Remotery* _rmt_GetGlobalInstance(void)
{
    return g_Remotery;
}

#ifdef RMT_PLATFORM_WINDOWS
#pragma pack(push, 8)
typedef struct tagTHREADNAME_INFO
{
    DWORD dwType;     // Must be 0x1000.
    LPCSTR szName;    // Pointer to name (in user addr space).
    DWORD dwThreadID; // Thread ID (-1=caller thread).
    DWORD dwFlags;    // Reserved for future use, must be zero.
} THREADNAME_INFO;
#pragma pack(pop)
#endif

wchar_t* MakeWideString(const char* string)
{
    size_t wlen;
    wchar_t* wstr;

    // First get the converted length
#if defined(RMT_PLATFORM_WINDOWS) && !RMT_USE_TINYCRT
    if (mbstowcs_s(&wlen, NULL, 0, string, INT_MAX) != 0)
    {
        return NULL;
    }
#else
    wlen = mbstowcs(NULL, string, 256);
#endif

    // Allocate enough words for the converted result
    wstr = (wchar_t*)(rmtMalloc((wlen + 1) * sizeof(wchar_t)));
    if (wstr == NULL)
    {
        return NULL;
    }

    // Convert
#if defined(RMT_PLATFORM_WINDOWS) && !RMT_USE_TINYCRT
    if (mbstowcs_s(&wlen, wstr, wlen + 1, string, wlen) != 0)
#else
    if (mbstowcs(wstr, string, wlen + 1) != wlen)
#endif
    {
        rmtFree(wstr);
        return NULL;
    }

    return  wstr;
}

static void SetDebuggerThreadName(const char* name)
{
#ifdef RMT_PLATFORM_WINDOWS
    THREADNAME_INFO info;

    // See if SetThreadDescription is available in this version of Windows
    // Introduced in Windows 10 build 1607
    HMODULE kernel32 = GetModuleHandleA("Kernel32.dll");
    if (kernel32 != NULL)
    {
        typedef HRESULT(WINAPI* SETTHREADDESCRIPTION)(HANDLE hThread, PCWSTR lpThreadDescription);
        SETTHREADDESCRIPTION SetThreadDescription = (SETTHREADDESCRIPTION)GetProcAddress(kernel32, "SetThreadDescription");
        if (SetThreadDescription != NULL)
        {
            // Create a wide-string version of the thread name
            wchar_t* wstr = MakeWideString(name);
            if (wstr != NULL)
            {
                // Set and return, leaving a fall-through for any failure cases to use the old exception method
                SetThreadDescription(GetCurrentThread(), wstr);
                rmtFree(wstr);
                return;
            }
        }
    }

    info.dwType = 0x1000;
    info.szName = name;
    info.dwThreadID = (DWORD)-1;
    info.dwFlags = 0;

#ifndef __MINGW32__
    __try
    {
        RaiseException(0x406D1388, 0, sizeof(info) / sizeof(ULONG_PTR), (ULONG_PTR*)&info);
    }
    __except (1 /* EXCEPTION_EXECUTE_HANDLER */)
    {
    }
#endif
#else
    RMT_UNREFERENCED_PARAMETER(name);
#endif

#ifdef RMT_PLATFORM_LINUX
    // pthread_setname_np is a non-standard GNU extension.
    char name_clamp[16];
    name_clamp[0] = 0;
    strncat_s(name_clamp, sizeof(name_clamp), name, 15);
#if defined(__FreeBSD__) || defined(__OpenBSD__)
    pthread_set_name_np(pthread_self(), name_clamp);
#else
    prctl(PR_SET_NAME, name_clamp, 0, 0, 0);
#endif
#endif
}

RMT_API void _rmt_SetCurrentThreadName(rmtPStr thread_name)
{
    ThreadProfiler* thread_profiler;

    if (g_Remotery == NULL)
    {
        return;
    }

    // Get data for this thread
    if (ThreadProfilers_GetCurrentThreadProfiler(g_Remotery->threadProfilers, &thread_profiler) != RMT_ERROR_NONE)
    {
        return;
    }

    // Copy name and apply to the debugger
    strcpy_s(thread_profiler->threadName, sizeof(thread_profiler->threadName), thread_name);
    thread_profiler->threadNameHash = MurmurHash3_x86_32(thread_name, strnlen_s(thread_name, 64), 0);
    SetDebuggerThreadName(thread_name);

    // Send the thread name for lookup
#ifdef RMT_PLATFORM_WINDOWS
    QueueThreadName(g_Remotery->mq_to_rmt_thread, thread_name, thread_profiler);
#endif
}

static rmtBool QueueLine(rmtMessageQueue* queue, unsigned char* text, rmtU32 size, struct ThreadProfiler* thread_profiler)
{
    Message* message;
    rmtU32 text_size;

    assert(queue != NULL);

    // Prefix with text size
    text_size = size - 8;
    U32ToByteArray(text + 4, text_size);

    // Allocate some space for the line
    message = rmtMessageQueue_AllocMessage(queue, size, thread_profiler);
    if (message == NULL)
        return RMT_FALSE;

    // Copy the text and commit the message
    memcpy(message->payload, text, size);
    rmtMessageQueue_CommitMessage(message, MsgID_LogText);

    return RMT_TRUE;
}

RMT_API void _rmt_LogText(rmtPStr text)
{
    int start_offset, offset, i;
    unsigned char line_buffer[1024] = {0};
    ThreadProfiler* thread_profiler;

    if (g_Remotery == NULL)
        return;

    if (ThreadProfilers_GetCurrentThreadProfiler(g_Remotery->threadProfilers, &thread_profiler) != RMT_ERROR_NONE)
    {
        return;
    }

    // Start the line with the message header
    line_buffer[0] = 'L';
    line_buffer[1] = 'O';
    line_buffer[2] = 'G';
    line_buffer[3] = 'M';
    // Fill with spaces to enable viewing line_buffer without offset in a debugger
    // (will be overwritten later by QueueLine/rmtMessageQueue_AllocMessage)
    line_buffer[4] = ' ';
    line_buffer[5] = ' ';
    line_buffer[6] = ' ';
    line_buffer[7] = ' ';
    start_offset = 8;

    // There might be newlines in the buffer, so split them into multiple network calls
    offset = start_offset;
    for (i = 0; text[i] != 0; i++)
    {
        char c = text[i];

        // Line wrap when too long or newline encountered
        if (offset == sizeof(line_buffer) - 1 || c == '\n')
        {
            // Send the line up to now
            if (QueueLine(g_Remotery->mq_to_rmt_thread, line_buffer, offset, thread_profiler) == RMT_FALSE)
                return;

            // Restart line
            offset = start_offset;

            // Don't add the newline character (if this was the reason for the flush)
            // to the restarted line_buffer, let's skip it
            if (c == '\n')
                continue;
        }

        line_buffer[offset++] = c;
    }

    // Send the last line
    if (offset > start_offset)
    {
        assert(offset < (int)sizeof(line_buffer));
        QueueLine(g_Remotery->mq_to_rmt_thread, line_buffer, offset, thread_profiler);
    }
}

RMT_API void _rmt_BeginCPUSample(rmtPStr name, rmtU32 flags, rmtU32* hash_cache)
{
    // 'hash_cache' stores a pointer to a sample name's hash value. Internally this is used to identify unique
    // callstacks and it would be ideal that it's not recalculated each time the sample is used. This can be statically
    // cached at the point of call or stored elsewhere when dynamic names are required.
    //
    // If 'hash_cache' is NULL then this call becomes more expensive, as it has to recalculate the hash of the name.

    ThreadProfiler* thread_profiler;

    if (g_Remotery == NULL)
        return;

    // TODO: Time how long the bits outside here cost and subtract them from the parent

    if (ThreadProfilers_GetCurrentThreadProfiler(g_Remotery->threadProfilers, &thread_profiler) == RMT_ERROR_NONE)
    {
        Sample* sample;
        rmtU32 name_hash = ThreadProfiler_GetNameHash(thread_profiler, g_Remotery->mq_to_rmt_thread, name, hash_cache);
        if (ThreadProfiler_Push(thread_profiler->sampleTrees[RMT_SampleType_CPU], name_hash, flags, &sample) == RMT_ERROR_NONE)
        {
            // If this is an aggregate sample, store the time in 'end' as we want to preserve 'start'
            if (sample->call_count > 1)
                sample->us_end = usTimer_Get(&g_Remotery->timer);
            else
                sample->us_start = usTimer_Get(&g_Remotery->timer);
        }
    }
}

RMT_API void _rmt_EndCPUSample(void)
{
    ThreadProfiler* thread_profiler;

    if (g_Remotery == NULL)
        return;

    if (ThreadProfilers_GetCurrentThreadProfiler(g_Remotery->threadProfilers, &thread_profiler) == RMT_ERROR_NONE)
    {
        Sample* sample = thread_profiler->sampleTrees[RMT_SampleType_CPU]->currentParent;

        if (sample->recurse_depth > 0)
        {
            sample->recurse_depth--;
        }
        else
        {
            rmtU64 us_end = usTimer_Get(&g_Remotery->timer);
            Sample_Close(sample, us_end);
            ThreadProfiler_Pop(thread_profiler, g_Remotery->mq_to_rmt_thread, sample);
        }
    }
}

#if RMT_USE_OPENGL || RMT_USE_D3D11
static void Remotery_DeleteSampleTree(Remotery* rmt, enum rmtSampleType sample_type)
{
    ThreadProfiler* thread_profiler;

    // Get the attached thread sampler and delete the sample tree
    assert(rmt != NULL);
    if (ThreadProfilers_GetCurrentThreadProfiler(rmt->threadProfilers, &thread_profiler) == RMT_ERROR_NONE)
    {
        SampleTree* sample_tree = thread_profiler->sampleTrees[sample_type];
        if (sample_tree != NULL)
        {
            Delete(SampleTree, sample_tree);
            thread_profiler->sampleTrees[sample_type] = NULL;
        }
    }
}

static rmtBool rmtMessageQueue_IsEmpty(rmtMessageQueue* queue)
{
    assert(queue != NULL);
    return queue->write_pos - queue->read_pos == 0;
}

typedef struct GatherQueuedSampleData
{
    SampleType sample_type;
    Buffer* flush_samples;
} GatherQueuedSampleData;

static void MapMessageQueueAndWait(Remotery* rmt, void (*map_message_queue_fn)(Remotery* rmt, Message*), void* data)
{
    // Basic spin lock on the map function itself
    while (AtomicCompareAndSwapPointer((long* volatile*)&rmt->map_message_queue_fn, NULL,
                                       (long*)map_message_queue_fn) == RMT_FALSE)
        msSleep(1);

    StoreReleasePointer((long* volatile*)&rmt->map_message_queue_data, (long*)data);

    // Wait until map completes
    while (LoadAcquirePointer((long* volatile*)&rmt->map_message_queue_fn) != NULL)
        msSleep(1);
}

static void GatherQueuedSamples(Remotery* rmt, Message* message)
{
    GatherQueuedSampleData* gather_data = (GatherQueuedSampleData*)rmt->map_message_queue_data;

    // Filter sample trees
    if (message->id == MsgID_SampleTree)
    {
        Msg_SampleTree* sample_tree = (Msg_SampleTree*)message->payload;
        Sample* sample = sample_tree->rootSample;
        if (sample->type == gather_data->sample_type)
        {
            // Make a copy of the entire sample tree as the remotery thread may overwrite it while
            // the calling thread tries to delete
            rmtU32 message_size = rmtMessageQueue_SizeForPayload(message->payload_size);
            Buffer_Write(gather_data->flush_samples, message, message_size);

            // Mark the message empty
            message->id = MsgID_None;
        }
    }
}

static void FreePendingSampleTrees(Remotery* rmt, SampleType sample_type, Buffer* flush_samples)
{
    rmtU8* data;
    rmtU8* data_end;

    // Gather all sample trees currently queued for the Remotery thread
    GatherQueuedSampleData gather_data;
    gather_data.sample_type = sample_type;
    gather_data.flush_samples = flush_samples;
    MapMessageQueueAndWait(rmt, GatherQueuedSamples, &gather_data);

    // Release all sample trees to their allocators
    data = flush_samples->data;
    data_end = data + flush_samples->bytes_used;
    while (data < data_end)
    {
        Message* message = (Message*)data;
        rmtU32 message_size = rmtMessageQueue_SizeForPayload(message->payload_size);
        Msg_SampleTree* sample_tree = (Msg_SampleTree*)message->payload;
        FreeSamples(sample_tree->rootSample, sample_tree->allocator);
        data += message_size;
    }
}

#endif

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @CUDA: CUDA event sampling
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

#if RMT_USE_CUDA

typedef struct CUDASample
{
    // IS-A inheritance relationship
    Sample base;

    // Pair of events that wrap the sample
    CUevent event_start;
    CUevent event_end;

} CUDASample;

static rmtError MapCUDAResult(CUresult result)
{
    switch (result)
    {
        case CUDA_SUCCESS:
            return RMT_ERROR_NONE;
        case CUDA_ERROR_DEINITIALIZED:
            return RMT_ERROR_CUDA_DEINITIALIZED;
        case CUDA_ERROR_NOT_INITIALIZED:
            return RMT_ERROR_CUDA_NOT_INITIALIZED;
        case CUDA_ERROR_INVALID_CONTEXT:
            return RMT_ERROR_CUDA_INVALID_CONTEXT;
        case CUDA_ERROR_INVALID_VALUE:
            return RMT_ERROR_CUDA_INVALID_VALUE;
        case CUDA_ERROR_INVALID_HANDLE:
            return RMT_ERROR_CUDA_INVALID_HANDLE;
        case CUDA_ERROR_OUT_OF_MEMORY:
            return RMT_ERROR_CUDA_OUT_OF_MEMORY;
        case CUDA_ERROR_NOT_READY:
            return RMT_ERROR_ERROR_NOT_READY;
        default:
            return RMT_ERROR_CUDA_UNKNOWN;
    }
}

#define CUDA_MAKE_FUNCTION(name, params)         \
    typedef CUresult(CUDAAPI* name##Ptr) params; \
    name##Ptr name = (name##Ptr)g_Remotery->cuda.name;

#define CUDA_GUARD(call)             \
    {                                \
        rmtError error = call;       \
        if (error != RMT_ERROR_NONE) \
            return error;            \
    }

// Wrappers around CUDA driver functions that manage the active context.
static rmtError CUDASetContext(void* context)
{
    CUDA_MAKE_FUNCTION(CtxSetCurrent, (CUcontext ctx));
    assert(CtxSetCurrent != NULL);
    return MapCUDAResult(CtxSetCurrent((CUcontext)context));
}
static rmtError CUDAGetContext(void** context)
{
    CUDA_MAKE_FUNCTION(CtxGetCurrent, (CUcontext * ctx));
    assert(CtxGetCurrent != NULL);
    return MapCUDAResult(CtxGetCurrent((CUcontext*)context));
}
static rmtError CUDAEnsureContext()
{
    void* current_context;
    CUDA_GUARD(CUDAGetContext(&current_context));

    assert(g_Remotery != NULL);
    if (current_context != g_Remotery->cuda.context)
        CUDA_GUARD(CUDASetContext(g_Remotery->cuda.context));

    return RMT_ERROR_NONE;
}

// Wrappers around CUDA driver functions that manage events
static rmtError CUDAEventCreate(CUevent* phEvent, unsigned int Flags)
{
    CUDA_MAKE_FUNCTION(EventCreate, (CUevent * phEvent, unsigned int Flags));
    CUDA_GUARD(CUDAEnsureContext());
    return MapCUDAResult(EventCreate(phEvent, Flags));
}
static rmtError CUDAEventDestroy(CUevent hEvent)
{
    CUDA_MAKE_FUNCTION(EventDestroy, (CUevent hEvent));
    CUDA_GUARD(CUDAEnsureContext());
    return MapCUDAResult(EventDestroy(hEvent));
}
static rmtError CUDAEventRecord(CUevent hEvent, void* hStream)
{
    CUDA_MAKE_FUNCTION(EventRecord, (CUevent hEvent, CUstream hStream));
    CUDA_GUARD(CUDAEnsureContext());
    return MapCUDAResult(EventRecord(hEvent, (CUstream)hStream));
}
static rmtError CUDAEventQuery(CUevent hEvent)
{
    CUDA_MAKE_FUNCTION(EventQuery, (CUevent hEvent));
    CUDA_GUARD(CUDAEnsureContext());
    return MapCUDAResult(EventQuery(hEvent));
}
static rmtError CUDAEventElapsedTime(float* pMilliseconds, CUevent hStart, CUevent hEnd)
{
    CUDA_MAKE_FUNCTION(EventElapsedTime, (float* pMilliseconds, CUevent hStart, CUevent hEnd));
    CUDA_GUARD(CUDAEnsureContext());
    return MapCUDAResult(EventElapsedTime(pMilliseconds, hStart, hEnd));
}

static rmtError CUDASample_Constructor(CUDASample* sample)
{
    rmtError error;

    assert(sample != NULL);

    // Chain to sample constructor
    Sample_Constructor((Sample*)sample);
    sample->base.type = RMT_SampleType_CUDA;
    sample->event_start = NULL;
    sample->event_end = NULL;

    // Create non-blocking events with timing
    assert(g_Remotery != NULL);
    error = CUDAEventCreate(&sample->event_start, CU_EVENT_DEFAULT);
    if (error == RMT_ERROR_NONE)
        error = CUDAEventCreate(&sample->event_end, CU_EVENT_DEFAULT);
    return error;
}

static void CUDASample_Destructor(CUDASample* sample)
{
    assert(sample != NULL);

    // Destroy events
    if (sample->event_start != NULL)
        CUDAEventDestroy(sample->event_start);
    if (sample->event_end != NULL)
        CUDAEventDestroy(sample->event_end);

    Sample_Destructor((Sample*)sample);
}

static rmtBool AreCUDASamplesReady(Sample* sample)
{
    rmtError error;
    Sample* child;

    CUDASample* cuda_sample = (CUDASample*)sample;
    assert(sample->type == RMT_SampleType_CUDA);

    // Check to see if both of the CUDA events have been processed
    error = CUDAEventQuery(cuda_sample->event_start);
    if (error != RMT_ERROR_NONE)
        return RMT_FALSE;
    error = CUDAEventQuery(cuda_sample->event_end);
    if (error != RMT_ERROR_NONE)
        return RMT_FALSE;

    // Check child sample events
    for (child = sample->first_child; child != NULL; child = child->next_sibling)
    {
        if (!AreCUDASamplesReady(child))
            return RMT_FALSE;
    }

    return RMT_TRUE;
}

static rmtBool GetCUDASampleTimes(Sample* root_sample, Sample* sample)
{
    Sample* child;

    CUDASample* cuda_root_sample = (CUDASample*)root_sample;
    CUDASample* cuda_sample = (CUDASample*)sample;

    float ms_start, ms_end;

    assert(root_sample != NULL);
    assert(sample != NULL);

    // Get millisecond timing of each sample event, relative to initial root sample
    if (CUDAEventElapsedTime(&ms_start, cuda_root_sample->event_start, cuda_sample->event_start) != RMT_ERROR_NONE)
        return RMT_FALSE;
    if (CUDAEventElapsedTime(&ms_end, cuda_root_sample->event_start, cuda_sample->event_end) != RMT_ERROR_NONE)
        return RMT_FALSE;

    // Convert to microseconds and add to the sample
    sample->us_start = (rmtU64)(ms_start * 1000);
    sample->us_end = (rmtU64)(ms_end * 1000);
    sample->us_length = sample->us_end - sample->us_start;

    // Get child sample times
    for (child = sample->first_child; child != NULL; child = child->next_sibling)
    {
        if (!GetCUDASampleTimes(root_sample, child))
            return RMT_FALSE;
    }

    return RMT_TRUE;
}

RMT_API void _rmt_BindCUDA(const rmtCUDABind* bind)
{
    assert(bind != NULL);
    if (g_Remotery != NULL)
        g_Remotery->cuda = *bind;
}

RMT_API void _rmt_BeginCUDASample(rmtPStr name, rmtU32* hash_cache, void* stream)
{
    ThreadProfiler* thread_profiler;

    if (g_Remotery == NULL)
        return;

    if (ThreadProfilers_GetCurrentThreadProfiler(g_Remotery->threadProfilers, &thread_profiler) == RMT_ERROR_NONE)
    {
        rmtError error;
        Sample* sample;
        rmtU32 name_hash = ThreadProfiler_GetNameHash(thread_profiler, g_Remotery->mq_to_rmt_thread, name, hash_cache);

        // Create the CUDA tree on-demand as the tree needs an up-front-created root.
        // This is not possible to create on initialisation as a CUDA binding is not yet available.
        SampleTree** cuda_tree = &thread_profiler->sampleTrees[RMT_SampleType_CUDA];
        if (*cuda_tree == NULL)
        {
            CUDASample* root_sample;

            New_3(SampleTree, *cuda_tree, sizeof(CUDASample), (ObjConstructor)CUDASample_Constructor,
                  (ObjDestructor)CUDASample_Destructor);
            if (error != RMT_ERROR_NONE)
                return;

            // Record an event once on the root sample, used to measure absolute sample
            // times since this point
            root_sample = (CUDASample*)(*cuda_tree)->root;
            error = CUDAEventRecord(root_sample->event_start, stream);
            if (error != RMT_ERROR_NONE)
                return;
        }

        // Push the same and record its event
        if (ThreadProfiler_Push(*cuda_tree, name_hash, 0, &sample) == RMT_ERROR_NONE)
        {
            CUDASample* cuda_sample = (CUDASample*)sample;
            CUDAEventRecord(cuda_sample->event_start, stream);
        }
    }
}

RMT_API void _rmt_EndCUDASample(void* stream)
{
    ThreadProfiler* thread_profiler;

    if (g_Remotery == NULL)
        return;

    if (ThreadProfilers_GetCurrentThreadProfiler(g_Remotery->threadProfilers, &thread_profiler) == RMT_ERROR_NONE)
    {
        CUDASample* sample = (CUDASample*)thread_profiler->sampleTrees[RMT_SampleType_CUDA]->currentParent;
        if (sample->base.recurse_depth > 0)
        {
            sample->base.recurse_depth--;
        }
        else
        {
            CUDAEventRecord(sample->event_end, stream);
            ThreadProfiler_Pop(thread_profiler, g_Remotery->mq_to_rmt_thread, (Sample*)sample);
        }
    }
}

#endif // RMT_USE_CUDA

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   @D3D11: Direct3D 11 event sampling
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

#if RMT_USE_D3D11

// As clReflect has no way of disabling C++ compile mode, this forces C interfaces everywhere...
#define CINTERFACE

// ...unfortunately these C++ helpers aren't wrapped by the same macro but they can be disabled individually
#define D3D11_NO_HELPERS

// Allow use of the D3D11 helper macros for accessing the C-style vtable
#define COBJMACROS

#ifdef _MSC_VER
// Disable for d3d11.h
// warning C4201: nonstandard extension used : nameless struct/union
#pragma warning(push)
#pragma warning(disable : 4201)
#endif

#include <d3d11.h>

#ifdef _MSC_VER
#pragma warning(pop)
#endif

typedef struct D3D11
{
    // Context set by user
    ID3D11Device* device;
    ID3D11DeviceContext* context;

    HRESULT last_error;

    // Queue to the D3D 11 main update thread
    // Given that BeginSample/EndSample need to be called from the same thread that does the update, there
    // is really no need for this to be a thread-safe queue. I'm using it for its convenience.
    rmtMessageQueue* mq_to_d3d11_main;

    // Mark the first time so that remaining timestamps are offset from this
    rmtU64 first_timestamp;
    // Last time in us (CPU time, via usTimer_Get) since we last resync'ed CPU & GPU
    rmtU64 last_resync;

    // Sample trees in transit in the message queue for release on shutdown
    Buffer* flush_samples;
} D3D11;

static rmtError D3D11_Create(D3D11** d3d11)
{
    rmtError error;

    assert(d3d11 != NULL);

    // Allocate space for the D3D11 data
    *d3d11 = (D3D11*)rmtMalloc(sizeof(D3D11));
    if (*d3d11 == NULL)
        return RMT_ERROR_MALLOC_FAIL;

    // Set defaults
    (*d3d11)->device = NULL;
    (*d3d11)->context = NULL;
    (*d3d11)->last_error = S_OK;
    (*d3d11)->mq_to_d3d11_main = NULL;
    (*d3d11)->first_timestamp = 0;
    (*d3d11)->last_resync = 0;
    (*d3d11)->flush_samples = NULL;

    New_1(rmtMessageQueue, (*d3d11)->mq_to_d3d11_main, g_Settings.messageQueueSizeInBytes);
    if (error != RMT_ERROR_NONE)
    {
        Delete(D3D11, *d3d11);
        return error;
    }

    New_1(Buffer, (*d3d11)->flush_samples, 8 * 1024);
    if (error != RMT_ERROR_NONE)
    {
        Delete(D3D11, *d3d11);
        return error;
    }

    return RMT_ERROR_NONE;
}

static void D3D11_Destructor(D3D11* d3d11)
{
    assert(d3d11 != NULL);
    Delete(Buffer, d3d11->flush_samples);
    Delete(rmtMessageQueue, d3d11->mq_to_d3d11_main);
}

static HRESULT rmtD3D11Finish(ID3D11Device* device, ID3D11DeviceContext* context, rmtU64* out_timestamp,
                              double* out_frequency)
{
    HRESULT result;
    ID3D11Query* full_stall_fence;
    ID3D11Query* query_disjoint;
    D3D11_QUERY_DESC query_desc;
    D3D11_QUERY_DESC disjoint_desc;
    UINT64 timestamp;
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint;

    query_desc.Query = D3D11_QUERY_TIMESTAMP;
    query_desc.MiscFlags = 0;
    result = ID3D11Device_CreateQuery(device, &query_desc, &full_stall_fence);
    if (result != S_OK)
        return result;

    disjoint_desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    disjoint_desc.MiscFlags = 0;
    result = ID3D11Device_CreateQuery(device, &disjoint_desc, &query_disjoint);
    if (result != S_OK)
    {
        ID3D11Query_Release(full_stall_fence);
        return result;
    }

    ID3D11DeviceContext_Begin(context, (ID3D11Asynchronous*)query_disjoint);
    ID3D11DeviceContext_End(context, (ID3D11Asynchronous*)full_stall_fence);
    ID3D11DeviceContext_End(context, (ID3D11Asynchronous*)query_disjoint);

    result = S_FALSE;

    while (result == S_FALSE)
    {
        result =
            ID3D11DeviceContext_GetData(context, (ID3D11Asynchronous*)query_disjoint, &disjoint, sizeof(disjoint), 0);
        if (result != S_OK && result != S_FALSE)
        {
            ID3D11Query_Release(full_stall_fence);
            ID3D11Query_Release(query_disjoint);
            return result;
        }
        if (result == S_OK)
        {
            result = ID3D11DeviceContext_GetData(context, (ID3D11Asynchronous*)full_stall_fence, &timestamp,
                                                 sizeof(timestamp), 0);
            if (result != S_OK && result != S_FALSE)
            {
                ID3D11Query_Release(full_stall_fence);
                ID3D11Query_Release(query_disjoint);
                return result;
            }
        }
        // Give HyperThreading threads a breath on this spinlock.
        YieldProcessor();
    }

    if (disjoint.Disjoint == FALSE)
    {
        double frequency = disjoint.Frequency / 1000000.0;
        *out_timestamp = timestamp;
        *out_frequency = frequency;
    }
    else
    {
        result = S_FALSE;
    }

    ID3D11Query_Release(full_stall_fence);
    ID3D11Query_Release(query_disjoint);
    return result;
}

static HRESULT SyncD3D11CpuGpuTimes(ID3D11Device* device, ID3D11DeviceContext* context, rmtU64* out_first_timestamp,
                                    rmtU64* out_last_resync)
{
    rmtU64 cpu_time_start = 0;
    rmtU64 cpu_time_stop = 0;
    rmtU64 average_half_RTT = 0; // RTT = Rountrip Time.
    UINT64 gpu_base = 0;
    double frequency = 1;
    int i;

    HRESULT result;
    result = rmtD3D11Finish(device, context, &gpu_base, &frequency);
    if (result != S_OK && result != S_FALSE)
        return result;

    for (i = 0; i < RMT_GPU_CPU_SYNC_NUM_ITERATIONS; ++i)
    {
        rmtU64 half_RTT;
        cpu_time_start = usTimer_Get(&g_Remotery->timer);
        result = rmtD3D11Finish(device, context, &gpu_base, &frequency);
        cpu_time_stop = usTimer_Get(&g_Remotery->timer);

        if (result != S_OK && result != S_FALSE)
            return result;

        // Ignore attempts where there was a disjoint, since there would
        // be a lot of noise in those readings for measuring the RTT
        if (result == S_OK)
        {
            // Average the time it takes a roundtrip from CPU to GPU
            // while doing nothing other than getting timestamps
            half_RTT = (cpu_time_stop - cpu_time_start) >> 1ULL;
            if (i == 0)
                average_half_RTT = half_RTT;
            else
                average_half_RTT = (average_half_RTT + half_RTT) >> 1ULL;
        }
    }

    // All GPU times are offset from gpu_base, and then taken to
    // the same relative origin CPU timestamps are based on.
    // CPU is in us, we must translate it to ns.
    *out_first_timestamp = gpu_base - (rmtU64)((cpu_time_start + average_half_RTT) * frequency);
    *out_last_resync = cpu_time_stop;

    return result;
}

typedef struct D3D11Timestamp
{
    // Inherit so that timestamps can be quickly allocated
    ObjectLink Link;

    // Pair of timestamp queries that wrap the sample
    ID3D11Query* query_start;
    ID3D11Query* query_end;

    // A disjoint to measure frequency/stability
    // TODO: Does *each* sample need one of these?
    ID3D11Query* query_disjoint;

    rmtU64 cpu_timestamp;
} D3D11Timestamp;

static rmtError D3D11Timestamp_Constructor(D3D11Timestamp* stamp)
{
    ThreadProfiler* thread_profiler;
    D3D11_QUERY_DESC timestamp_desc;
    D3D11_QUERY_DESC disjoint_desc;
    ID3D11Device* device;
    HRESULT* last_error;
    rmtError rmt_error;

    assert(stamp != NULL);

    ObjectLink_Constructor((ObjectLink*)stamp);

    // Set defaults
    stamp->query_start = NULL;
    stamp->query_end = NULL;
    stamp->query_disjoint = NULL;
    stamp->cpu_timestamp = 0;

    assert(g_Remotery != NULL);
    rmt_error = ThreadProfilers_GetCurrentThreadProfiler(g_Remotery->threadProfilers, &thread_profiler);
    if (rmt_error != RMT_ERROR_NONE)
    {
        return rmt_error;
    }
    assert(thread_profiler->d3d11 != NULL);
    device = thread_profiler->d3d11->device;
    last_error = &thread_profiler->d3d11->last_error;

    // Create start/end timestamp queries
    timestamp_desc.Query = D3D11_QUERY_TIMESTAMP;
    timestamp_desc.MiscFlags = 0;
    *last_error = ID3D11Device_CreateQuery(device, &timestamp_desc, &stamp->query_start);
    if (*last_error != S_OK)
        return RMT_ERROR_D3D11_FAILED_TO_CREATE_QUERY;
    *last_error = ID3D11Device_CreateQuery(device, &timestamp_desc, &stamp->query_end);
    if (*last_error != S_OK)
        return RMT_ERROR_D3D11_FAILED_TO_CREATE_QUERY;

    // Create disjoint query
    disjoint_desc.Query = D3D11_QUERY_TIMESTAMP_DISJOINT;
    disjoint_desc.MiscFlags = 0;
    *last_error = ID3D11Device_CreateQuery(device, &disjoint_desc, &stamp->query_disjoint);
    if (*last_error != S_OK)
        return RMT_ERROR_D3D11_FAILED_TO_CREATE_QUERY;

    return RMT_ERROR_NONE;
}

static void D3D11Timestamp_Destructor(D3D11Timestamp* stamp)
{
    assert(stamp != NULL);

    // Destroy queries
    if (stamp->query_disjoint != NULL)
        ID3D11Query_Release(stamp->query_disjoint);
    if (stamp->query_end != NULL)
        ID3D11Query_Release(stamp->query_end);
    if (stamp->query_start != NULL)
        ID3D11Query_Release(stamp->query_start);
}

static void D3D11Timestamp_Begin(D3D11Timestamp* stamp, ID3D11DeviceContext* context)
{
    assert(stamp != NULL);

    // Start of disjoint and first query
    stamp->cpu_timestamp = usTimer_Get(&g_Remotery->timer);
    ID3D11DeviceContext_Begin(context, (ID3D11Asynchronous*)stamp->query_disjoint);
    ID3D11DeviceContext_End(context, (ID3D11Asynchronous*)stamp->query_start);
}

static void D3D11Timestamp_End(D3D11Timestamp* stamp, ID3D11DeviceContext* context)
{
    assert(stamp != NULL);

    // End of disjoint and second query
    ID3D11DeviceContext_End(context, (ID3D11Asynchronous*)stamp->query_end);
    ID3D11DeviceContext_End(context, (ID3D11Asynchronous*)stamp->query_disjoint);
}

static HRESULT D3D11Timestamp_GetData(D3D11Timestamp* stamp, ID3D11Device* device, ID3D11DeviceContext* context,
                                      rmtU64* out_start, rmtU64* out_end, rmtU64* out_first_timestamp,
                                      rmtU64* out_last_resync)
{
    ID3D11Asynchronous* query_start;
    ID3D11Asynchronous* query_end;
    ID3D11Asynchronous* query_disjoint;
    HRESULT result;

    UINT64 start;
    UINT64 end;
    D3D11_QUERY_DATA_TIMESTAMP_DISJOINT disjoint;

    assert(stamp != NULL);
    query_start = (ID3D11Asynchronous*)stamp->query_start;
    query_end = (ID3D11Asynchronous*)stamp->query_end;
    query_disjoint = (ID3D11Asynchronous*)stamp->query_disjoint;

    // Check to see if all queries are ready
    // If any fail to arrive, wait until later
    result = ID3D11DeviceContext_GetData(context, query_start, &start, sizeof(start), D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if (result != S_OK)
        return result;
    result = ID3D11DeviceContext_GetData(context, query_end, &end, sizeof(end), D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if (result != S_OK)
        return result;
    result = ID3D11DeviceContext_GetData(context, query_disjoint, &disjoint, sizeof(disjoint),
                                         D3D11_ASYNC_GETDATA_DONOTFLUSH);
    if (result != S_OK)
        return result;

    if (disjoint.Disjoint == FALSE)
    {
        double frequency = disjoint.Frequency / 1000000.0;

        // Mark the first timestamp. We may resync if we detect the GPU timestamp is in the
        // past (i.e. happened before the CPU command) since it should be impossible.
        assert(out_first_timestamp != NULL);
        if (*out_first_timestamp == 0 || ((start - *out_first_timestamp) / frequency) < stamp->cpu_timestamp)
        {
            result = SyncD3D11CpuGpuTimes(device, context, out_first_timestamp, out_last_resync);
            if (result != S_OK)
                return result;
        }

        // Calculate start and end timestamps from the disjoint info
        *out_start = (rmtU64)((start - *out_first_timestamp) / frequency);
        *out_end = (rmtU64)((end - *out_first_timestamp) / frequency);
    }
    else
    {
#if RMT_D3D11_RESYNC_ON_DISJOINT
        result = SyncD3D11CpuGpuTimes(device, context, out_first_timestamp, out_last_resync);
        if (result != S_OK)
            return result;
#endif
    }

    return S_OK;
}

typedef struct D3D11Sample
{
    // IS-A inheritance relationship
    Sample base;

    D3D11Timestamp* timestamp;

} D3D11Sample;

static rmtError D3D11Sample_Constructor(D3D11Sample* sample)
{
    rmtError error;

    assert(sample != NULL);

    // Chain to sample constructor
    Sample_Constructor((Sample*)sample);
    sample->base.type = RMT_SampleType_D3D11;
    New_0(D3D11Timestamp, sample->timestamp);

    return RMT_ERROR_NONE;
}

static void D3D11Sample_Destructor(D3D11Sample* sample)
{
    Delete(D3D11Timestamp, sample->timestamp);
    Sample_Destructor((Sample*)sample);
}

RMT_API void _rmt_BindD3D11(void* device, void* context)
{
    if (g_Remotery != NULL)
    {
        ThreadProfiler* thread_profiler;
        if (ThreadProfilers_GetCurrentThreadProfiler(g_Remotery->threadProfilers, &thread_profiler) == RMT_ERROR_NONE)
        {
            assert(thread_profiler->d3d11 != NULL);

            assert(device != NULL);
            thread_profiler->d3d11->device = (ID3D11Device*)device;
            assert(context != NULL);
            thread_profiler->d3d11->context = (ID3D11DeviceContext*)context;
        }
    }
}

static void UpdateD3D11Frame(ThreadProfiler* thread_profiler);

RMT_API void _rmt_UnbindD3D11(void)
{
    if (g_Remotery != NULL)
    {
        ThreadProfiler* thread_profiler;
        if (ThreadProfilers_GetCurrentThreadProfiler(g_Remotery->threadProfilers, &thread_profiler) == RMT_ERROR_NONE)
        {
            D3D11* d3d11 = thread_profiler->d3d11;
            assert(d3d11 != NULL);

            // Stall waiting for the D3D queue to empty into the Remotery queue
            while (!rmtMessageQueue_IsEmpty(d3d11->mq_to_d3d11_main))
                UpdateD3D11Frame(thread_profiler);

            // There will be a whole bunch of D3D11 sample trees queued up the remotery queue that need releasing
            FreePendingSampleTrees(g_Remotery, RMT_SampleType_D3D11, d3d11->flush_samples);

            // Inform sampler to not add any more samples
            d3d11->device = NULL;
            d3d11->context = NULL;

            // Forcefully delete sample tree on this thread to release time stamps from
            // the same thread that created them
            Remotery_DeleteSampleTree(g_Remotery, RMT_SampleType_D3D11);
        }
    }
}

RMT_API void _rmt_BeginD3D11Sample(rmtPStr name, rmtU32* hash_cache)
{
    ThreadProfiler* thread_profiler;
    D3D11* d3d11;

    if (g_Remotery == NULL)
        return;

    if (ThreadProfilers_GetCurrentThreadProfiler(g_Remotery->threadProfilers, &thread_profiler) == RMT_ERROR_NONE)
    {
        Sample* sample;
        rmtU32 name_hash;
        SampleTree** d3d_tree;

        // Has D3D11 been unbound?
        d3d11 = thread_profiler->d3d11;
        assert(d3d11 != NULL);
        if (d3d11->device == NULL || d3d11->context == NULL)
            return;

        name_hash = ThreadProfiler_GetNameHash(thread_profiler, g_Remotery->mq_to_rmt_thread, name, hash_cache);

        // Create the D3D11 tree on-demand as the tree needs an up-front-created root.
        // This is not possible to create on initialisation as a D3D11 binding is not yet available.
        d3d_tree = &thread_profiler->sampleTrees[RMT_SampleType_D3D11];
        if (*d3d_tree == NULL)
        {
            rmtError error;
            New_3(SampleTree, *d3d_tree, sizeof(D3D11Sample), (ObjConstructor)D3D11Sample_Constructor,
                  (ObjDestructor)D3D11Sample_Destructor);
            if (error != RMT_ERROR_NONE)
                return;
        }

        // Push the sample and activate the timestamp
        if (ThreadProfiler_Push(*d3d_tree, name_hash, 0, &sample) == RMT_ERROR_NONE)
        {
            D3D11Sample* d3d_sample = (D3D11Sample*)sample;
            D3D11Timestamp_Begin(d3d_sample->timestamp, d3d11->context);
        }
    }
}

static rmtBool GetD3D11SampleTimes(Sample* sample, ThreadProfiler* thread_profiler, rmtU64* out_first_timestamp,
                                   rmtU64* out_last_resync)
{
    Sample* child;

    D3D11Sample* d3d_sample = (D3D11Sample*)sample;

    assert(sample != NULL);
    if (d3d_sample->timestamp != NULL)
    {
        HRESULT result;

        D3D11* d3d11 = thread_profiler->d3d11;
        assert(d3d11 != NULL);

        assert(out_last_resync != NULL);

#if (RMT_GPU_CPU_SYNC_SECONDS > 0)
        if (*out_last_resync < d3d_sample->timestamp->cpu_timestamp)
        {
            // Convert from us to seconds.
            rmtU64 time_diff = (d3d_sample->timestamp->cpu_timestamp - *out_last_resync) / 1000000ULL;
            if (time_diff > RMT_GPU_CPU_SYNC_SECONDS)
            {
                result = SyncD3D11CpuGpuTimes(d3d11->device, d3d11->context, out_first_timestamp, out_last_resync);
                if (result != S_OK)
                {
                    d3d11->last_error = result;
                    return RMT_FALSE;
                }
            }
        }
#endif

        result = D3D11Timestamp_GetData(d3d_sample->timestamp, d3d11->device, d3d11->context, &sample->us_start,
                                        &sample->us_end, out_first_timestamp, out_last_resync);

        if (result != S_OK)
        {
            d3d11->last_error = result;
            return RMT_FALSE;
        }

        sample->us_length = sample->us_end - sample->us_start;
    }

    // Sum length on the parent to track un-sampled time in the parent
    if (sample->parent != NULL)
    {
        sample->parent->us_sampled_length += sample->us_length;
    }

    // Get child sample times
    for (child = sample->first_child; child != NULL; child = child->next_sibling)
    {
        if (!GetD3D11SampleTimes(child, thread_profiler, out_first_timestamp, out_last_resync))
            return RMT_FALSE;
    }

    return RMT_TRUE;
}

static void UpdateD3D11Frame(ThreadProfiler* thread_profiler)
{
    D3D11* d3d11;

    if (g_Remotery == NULL)
        return;

    d3d11 = thread_profiler->d3d11;
    assert(d3d11 != NULL);

    rmt_BeginCPUSample(rmt_UpdateD3D11Frame, 0);

    // Process all messages in the D3D queue
    for (;;)
    {
        Msg_SampleTree* sample_tree;
        Sample* sample;

        Message* message = rmtMessageQueue_PeekNextMessage(d3d11->mq_to_d3d11_main);
        if (message == NULL)
            break;

        // There's only one valid message type in this queue
        assert(message->id == MsgID_SampleTree);
        sample_tree = (Msg_SampleTree*)message->payload;
        sample = sample_tree->rootSample;
        assert(sample->type == RMT_SampleType_D3D11);

        // Retrieve timing of all D3D11 samples
        // If they aren't ready leave the message unconsumed, holding up later frames and maintaining order
        if (!GetD3D11SampleTimes(sample, thread_profiler, &d3d11->first_timestamp, &d3d11->last_resync))
            break;

        // Pass samples onto the remotery thread for sending to the viewer
        QueueSampleTree(g_Remotery->mq_to_rmt_thread, sample, sample_tree->allocator, sample_tree->threadName,
                             message->threadProfiler, RMT_FALSE);
        rmtMessageQueue_ConsumeNextMessage(d3d11->mq_to_d3d11_main, message);
    }

    rmt_EndCPUSample();
}

RMT_API void _rmt_EndD3D11Sample(void)
{
    ThreadProfiler* thread_profiler;
    D3D11* d3d11;

    if (g_Remotery == NULL)
        return;

    if (ThreadProfilers_GetCurrentThreadProfiler(g_Remotery->threadProfilers, &thread_profiler) == RMT_ERROR_NONE)
    {
        D3D11Sample* d3d_sample;

        // Has D3D11 been unbound?
        d3d11 = thread_profiler->d3d11;
        assert(d3d11 != NULL);
        if (d3d11->device == NULL || d3d11->context == NULL)
            return;

        // Close the timestamp
        d3d_sample = (D3D11Sample*)thread_profiler->sampleTrees[RMT_SampleType_D3D11]->currentParent;
        if (d3d_sample->base.recurse_depth > 0)
        {
            d3d_sample->base.recurse_depth--;
        }
        else
        {
            if (d3d_sample->timestamp != NULL)
                D3D11Timestamp_End(d3d_sample->timestamp, d3d11->context);

            // Send to the update loop for ready-polling
            if (ThreadProfiler_Pop(thread_profiler, d3d11->mq_to_d3d11_main, (Sample*)d3d_sample))
                // Perform ready-polling on popping of the root sample
                UpdateD3D11Frame(thread_profiler);
        }
    }
}

#endif // RMT_USE_D3D11

/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
@OpenGL: OpenGL event sampling
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/

#if RMT_USE_OPENGL

#ifndef APIENTRY
#if defined(__MINGW32__) || defined(__CYGWIN__)
#define APIENTRY __stdcall
#elif (defined(_MSC_VER) && (_MSC_VER >= 800)) || defined(_STDCALL_SUPPORTED) || defined(__BORLANDC__)
#define APIENTRY __stdcall
#else
#define APIENTRY
#endif
#endif

#ifndef GLAPI
#if defined(__MINGW32__) || defined(__CYGWIN__)
#define GLAPI extern
#elif defined(_WIN32)
#define GLAPI WINGDIAPI
#else
#define GLAPI extern
#endif
#endif

#ifndef GLAPIENTRY
#define GLAPIENTRY APIENTRY
#endif

typedef rmtU32 GLenum;
typedef rmtU32 GLuint;
typedef rmtS32 GLint;
typedef rmtS32 GLsizei;
typedef rmtU64 GLuint64;
typedef rmtS64 GLint64;
typedef unsigned char GLubyte;

typedef GLenum(GLAPIENTRY* PFNGLGETERRORPROC)(void);
typedef void(GLAPIENTRY* PFNGLGENQUERIESPROC)(GLsizei n, GLuint* ids);
typedef void(GLAPIENTRY* PFNGLDELETEQUERIESPROC)(GLsizei n, const GLuint* ids);
typedef void(GLAPIENTRY* PFNGLBEGINQUERYPROC)(GLenum target, GLuint id);
typedef void(GLAPIENTRY* PFNGLENDQUERYPROC)(GLenum target);
typedef void(GLAPIENTRY* PFNGLGETQUERYOBJECTIVPROC)(GLuint id, GLenum pname, GLint* params);
typedef void(GLAPIENTRY* PFNGLGETQUERYOBJECTUIVPROC)(GLuint id, GLenum pname, GLuint* params);
typedef void(GLAPIENTRY* PFNGLGETQUERYOBJECTI64VPROC)(GLuint id, GLenum pname, GLint64* params);
typedef void(GLAPIENTRY* PFNGLGETQUERYOBJECTUI64VPROC)(GLuint id, GLenum pname, GLuint64* params);
typedef void(GLAPIENTRY* PFNGLQUERYCOUNTERPROC)(GLuint id, GLenum target);
typedef void(GLAPIENTRY* PFNGLGETINTEGER64VPROC)(GLenum pname, GLint64* data);
typedef void(GLAPIENTRY* PFNGLFINISHPROC)(void);

#define GL_NO_ERROR 0
#define GL_QUERY_RESULT 0x8866
#define GL_QUERY_RESULT_AVAILABLE 0x8867
#define GL_TIME_ELAPSED 0x88BF
#define GL_TIMESTAMP 0x8E28

#define RMT_GL_GET_FUN(x)                  \
    assert(g_Remotery->opengl->x != NULL); \
    g_Remotery->opengl->x

#define rmtglGenQueries RMT_GL_GET_FUN(__glGenQueries)
#define rmtglDeleteQueries RMT_GL_GET_FUN(__glDeleteQueries)
#define rmtglBeginQuery RMT_GL_GET_FUN(__glBeginQuery)
#define rmtglEndQuery RMT_GL_GET_FUN(__glEndQuery)
#define rmtglGetQueryObjectiv RMT_GL_GET_FUN(__glGetQueryObjectiv)
#define rmtglGetQueryObjectuiv RMT_GL_GET_FUN(__glGetQueryObjectuiv)
#define rmtglGetQueryObjecti64v RMT_GL_GET_FUN(__glGetQueryObjecti64v)
#define rmtglGetQueryObjectui64v RMT_GL_GET_FUN(__glGetQueryObjectui64v)
#define rmtglQueryCounter RMT_GL_GET_FUN(__glQueryCounter)
#define rmtglGetInteger64v RMT_GL_GET_FUN(__glGetInteger64v)
#define rmtglFinish RMT_GL_GET_FUN(__glFinish)

struct OpenGL_t
{
    // Handle to the OS OpenGL DLL
    void* dll_handle;

    PFNGLGETERRORPROC __glGetError;
    PFNGLGENQUERIESPROC __glGenQueries;
    PFNGLDELETEQUERIESPROC __glDeleteQueries;
    PFNGLBEGINQUERYPROC __glBeginQuery;
    PFNGLENDQUERYPROC __glEndQuery;
    PFNGLGETQUERYOBJECTIVPROC __glGetQueryObjectiv;
    PFNGLGETQUERYOBJECTUIVPROC __glGetQueryObjectuiv;
    PFNGLGETQUERYOBJECTI64VPROC __glGetQueryObjecti64v;
    PFNGLGETQUERYOBJECTUI64VPROC __glGetQueryObjectui64v;
    PFNGLQUERYCOUNTERPROC __glQueryCounter;
    PFNGLGETINTEGER64VPROC __glGetInteger64v;
    PFNGLFINISHPROC __glFinish;

    // Queue to the OpenGL main update thread
    // Given that BeginSample/EndSample need to be called from the same thread that does the update, there
    // is really no need for this to be a thread-safe queue. I'm using it for its convenience.
    rmtMessageQueue* mq_to_opengl_main;

    // Mark the first time so that remaining timestamps are offset from this
    rmtU64 first_timestamp;
    // Last time in us (CPU time, via usTimer_Get) since we last resync'ed CPU & GPU
    rmtU64 last_resync;

    // Sample trees in transit in the message queue for release on shutdown
    Buffer* flush_samples;
};

static GLenum rmtglGetError(void)
{
    if (g_Remotery != NULL)
    {
        assert(g_Remotery->opengl != NULL);
        if (g_Remotery->opengl->__glGetError != NULL)
            return g_Remotery->opengl->__glGetError();
    }

    return (GLenum)0;
}

#ifdef RMT_PLATFORM_LINUX
#ifdef __cplusplus
extern "C" void* glXGetProcAddressARB(const GLubyte*);
#else
extern void* glXGetProcAddressARB(const GLubyte*);
#endif
#endif

static ProcReturnType rmtglGetProcAddress(OpenGL* opengl, const char* symbol)
{
#if defined(RMT_PLATFORM_WINDOWS)
    {
        // Get OpenGL extension-loading function for each call
        typedef ProcReturnType(WINAPI * wglGetProcAddressFn)(LPCSTR);
        assert(opengl != NULL);
        {
            wglGetProcAddressFn wglGetProcAddress =
                (wglGetProcAddressFn)rmtGetProcAddress(opengl->dll_handle, "wglGetProcAddress");
            if (wglGetProcAddress != NULL)
                return wglGetProcAddress(symbol);
        }
    }

#elif defined(RMT_PLATFORM_MACOS) && !defined(GLEW_APPLE_GLX)

    return rmtGetProcAddress(opengl->dll_handle, symbol);

#elif defined(RMT_PLATFORM_LINUX)

    return glXGetProcAddressARB((const GLubyte*)symbol);

#endif

    return NULL;
}

static rmtError OpenGL_Create(OpenGL** opengl)
{
    rmtError error;

    assert(opengl != NULL);

    *opengl = (OpenGL*)rmtMalloc(sizeof(OpenGL));
    if (*opengl == NULL)
        return RMT_ERROR_MALLOC_FAIL;

    (*opengl)->dll_handle = NULL;

    (*opengl)->__glGetError = NULL;
    (*opengl)->__glGenQueries = NULL;
    (*opengl)->__glDeleteQueries = NULL;
    (*opengl)->__glBeginQuery = NULL;
    (*opengl)->__glEndQuery = NULL;
    (*opengl)->__glGetQueryObjectiv = NULL;
    (*opengl)->__glGetQueryObjectuiv = NULL;
    (*opengl)->__glGetQueryObjecti64v = NULL;
    (*opengl)->__glGetQueryObjectui64v = NULL;
    (*opengl)->__glQueryCounter = NULL;
    (*opengl)->__glGetInteger64v = NULL;
    (*opengl)->__glFinish = NULL;

    (*opengl)->mq_to_opengl_main = NULL;
    (*opengl)->first_timestamp = 0;
    (*opengl)->last_resync = 0;
    (*opengl)->flush_samples = NULL;

    New_1(Buffer, (*opengl)->flush_samples, 8 * 1024);
    if (error != RMT_ERROR_NONE)
    {
        Delete(OpenGL, *opengl);
        return error;
    }

    New_1(rmtMessageQueue, (*opengl)->mq_to_opengl_main, g_Settings.messageQueueSizeInBytes);

    return error;
}

static void OpenGL_Destructor(OpenGL* opengl)
{
    assert(opengl != NULL);
    Delete(rmtMessageQueue, opengl->mq_to_opengl_main);
    Delete(Buffer, opengl->flush_samples);
}

static void SyncOpenGLCpuGpuTimes(rmtU64* out_first_timestamp, rmtU64* out_last_resync)
{
    rmtU64 cpu_time_start = 0;
    rmtU64 cpu_time_stop = 0;
    rmtU64 average_half_RTT = 0; // RTT = Rountrip Time.
    GLint64 gpu_base = 0;
    int i;

    rmtglFinish();

    for (i = 0; i < RMT_GPU_CPU_SYNC_NUM_ITERATIONS; ++i)
    {
        rmtU64 half_RTT;

        rmtglFinish();
        cpu_time_start = usTimer_Get(&g_Remotery->timer);
        rmtglGetInteger64v(GL_TIMESTAMP, &gpu_base);
        cpu_time_stop = usTimer_Get(&g_Remotery->timer);
        // Average the time it takes a roundtrip from CPU to GPU
        // while doing nothing other than getting timestamps
        half_RTT = (cpu_time_stop - cpu_time_start) >> 1ULL;
        if (i == 0)
            average_half_RTT = half_RTT;
        else
            average_half_RTT = (average_half_RTT + half_RTT) >> 1ULL;
    }

    // All GPU times are offset from gpu_base, and then taken to
    // the same relative origin CPU timestamps are based on.
    // CPU is in us, we must translate it to ns.
    *out_first_timestamp = (rmtU64)(gpu_base) - (cpu_time_start + average_half_RTT) * 1000ULL;
    *out_last_resync = cpu_time_stop;
}

typedef struct OpenGLTimestamp
{
    // Inherit so that timestamps can be quickly allocated
    ObjectLink Link;

    // Pair of timestamp queries that wrap the sample
    GLuint queries[2];
    rmtU64 cpu_timestamp;
} OpenGLTimestamp;

static rmtError OpenGLTimestamp_Constructor(OpenGLTimestamp* stamp)
{
    GLenum error;

    assert(stamp != NULL);

    ObjectLink_Constructor((ObjectLink*)stamp);

    // Set defaults
    stamp->queries[0] = stamp->queries[1] = 0;
    stamp->cpu_timestamp = 0;

    // Empty the error queue before using it for glGenQueries
    while ((error = rmtglGetError()) != GL_NO_ERROR)
        ;

    // Create start/end timestamp queries
    assert(g_Remotery != NULL);
    rmtglGenQueries(2, stamp->queries);
    error = rmtglGetError();
    if (error != GL_NO_ERROR)
        return RMT_ERROR_OPENGL_ERROR;

    return RMT_ERROR_NONE;
}

static void OpenGLTimestamp_Destructor(OpenGLTimestamp* stamp)
{
    assert(stamp != NULL);

    // Destroy queries
    if (stamp->queries[0] != 0)
        rmtglDeleteQueries(2, stamp->queries);
}

static void OpenGLTimestamp_Begin(OpenGLTimestamp* stamp)
{
    assert(stamp != NULL);

    // First query
    assert(g_Remotery != NULL);
    stamp->cpu_timestamp = usTimer_Get(&g_Remotery->timer);
    rmtglQueryCounter(stamp->queries[0], GL_TIMESTAMP);
}

static void OpenGLTimestamp_End(OpenGLTimestamp* stamp)
{
    assert(stamp != NULL);

    // Second query
    assert(g_Remotery != NULL);
    rmtglQueryCounter(stamp->queries[1], GL_TIMESTAMP);
}

static rmtBool OpenGLTimestamp_GetData(OpenGLTimestamp* stamp, rmtU64* out_start, rmtU64* out_end,
                                       rmtU64* out_first_timestamp, rmtU64* out_last_resync)
{
    GLuint64 start = 0, end = 0;
    GLint startAvailable = 0, endAvailable = 0;

    assert(g_Remotery != NULL);

    assert(stamp != NULL);
    assert(stamp->queries[0] != 0 && stamp->queries[1] != 0);

    // Check to see if all queries are ready
    // If any fail to arrive, wait until later
    rmtglGetQueryObjectiv(stamp->queries[0], GL_QUERY_RESULT_AVAILABLE, &startAvailable);
    if (!startAvailable)
        return RMT_FALSE;
    rmtglGetQueryObjectiv(stamp->queries[1], GL_QUERY_RESULT_AVAILABLE, &endAvailable);
    if (!endAvailable)
        return RMT_FALSE;

    rmtglGetQueryObjectui64v(stamp->queries[0], GL_QUERY_RESULT, &start);
    rmtglGetQueryObjectui64v(stamp->queries[1], GL_QUERY_RESULT, &end);

    // Mark the first timestamp. We may resync if we detect the GPU timestamp is in the
    // past (i.e. happened before the CPU command) since it should be impossible.
    assert(out_first_timestamp != NULL);
    if (*out_first_timestamp == 0 || ((start - *out_first_timestamp) / 1000ULL) < stamp->cpu_timestamp)
        SyncOpenGLCpuGpuTimes(out_first_timestamp, out_last_resync);

    // Calculate start and end timestamps (we want us, the queries give us ns)
    *out_start = (rmtU64)(start - *out_first_timestamp) / 1000ULL;
    *out_end = (rmtU64)(end - *out_first_timestamp) / 1000ULL;

    return RMT_TRUE;
}

typedef struct OpenGLSample
{
    // IS-A inheritance relationship
    Sample base;

    OpenGLTimestamp* timestamp;

} OpenGLSample;

static rmtError OpenGLSample_Constructor(OpenGLSample* sample)
{
    rmtError error;

    assert(sample != NULL);

    // Chain to sample constructor
    Sample_Constructor((Sample*)sample);
    sample->base.type = RMT_SampleType_OpenGL;
    New_0(OpenGLTimestamp, sample->timestamp);

    return RMT_ERROR_NONE;
}

static void OpenGLSample_Destructor(OpenGLSample* sample)
{
    Delete(OpenGLTimestamp, sample->timestamp);
    Sample_Destructor((Sample*)sample);
}

RMT_API void _rmt_BindOpenGL()
{
    if (g_Remotery != NULL)
    {
        OpenGL* opengl = g_Remotery->opengl;
        assert(opengl != NULL);

#if defined(RMT_PLATFORM_WINDOWS)
        opengl->dll_handle = rmtLoadLibrary("opengl32.dll");
#elif defined(RMT_PLATFORM_MACOS)
        opengl->dll_handle = rmtLoadLibrary("/System/Library/Frameworks/OpenGL.framework/Versions/Current/OpenGL");
#elif defined(RMT_PLATFORM_LINUX)
        opengl->dll_handle = rmtLoadLibrary("libGL.so");
#endif

        opengl->__glGetError = (PFNGLGETERRORPROC)rmtGetProcAddress(opengl->dll_handle, "glGetError");
        opengl->__glGenQueries = (PFNGLGENQUERIESPROC)rmtglGetProcAddress(opengl, "glGenQueries");
        opengl->__glDeleteQueries = (PFNGLDELETEQUERIESPROC)rmtglGetProcAddress(opengl, "glDeleteQueries");
        opengl->__glBeginQuery = (PFNGLBEGINQUERYPROC)rmtglGetProcAddress(opengl, "glBeginQuery");
        opengl->__glEndQuery = (PFNGLENDQUERYPROC)rmtglGetProcAddress(opengl, "glEndQuery");
        opengl->__glGetQueryObjectiv = (PFNGLGETQUERYOBJECTIVPROC)rmtglGetProcAddress(opengl, "glGetQueryObjectiv");
        opengl->__glGetQueryObjectuiv = (PFNGLGETQUERYOBJECTUIVPROC)rmtglGetProcAddress(opengl, "glGetQueryObjectuiv");
        opengl->__glGetQueryObjecti64v =
            (PFNGLGETQUERYOBJECTI64VPROC)rmtglGetProcAddress(opengl, "glGetQueryObjecti64v");
        opengl->__glGetQueryObjectui64v =
            (PFNGLGETQUERYOBJECTUI64VPROC)rmtglGetProcAddress(opengl, "glGetQueryObjectui64v");
        opengl->__glQueryCounter = (PFNGLQUERYCOUNTERPROC)rmtglGetProcAddress(opengl, "glQueryCounter");
        opengl->__glGetInteger64v = (PFNGLGETINTEGER64VPROC)rmtglGetProcAddress(opengl, "glGetInteger64v");
        opengl->__glFinish = (PFNGLFINISHPROC)rmtGetProcAddress(opengl->dll_handle, "glFinish");
    }
}

static void UpdateOpenGLFrame(void);

RMT_API void _rmt_UnbindOpenGL(void)
{
    if (g_Remotery != NULL)
    {
        OpenGL* opengl = g_Remotery->opengl;
        assert(opengl != NULL);

        // Stall waiting for the OpenGL queue to empty into the Remotery queue
        while (!rmtMessageQueue_IsEmpty(opengl->mq_to_opengl_main))
            UpdateOpenGLFrame();

        // There will be a whole bunch of OpenGL sample trees queued up the remotery queue that need releasing
        FreePendingSampleTrees(g_Remotery, RMT_SampleType_OpenGL, opengl->flush_samples);

        // Forcefully delete sample tree on this thread to release time stamps from
        // the same thread that created them
        Remotery_DeleteSampleTree(g_Remotery, RMT_SampleType_OpenGL);

        // Release reference to the OpenGL DLL
        if (opengl->dll_handle != NULL)
        {
            rmtFreeLibrary(opengl->dll_handle);
            opengl->dll_handle = NULL;
        }
    }
}

RMT_API void _rmt_BeginOpenGLSample(rmtPStr name, rmtU32* hash_cache)
{
    ThreadProfiler* thread_profiler;

    if (g_Remotery == NULL)
        return;

    if (ThreadProfilers_GetCurrentThreadProfiler(g_Remotery->threadProfilers, &thread_profiler) == RMT_ERROR_NONE)
    {
        Sample* sample;
        rmtU32 name_hash = ThreadProfiler_GetNameHash(thread_profiler, g_Remotery->mq_to_rmt_thread, name, hash_cache);

        // Create the OpenGL tree on-demand as the tree needs an up-front-created root.
        // This is not possible to create on initialisation as a OpenGL binding is not yet available.
        SampleTree** ogl_tree = &thread_profiler->sampleTrees[RMT_SampleType_OpenGL];
        if (*ogl_tree == NULL)
        {
            rmtError error;
            New_3(SampleTree, *ogl_tree, sizeof(OpenGLSample), (ObjConstructor)OpenGLSample_Constructor,
                  (ObjDestructor)OpenGLSample_Destructor);
            if (error != RMT_ERROR_NONE)
                return;
        }

        // Push the sample and activate the timestamp
        if (ThreadProfiler_Push(*ogl_tree, name_hash, 0, &sample) == RMT_ERROR_NONE)
        {
            OpenGLSample* ogl_sample = (OpenGLSample*)sample;
            OpenGLTimestamp_Begin(ogl_sample->timestamp);
        }
    }
}

static rmtBool GetOpenGLSampleTimes(Sample* sample, rmtU64* out_first_timestamp, rmtU64* out_last_resync)
{
    Sample* child;

    OpenGLSample* ogl_sample = (OpenGLSample*)sample;

    assert(sample != NULL);
    if (ogl_sample->timestamp != NULL)
    {
        assert(out_last_resync != NULL);
#if (RMT_GPU_CPU_SYNC_SECONDS > 0)
        if (*out_last_resync < ogl_sample->timestamp->cpu_timestamp)
        {
            // Convert from us to seconds.
            rmtU64 time_diff = (ogl_sample->timestamp->cpu_timestamp - *out_last_resync) / 1000000ULL;
            if (time_diff > RMT_GPU_CPU_SYNC_SECONDS)
                SyncOpenGLCpuGpuTimes(out_first_timestamp, out_last_resync);
        }
#endif

        if (!OpenGLTimestamp_GetData(ogl_sample->timestamp, &sample->us_start, &sample->us_end, out_first_timestamp,
                                     out_last_resync))
            return RMT_FALSE;

        sample->us_length = sample->us_end - sample->us_start;
    }

    // Get child sample times
    for (child = sample->first_child; child != NULL; child = child->next_sibling)
    {
        if (!GetOpenGLSampleTimes(child, out_first_timestamp, out_last_resync))
            return RMT_FALSE;
    }

    return RMT_TRUE;
}

static void UpdateOpenGLFrame(void)
{
    OpenGL* opengl;

    if (g_Remotery == NULL)
        return;

    opengl = g_Remotery->opengl;
    assert(opengl != NULL);

    rmt_BeginCPUSample(rmt_UpdateOpenGLFrame, 0);

    // Process all messages in the OpenGL queue
    while (1)
    {
        Msg_SampleTree* sample_tree;
        Sample* sample;

        Message* message = rmtMessageQueue_PeekNextMessage(opengl->mq_to_opengl_main);
        if (message == NULL)
            break;

        // There's only one valid message type in this queue
        assert(message->id == MsgID_SampleTree);
        sample_tree = (Msg_SampleTree*)message->payload;
        sample = sample_tree->rootSample;
        assert(sample->type == RMT_SampleType_OpenGL);

        // Retrieve timing of all OpenGL samples
        // If they aren't ready leave the message unconsumed, holding up later frames and maintaining order
        if (!GetOpenGLSampleTimes(sample, &opengl->first_timestamp, &opengl->last_resync))
            break;

        // Pass samples onto the remotery thread for sending to the viewer
        QueueSampleTree(g_Remotery->mq_to_rmt_thread, sample, sample_tree->allocator, sample_tree->threadName,
                             message->threadProfiler, RMT_FALSE);
        rmtMessageQueue_ConsumeNextMessage(opengl->mq_to_opengl_main, message);
    }

    rmt_EndCPUSample();
}

RMT_API void _rmt_EndOpenGLSample(void)
{
    ThreadProfiler* thread_profiler;

    if (g_Remotery == NULL)
        return;

    if (ThreadProfilers_GetCurrentThreadProfiler(g_Remotery->threadProfilers, &thread_profiler) == RMT_ERROR_NONE)
    {
        // Close the timestamp
        OpenGLSample* ogl_sample = (OpenGLSample*)thread_profiler->sampleTrees[RMT_SampleType_OpenGL]->currentParent;
        if (ogl_sample->base.recurse_depth > 0)
        {
            ogl_sample->base.recurse_depth--;
        }
        else
        {
            if (ogl_sample->timestamp != NULL)
                OpenGLTimestamp_End(ogl_sample->timestamp);

            // Send to the update loop for ready-polling
            if (ThreadProfiler_Pop(thread_profiler, g_Remotery->opengl->mq_to_opengl_main, (Sample*)ogl_sample))
                // Perform ready-polling on popping of the root sample
                UpdateOpenGLFrame();
        }
    }
}

#endif // RMT_USE_OPENGL

/*
 ------------------------------------------------------------------------------------------------------------------------
 ------------------------------------------------------------------------------------------------------------------------
 @Metal: Metal event sampling
 ------------------------------------------------------------------------------------------------------------------------
 ------------------------------------------------------------------------------------------------------------------------
 */

#if RMT_USE_METAL

struct Metal_t
{
    // Queue to the Metal main update thread
    // Given that BeginSample/EndSample need to be called from the same thread that does the update, there
    // is really no need for this to be a thread-safe queue. I'm using it for its convenience.
    rmtMessageQueue* mq_to_metal_main;
};

static rmtError Metal_Create(Metal** metal)
{
    rmtError error;

    assert(metal != NULL);

    *metal = (Metal*)rmtMalloc(sizeof(Metal));
    if (*metal == NULL)
        return RMT_ERROR_MALLOC_FAIL;

    (*metal)->mq_to_metal_main = NULL;

    New_1(rmtMessageQueue, (*metal)->mq_to_metal_main, g_Settings.messageQueueSizeInBytes);
    return error;
}

static void Metal_Destructor(Metal* metal)
{
    assert(metal != NULL);
    Delete(rmtMessageQueue, metal->mq_to_metal_main);
}

typedef struct MetalTimestamp
{
    // Inherit so that timestamps can be quickly allocated
    ObjectLink Link;

    // Output from GPU callbacks
    rmtU64 start;
    rmtU64 end;
    rmtBool ready;
} MetalTimestamp;

static rmtError MetalTimestamp_Constructor(MetalTimestamp* stamp)
{
    assert(stamp != NULL);

    ObjectLink_Constructor((ObjectLink*)stamp);

    // Set defaults
    stamp->start = 0;
    stamp->end = 0;
    stamp->ready = RMT_FALSE;

    return RMT_ERROR_NONE;
}

static void MetalTimestamp_Destructor(MetalTimestamp* stamp)
{
    assert(stamp != NULL);
}

rmtU64 rmtMetal_usGetTime()
{
    // Share the CPU timer for auto-sync
    assert(g_Remotery != NULL);
    return usTimer_Get(&g_Remotery->timer);
}

void rmtMetal_MeasureCommandBuffer(unsigned long long* out_start, unsigned long long* out_end, unsigned int* out_ready);

static void MetalTimestamp_Begin(MetalTimestamp* stamp)
{
    assert(stamp != NULL);
    stamp->ready = RMT_FALSE;

    // Metal can currently only issue callbacks at the command buffer level
    // So for now measure execution of the entire command buffer
    rmtMetal_MeasureCommandBuffer(&stamp->start, &stamp->end, &stamp->ready);
}

static void MetalTimestamp_End(MetalTimestamp* stamp)
{
    assert(stamp != NULL);

    // As Metal can currently only measure entire command buffers, this function is a no-op
    // as the completed handler was already issued in Begin
}

static rmtBool MetalTimestamp_GetData(MetalTimestamp* stamp, rmtU64* out_start, rmtU64* out_end)
{
    assert(g_Remotery != NULL);
    assert(stamp != NULL);

    // GPU writes ready flag when complete handler is called
    if (stamp->ready == RMT_FALSE)
        return RMT_FALSE;

    *out_start = stamp->start;
    *out_end = stamp->end;

    return RMT_TRUE;
}

typedef struct MetalSample
{
    // IS-A inheritance relationship
    Sample base;

    MetalTimestamp* timestamp;

} MetalSample;

static rmtError MetalSample_Constructor(MetalSample* sample)
{
    rmtError error;

    assert(sample != NULL);

    // Chain to sample constructor
    Sample_Constructor((Sample*)sample);
    sample->base.type = RMT_SampleType_Metal;
    New_0(MetalTimestamp, sample->timestamp);

    return RMT_ERROR_NONE;
}

static void MetalSample_Destructor(MetalSample* sample)
{
    Delete(MetalTimestamp, sample->timestamp);
    Sample_Destructor((Sample*)sample);
}

static void UpdateOpenGLFrame(void);

/*RMT_API void _rmt_UnbindMetal(void)
{
    if (g_Remotery != NULL)
    {
        Metal* metal = g_Remotery->metal;
        assert(metal != NULL);

        // Stall waiting for the Metal queue to empty into the Remotery queue
        while (!rmtMessageQueue_IsEmpty(metal->mq_to_metal_main))
            UpdateMetalFrame();

        // Forcefully delete sample tree on this thread to release time stamps from
        // the same thread that created them
        Remotery_BlockingDeleteSampleTree(g_Remotery, RMT_SampleType_Metal);
    }
}*/

RMT_API void _rmt_BeginMetalSample(rmtPStr name, rmtU32* hash_cache)
{
    ThreadProfiler* thread_profiler;

    if (g_Remotery == NULL)
        return;

    if (ThreadProfilers_GetCurrentThreadProfiler(g_Remotery->threadProfilers, &thread_profiler) == RMT_ERROR_NONE)
    {
        Sample* sample;
        rmtU32 name_hash = ThreadProfiler_GetNameHash(thread_profiler, g_Remotery->mq_to_rmt_thread, name, hash_cache);

        // Create the Metal tree on-demand as the tree needs an up-front-created root.
        // This is not possible to create on initialisation as a Metal binding is not yet available.
        SampleTree** metal_tree = &thread_profiler->sampleTrees[RMT_SampleType_Metal];
        if (*metal_tree == NULL)
        {
            rmtError error;
            New_3(SampleTree, *metal_tree, sizeof(MetalSample), (ObjConstructor)MetalSample_Constructor,
                  (ObjDestructor)MetalSample_Destructor);
            if (error != RMT_ERROR_NONE)
                return;
        }

        // Push the sample and activate the timestamp
        if (ThreadProfiler_Push(*metal_tree, name_hash, 0, &sample) == RMT_ERROR_NONE)
        {
            MetalSample* metal_sample = (MetalSample*)sample;
            MetalTimestamp_Begin(metal_sample->timestamp);
        }
    }
}

static rmtBool GetMetalSampleTimes(Sample* sample)
{
    Sample* child;

    MetalSample* metal_sample = (MetalSample*)sample;

    assert(sample != NULL);
    if (metal_sample->timestamp != NULL)
    {
        if (!MetalTimestamp_GetData(metal_sample->timestamp, &sample->us_start, &sample->us_end))
            return RMT_FALSE;

        sample->us_length = sample->us_end - sample->us_start;
    }

    // Get child sample times
    for (child = sample->first_child; child != NULL; child = child->next_sibling)
    {
        if (!GetMetalSampleTimes(child))
            return RMT_FALSE;
    }

    return RMT_TRUE;
}

static void UpdateMetalFrame(void)
{
    Metal* metal;

    if (g_Remotery == NULL)
        return;

    metal = g_Remotery->metal;
    assert(metal != NULL);

    rmt_BeginCPUSample(rmt_UpdateMetalFrame, 0);

    // Process all messages in the Metal queue
    while (1)
    {
        Msg_SampleTree* sample_tree;
        Sample* sample;

        Message* message = rmtMessageQueue_PeekNextMessage(metal->mq_to_metal_main);
        if (message == NULL)
            break;

        // There's only one valid message type in this queue
        assert(message->id == MsgID_SampleTree);
        sample_tree = (Msg_SampleTree*)message->payload;
        sample = sample_tree->rootSample;
        assert(sample->type == RMT_SampleType_Metal);

        // Retrieve timing of all Metal samples
        // If they aren't ready leave the message unconsumed, holding up later frames and maintaining order
        if (!GetMetalSampleTimes(sample))
            break;

        // Pass samples onto the remotery thread for sending to the viewer
        QueueSampleTree(g_Remotery->mq_to_rmt_thread, sample, sample_tree->allocator, sample_tree->threadName,
                             message->threadProfiler, RMT_FALSE);
        rmtMessageQueue_ConsumeNextMessage(metal->mq_to_metal_main, message);
    }

    rmt_EndCPUSample();
}

RMT_API void _rmt_EndMetalSample(void)
{
    ThreadProfiler* thread_profiler;

    if (g_Remotery == NULL)
        return;

    if (ThreadProfilers_GetCurrentThreadProfiler(g_Remotery->threadProfilers, &thread_profiler) == RMT_ERROR_NONE)
    {
        // Close the timestamp
        MetalSample* metal_sample = (MetalSample*)thread_profiler->sampleTrees[RMT_SampleType_Metal]->currentParent;
        if (metal_sample->base.recurse_depth > 0)
        {
            metal_sample->base.recurse_depth--;
        }
        else
        {
            if (metal_sample->timestamp != NULL)
                MetalTimestamp_End(metal_sample->timestamp);

            // Send to the update loop for ready-polling
            if (ThreadProfiler_Pop(thread_profiler, g_Remotery->metal->mq_to_metal_main, (Sample*)metal_sample))
                // Perform ready-polling on popping of the root sample
                UpdateMetalFrame();
        }
    }
}

#endif // RMT_USE_METAL

// Iterator
RMT_API void _rmt_IterateChildren(rmtSampleIterator* iterator, rmtSample* sample)
{
    iterator->sample = 0;
    iterator->initial = sample != NULL ? sample->first_child : 0;
}

RMT_API rmtBool _rmt_IterateNext(rmtSampleIterator* iter)
{
    if (iter->initial != NULL)
    {
        iter->sample = iter->initial;
        iter->initial = 0;
    }
    else
    {
        if (iter->sample != NULL)
            iter->sample = iter->sample->next_sibling;
    }

    return iter->sample != NULL ? RMT_TRUE : RMT_FALSE;
}

// Sample tree accessors
RMT_API const char* _rmt_SampleTreeGetThreadName(rmtSampleTree* sample_tree)
{
    return sample_tree->threadName;
}

RMT_API rmtSample* _rmt_SampleTreeGetRootSample(rmtSampleTree* sample_tree)
{
    return sample_tree->rootSample;
}

// Sample accessors
RMT_API const char* _rmt_SampleGetName(rmtSample* sample)
{
    const char* name = StringTable_Find(g_Remotery->string_table, sample->name_hash);
    if (name == NULL)
    {
        return "null";
    }
    return name;
}

RMT_API rmtU32 _rmt_SampleGetNameHash(rmtSample* sample)
{
    return sample->name_hash;
}

RMT_API rmtU32 _rmt_SampleGetCallCount(rmtSample* sample)
{
    return sample->call_count;
}

RMT_API rmtU64 _rmt_SampleGetStart(rmtSample* sample)
{
    return sample->us_start;
}

RMT_API rmtU64 _rmt_SampleGetTime(rmtSample* sample)
{
    return sample->us_length;
}

RMT_API rmtU64 _rmt_SampleGetSelfTime(rmtSample* sample)
{
    return (rmtU64)maxS64(sample->us_length - sample->us_sampled_length, 0);
}

RMT_API rmtSampleType _rmt_SampleGetType(rmtSample* sample)
{
    return sample->type;
}

static rmtU8 _rmt_HexDigitToByte(rmtU8 c)
{
    if (c >= (rmtU8)'a')
        return 0xA + (c - (rmtU8)'a');
    return c - (rmtU8)'0';
}

RMT_API void _rmt_SampleGetColour(rmtSample* sample, rmtU8* r, rmtU8* g, rmtU8* b)
{
    rmtU8* p = sample->unique_id_html_colour + 1; // skip the '#' sign
    *r = _rmt_HexDigitToByte(p[0]) << 4 | _rmt_HexDigitToByte(p[1]);
    *g = _rmt_HexDigitToByte(p[2]) << 4 | _rmt_HexDigitToByte(p[3]);
    *b = _rmt_HexDigitToByte(p[4]) << 4 | _rmt_HexDigitToByte(p[5]);
}


#endif // RMT_ENABLED
