

/*
Copyright 2014-2018 Celtoys Ltd

Licensed under the Apache License, Version 2.0 (the "License");
you may not use this file except in compliance with the License.
You may obtain a copy of the License at

    http://www.apache.org/licenses/LICENSE-2.0

Unless required by applicable law or agreed to in writing, software
distributed under the License is distributed on an "AS IS" BASIS,
WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
See the License for the specific language governing permissions and
limitations under the License.
*/


/*

Compiling
---------

* Windows (MSVC) - add lib/Remotery.c and lib/Remotery.h to your program. Set include
  directories to add Remotery/lib path. The required library ws2_32.lib should be picked
  up through the use of the #pragma comment(lib, "ws2_32.lib") directive in Remotery.c.

* Mac OS X (XCode) - simply add lib/Remotery.c and lib/Remotery.h to your program.

* Linux (GCC) - add the source in lib folder. Compilation of the code requires -pthreads for
  library linkage. For example to compile the same run: cc lib/Remotery.c sample/sample.c
  -I lib -pthread -lm

You can define some extra macros to modify what features are compiled into Remotery. These are
documented just below this comment.

*/


#ifndef RMT_INCLUDED_H
#define RMT_INCLUDED_H


// Set to 0 to not include any bits of Remotery in your build
#ifndef RMT_ENABLED
#define RMT_ENABLED 1
#endif

// Help performance of the server sending data to the client by marking this machine as little-endian 
#ifndef RMT_ASSUME_LITTLE_ENDIAN
#define RMT_ASSUME_LITTLE_ENDIAN 0
#endif

// Used by the Celtoys TinyCRT library (not released yet)
#ifndef RMT_USE_TINYCRT
#define RMT_USE_TINYCRT 0
#endif

// Assuming CUDA headers/libs are setup, allow CUDA profiling
#ifndef RMT_USE_CUDA
#define RMT_USE_CUDA 0
#endif

// Assuming Direct3D 11 headers/libs are setup, allow D3D11 profiling
#ifndef RMT_USE_D3D11
#define RMT_USE_D3D11 0
#endif

// Allow OpenGL profiling
#ifndef RMT_USE_OPENGL
#define RMT_USE_OPENGL 0
#endif

// Allow Metal profiling
#ifndef RMT_USE_METAL
#define RMT_USE_METAL 0
#endif

// Initially use POSIX thread names to name threads instead of Thread0, 1, ...
#ifndef RMT_USE_POSIX_THREADNAMES
#define RMT_USE_POSIX_THREADNAMES 0
#endif

// How many times we spin data back and forth between CPU & GPU
// to calculate average RTT (Roundtrip Time). Cannot be 0.
// Affects OpenGL & D3D11
#ifndef RMT_GPU_CPU_SYNC_NUM_ITERATIONS
#define RMT_GPU_CPU_SYNC_NUM_ITERATIONS 16
#endif

// Time in seconds between each resync to compensate for drifting between GPU & CPU timers,
// effects of power saving, etc. Resyncs can cause stutter, lag spikes, stalls.
// Set to 0 for never.
// Affects OpenGL & D3D11
#ifndef RMT_GPU_CPU_SYNC_SECONDS
#define RMT_GPU_CPU_SYNC_SECONDS 30
#endif

// Whether we should automatically resync if we detect a timer disjoint (e.g.
// changed from AC power to battery, GPU is overheating, or throttling up/down
// due to laptop savings events). Set it to 0 to avoid resync in such events.
// Useful if for some odd reason a driver reports a lot of disjoints.
// Affects D3D11
#ifndef RMT_D3D11_RESYNC_ON_DISJOINT
#define RMT_D3D11_RESYNC_ON_DISJOINT 1
#endif


/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   Compiler/Platform Detection and Preprocessor Utilities
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/


// Platform identification
#if defined(_WINDOWS) || defined(_WIN32)
    #define RMT_PLATFORM_WINDOWS
#elif defined(__linux__) || defined(__FreeBSD__) || defined(__OpenBSD__)
    #define RMT_PLATFORM_LINUX
    #define RMT_PLATFORM_POSIX
#elif defined(__APPLE__)
    #define RMT_PLATFORM_MACOS
    #define RMT_PLATFORM_POSIX
#endif

// Architecture identification
#ifdef RMT_PLATFORM_WINDOWS
#ifdef _M_AMD64
#define RMT_ARCH_64BIT
#else
#define RMT_ARCH_32BIT
#endif
#endif

#ifdef RMT_DLL
    #if defined (RMT_PLATFORM_WINDOWS)
        #if defined (RMT_IMPL)
            #define RMT_API __declspec(dllexport)
        #else
            #define RMT_API __declspec(dllimport)
        #endif
    #elif defined (RMT_PLATFORM_POSIX)
        #if defined (RMT_IMPL)
            #define RMT_API __attribute__((visibility("default")))
        #else
            #define RMT_API
        #endif
    #endif
#else
    #define RMT_API
#endif

// Allows macros to be written that can work around the inability to do: #define(x) #ifdef x
// with the C preprocessor.
#if RMT_ENABLED
    #define IFDEF_RMT_ENABLED(t, f) t
#else
    #define IFDEF_RMT_ENABLED(t, f) f
#endif
#if RMT_ENABLED && RMT_USE_CUDA
    #define IFDEF_RMT_USE_CUDA(t, f) t
#else
    #define IFDEF_RMT_USE_CUDA(t, f) f
#endif
#if RMT_ENABLED && RMT_USE_D3D11
    #define IFDEF_RMT_USE_D3D11(t, f) t
#else
    #define IFDEF_RMT_USE_D3D11(t, f) f
#endif
#if RMT_ENABLED && RMT_USE_OPENGL
    #define IFDEF_RMT_USE_OPENGL(t, f) t
#else
    #define IFDEF_RMT_USE_OPENGL(t, f) f
#endif
#if RMT_ENABLED && RMT_USE_METAL
    #define IFDEF_RMT_USE_METAL(t, f) t
#else
    #define IFDEF_RMT_USE_METAL(t, f) f
#endif


// Public interface is written in terms of these macros to easily enable/disable itself
#define RMT_OPTIONAL(macro, x) IFDEF_ ## macro(x, )
#define RMT_OPTIONAL_RET(macro, x, y) IFDEF_ ## macro(x, (y))



/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   Types
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/



// Boolean
typedef unsigned int rmtBool;
#define RMT_TRUE ((rmtBool)1)
#define RMT_FALSE ((rmtBool)0)


// Unsigned integer types
typedef unsigned char rmtU8;
typedef unsigned short rmtU16;
typedef unsigned int rmtU32;
typedef unsigned long long rmtU64;


// Signed integer types
typedef char rmtS8;
typedef short rmtS16;
typedef int rmtS32;
typedef long long rmtS64;


// Const, null-terminated string pointer
typedef const char* rmtPStr;

// Opaque pointer for a sample graph tree
typedef struct Msg_SampleTree rmtSampleTree;

// Opaque pointer to a node in the sample graph tree
typedef struct Sample rmtSample;

// Handle to the main remotery instance
typedef struct Remotery Remotery;

// All possible error codes
// clang-format off
typedef enum rmtError
{
    RMT_ERROR_NONE,
    RMT_ERROR_RECURSIVE_SAMPLE,                 // Not an error but an internal message to calling code
    RMT_ERROR_UNKNOWN,                          // An error with a message yet to be defined, only for internal error handling

    // System errors
    RMT_ERROR_MALLOC_FAIL,                      // Malloc call within remotery failed
    RMT_ERROR_TLS_ALLOC_FAIL,                   // Attempt to allocate thread local storage failed
    RMT_ERROR_VIRTUAL_MEMORY_BUFFER_FAIL,       // Failed to create a virtual memory mirror buffer
    RMT_ERROR_CREATE_THREAD_FAIL,               // Failed to create a thread for the server
    RMT_ERROR_OPEN_THREAD_HANDLE_FAIL,          // Failed to open a thread handle, given a thread id

    // Network TCP/IP socket errors
    RMT_ERROR_SOCKET_INIT_NETWORK_FAIL,         // Network initialisation failure (e.g. on Win32, WSAStartup fails)
    RMT_ERROR_SOCKET_CREATE_FAIL,               // Can't create a socket for connection to the remote viewer
    RMT_ERROR_SOCKET_BIND_FAIL,                 // Can't bind a socket for the server
    RMT_ERROR_SOCKET_LISTEN_FAIL,               // Created server socket failed to enter a listen state
    RMT_ERROR_SOCKET_SET_NON_BLOCKING_FAIL,     // Created server socket failed to switch to a non-blocking state
    RMT_ERROR_SOCKET_INVALID_POLL,              // Poll attempt on an invalid socket
    RMT_ERROR_SOCKET_SELECT_FAIL,               // Server failed to call select on socket
    RMT_ERROR_SOCKET_POLL_ERRORS,               // Poll notified that the socket has errors
    RMT_ERROR_SOCKET_ACCEPT_FAIL,               // Server failed to accept connection from client
    RMT_ERROR_SOCKET_SEND_TIMEOUT,              // Timed out trying to send data
    RMT_ERROR_SOCKET_SEND_FAIL,                 // Unrecoverable error occured while client/server tried to send data
    RMT_ERROR_SOCKET_RECV_NO_DATA,              // No data available when attempting a receive
    RMT_ERROR_SOCKET_RECV_TIMEOUT,              // Timed out trying to receive data
    RMT_ERROR_SOCKET_RECV_FAILED,               // Unrecoverable error occured while client/server tried to receive data

    // WebSocket errors
    RMT_ERROR_WEBSOCKET_HANDSHAKE_NOT_GET,      // WebSocket server handshake failed, not HTTP GET
    RMT_ERROR_WEBSOCKET_HANDSHAKE_NO_VERSION,   // WebSocket server handshake failed, can't locate WebSocket version
    RMT_ERROR_WEBSOCKET_HANDSHAKE_BAD_VERSION,  // WebSocket server handshake failed, unsupported WebSocket version
    RMT_ERROR_WEBSOCKET_HANDSHAKE_NO_HOST,      // WebSocket server handshake failed, can't locate host
    RMT_ERROR_WEBSOCKET_HANDSHAKE_BAD_HOST,     // WebSocket server handshake failed, host is not allowed to connect
    RMT_ERROR_WEBSOCKET_HANDSHAKE_NO_KEY,       // WebSocket server handshake failed, can't locate WebSocket key
    RMT_ERROR_WEBSOCKET_HANDSHAKE_BAD_KEY,      // WebSocket server handshake failed, WebSocket key is ill-formed
    RMT_ERROR_WEBSOCKET_HANDSHAKE_STRING_FAIL,  // WebSocket server handshake failed, internal error, bad string code
    RMT_ERROR_WEBSOCKET_DISCONNECTED,           // WebSocket server received a disconnect request and closed the socket
    RMT_ERROR_WEBSOCKET_BAD_FRAME_HEADER,       // Couldn't parse WebSocket frame header
    RMT_ERROR_WEBSOCKET_BAD_FRAME_HEADER_SIZE,  // Partially received wide frame header size
    RMT_ERROR_WEBSOCKET_BAD_FRAME_HEADER_MASK,  // Partially received frame header data mask
    RMT_ERROR_WEBSOCKET_RECEIVE_TIMEOUT,        // Timeout receiving frame header

    RMT_ERROR_REMOTERY_NOT_CREATED,             // Remotery object has not been created
    RMT_ERROR_SEND_ON_INCOMPLETE_PROFILE,       // An attempt was made to send an incomplete profile tree to the client

    // CUDA error messages
    RMT_ERROR_CUDA_DEINITIALIZED,               // This indicates that the CUDA driver is in the process of shutting down
    RMT_ERROR_CUDA_NOT_INITIALIZED,             // This indicates that the CUDA driver has not been initialized with cuInit() or that initialization has failed
    RMT_ERROR_CUDA_INVALID_CONTEXT,             // This most frequently indicates that there is no context bound to the current thread
    RMT_ERROR_CUDA_INVALID_VALUE,               // This indicates that one or more of the parameters passed to the API call is not within an acceptable range of values
    RMT_ERROR_CUDA_INVALID_HANDLE,              // This indicates that a resource handle passed to the API call was not valid
    RMT_ERROR_CUDA_OUT_OF_MEMORY,               // The API call failed because it was unable to allocate enough memory to perform the requested operation
    RMT_ERROR_ERROR_NOT_READY,                  // This indicates that a resource handle passed to the API call was not valid

    // Direct3D 11 error messages
    RMT_ERROR_D3D11_FAILED_TO_CREATE_QUERY,     // Failed to create query for sample

    // OpenGL error messages
    RMT_ERROR_OPENGL_ERROR,                     // Generic OpenGL error, no need to expose detail since app will need an OpenGL error callback registered

    RMT_ERROR_CUDA_UNKNOWN,
} rmtError;
// clang-format on

typedef enum rmtSampleFlags
{
    // Default behaviour
    RMTSF_None = 0,

    // Search parent for same-named samples and merge timing instead of adding a new sample
    RMTSF_Aggregate = 1,

    // Merge sample with parent if it's the same sample
    RMTSF_Recursive = 2,

    // Set this flag on any of your root samples so that Remotery will assert if it ends up *not* being the root sample.
    // This will quickly allow you to detect Begin/End mismatches causing a sample tree imbalance.
    RMTSF_Root = 4,
} rmtSampleFlags;


typedef enum rmtSampleType
{
    RMT_SampleType_CPU,
    RMT_SampleType_CUDA,
    RMT_SampleType_D3D11,
    RMT_SampleType_OpenGL,
    RMT_SampleType_Metal,
    RMT_SampleType_Count,
} rmtSampleType;

// Struct to hold iterator info
typedef struct rmtSampleIterator
{
// public
    rmtSample* sample;
// private
    rmtSample* initial;
} rmtSampleIterator;


/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   Public Interface
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/



// Can call remotery functions on a null pointer
// TODO: Can embed extern "C" in these macros?

#define rmt_Settings()                                                              \
    RMT_OPTIONAL_RET(RMT_ENABLED, _rmt_Settings(), NULL )

#define rmt_CreateGlobalInstance(rmt)                                               \
    RMT_OPTIONAL_RET(RMT_ENABLED, _rmt_CreateGlobalInstance(rmt), RMT_ERROR_NONE)

#define rmt_DestroyGlobalInstance(rmt)                                              \
    RMT_OPTIONAL(RMT_ENABLED, _rmt_DestroyGlobalInstance(rmt))

#define rmt_SetGlobalInstance(rmt)                                                  \
    RMT_OPTIONAL(RMT_ENABLED, _rmt_SetGlobalInstance(rmt))

#define rmt_GetGlobalInstance()                                                     \
    RMT_OPTIONAL_RET(RMT_ENABLED, _rmt_GetGlobalInstance(), NULL)

#define rmt_SetCurrentThreadName(rmt)                                               \
    RMT_OPTIONAL(RMT_ENABLED, _rmt_SetCurrentThreadName(rmt))

#define rmt_LogText(text)                                                           \
    RMT_OPTIONAL(RMT_ENABLED, _rmt_LogText(text))

#define rmt_BeginCPUSample(name, flags)                                             \
    RMT_OPTIONAL(RMT_ENABLED, {                                                     \
        static rmtU32 rmt_sample_hash_##name = 0;                                   \
        _rmt_BeginCPUSample(#name, flags, &rmt_sample_hash_##name);                 \
    })

#define rmt_BeginCPUSampleDynamic(namestr, flags)                                   \
    RMT_OPTIONAL(RMT_ENABLED, _rmt_BeginCPUSample(namestr, flags, NULL))

#define rmt_EndCPUSample()                                                          \
    RMT_OPTIONAL(RMT_ENABLED, _rmt_EndCPUSample())


#define rmt_IterateChildren(iter, sample)                                           \
    RMT_OPTIONAL(RMT_ENABLED, _rmt_IterateChildren(iter, sample))

#define rmt_IterateNext(iter)                                                       \
    RMT_OPTIONAL_RET(RMT_ENABLED, _rmt_IterateNext(iter), RMT_FALSE)


#define rmt_SampleTreeGetThreadName(sample_tree)                                    \
    RMT_OPTIONAL_RET(RMT_ENABLED, _rmt_SampleTreeGetThreadName(sample_tree), NULL)

#define rmt_SampleTreeGetRootSample(sample_tree)                                    \
    RMT_OPTIONAL_RET(RMT_ENABLED, _rmt_SampleTreeGetRootSample(sample_tree), NULL)

// Should only called from within the sample tree callback,
// when the internal string lookup table is valid (i.e. on the main Remotery thread)
#define rmt_SampleGetName(sample)                                                   \
    RMT_OPTIONAL_RET(RMT_ENABLED, _rmt_SampleGetName(sample), NULL)

#define rmt_SampleGetNameHash(sample)                                               \
    RMT_OPTIONAL_RET(RMT_ENABLED, _rmt_SampleGetNameHash(sample), 0U)

#define rmt_SampleGetCallCount(sample)                                              \
    RMT_OPTIONAL_RET(RMT_ENABLED, _rmt_SampleGetCallCount(sample), 0U)

#define rmt_SampleGetStart(sample)                                                  \
    RMT_OPTIONAL_RET(RMT_ENABLED, _rmt_SampleGetStart(sample), 0LLU)

#define rmt_SampleGetTime(sample)                                                   \
    RMT_OPTIONAL_RET(RMT_ENABLED, _rmt_SampleGetTime(sample), 0LLU)

#define rmt_SampleGetSelfTime(sample)                                               \
    RMT_OPTIONAL_RET(RMT_ENABLED, _rmt_SampleGetSelfTime(sample), 0LLU)

#define rmt_SampleGetColour(sample, r, g, b)                                        \
    RMT_OPTIONAL(RMT_ENABLED, _rmt_SampleGetColour(sample, r, g, b))

#define rmt_SampleGetType(sample)                                                   \
    RMT_OPTIONAL_RET(RMT_ENABLED, _rmt_SampleGetType(sample), RMT_SampleType_Count)


// Callback function pointer types
typedef void* (*rmtMallocPtr)(void* mm_context, rmtU32 size);
typedef void* (*rmtReallocPtr)(void* mm_context, void* ptr, rmtU32 size);
typedef void (*rmtFreePtr)(void* mm_context, void* ptr);
typedef void (*rmtInputHandlerPtr)(const char* text, void* context);
typedef void (*rmtSampleTreeHandlerPtr)(void* cbk_context, rmtSampleTree* sample_tree);


// Struture to fill in to modify Remotery default settings
typedef struct rmtSettings
{
    // Which port to listen for incoming connections on
    rmtU16 port;

    // When this server exits it can leave the port open in TIME_WAIT state for a while. This forces
    // subsequent server bind attempts to fail when restarting. If you find restarts fail repeatedly
    // with bind attempts, set this to true to forcibly reuse the open port.
    rmtBool reuse_open_port;

    // Only allow connections on localhost?
    // For dev builds you may want to access your game from other devices but if
    // you distribute a game to your players with Remotery active, probably best
    // to limit connections to localhost.
    rmtBool limit_connections_to_localhost;

    // Whether to enable runtime thread sampling that discovers which processors a thread is running
    // on. This will suspend and resume threads from outside repeatdly and inject code into each
    // thread that automatically instruments the processor.
    // Default: Enabled
    rmtBool enableThreadSampler;

    // How long to sleep between server updates, hopefully trying to give
    // a little CPU back to other threads.
    rmtU32 msSleepBetweenServerUpdates;

    // Size of the internal message queues Remotery uses
    // Will be rounded to page granularity of 64k
    rmtU32 messageQueueSizeInBytes;

    // If the user continuously pushes to the message queue, the server network
    // code won't get a chance to update unless there's an upper-limit on how
    // many messages can be consumed per loop.
    rmtU32 maxNbMessagesPerUpdate;

    // Callback pointers for memory allocation
    rmtMallocPtr malloc;
    rmtReallocPtr realloc;
    rmtFreePtr free;
    void* mm_context;

    // Callback pointer for receiving input from the Remotery console
    rmtInputHandlerPtr input_handler;

    // Callback pointer for traversing the sample tree graph
    rmtSampleTreeHandlerPtr sampletree_handler;
    void* sampletree_context;

    // Context pointer that gets sent to Remotery console callback function
    void* input_handler_context;

    rmtPStr logPath;
} rmtSettings;


// Structure to fill in when binding CUDA to Remotery
typedef struct rmtCUDABind
{
    // The main context that all driver functions apply before each call
    void* context;

    // Driver API function pointers that need to be pointed to
    // Untyped so that the CUDA headers are not required in this file
    // NOTE: These are named differently to the CUDA functions because the CUDA API has a habit of using
    // macros to point function calls to different versions, e.g. cuEventDestroy is a macro for
    // cuEventDestroy_v2.
    void* CtxSetCurrent;
    void* CtxGetCurrent;
    void* EventCreate;
    void* EventDestroy;
    void* EventRecord;
    void* EventQuery;
    void* EventElapsedTime;

} rmtCUDABind;


// Call once after you've initialised CUDA to bind it to Remotery
#define rmt_BindCUDA(bind)                                                  \
    RMT_OPTIONAL(RMT_USE_CUDA, _rmt_BindCUDA(bind))

// Mark the beginning of a CUDA sample on the specified asynchronous stream
#define rmt_BeginCUDASample(name, stream)                                   \
    RMT_OPTIONAL(RMT_USE_CUDA, {                                            \
        static rmtU32 rmt_sample_hash_##name = 0;                           \
        _rmt_BeginCUDASample(#name, &rmt_sample_hash_##name, stream);       \
    })

// Mark the end of a CUDA sample on the specified asynchronous stream
#define rmt_EndCUDASample(stream)                                           \
    RMT_OPTIONAL(RMT_USE_CUDA, _rmt_EndCUDASample(stream))


#define rmt_BindD3D11(device, context)                                      \
    RMT_OPTIONAL(RMT_USE_D3D11, _rmt_BindD3D11(device, context))

#define rmt_UnbindD3D11()                                                   \
    RMT_OPTIONAL(RMT_USE_D3D11, _rmt_UnbindD3D11())

#define rmt_BeginD3D11Sample(name)                                          \
    RMT_OPTIONAL(RMT_USE_D3D11, {                                           \
        static rmtU32 rmt_sample_hash_##name = 0;                           \
        _rmt_BeginD3D11Sample(#name, &rmt_sample_hash_##name);              \
    })

#define rmt_BeginD3D11SampleDynamic(namestr)                                \
    RMT_OPTIONAL(RMT_USE_D3D11, _rmt_BeginD3D11Sample(namestr, NULL))

#define rmt_EndD3D11Sample()                                                \
    RMT_OPTIONAL(RMT_USE_D3D11, _rmt_EndD3D11Sample())


#define rmt_BindOpenGL()                                                    \
    RMT_OPTIONAL(RMT_USE_OPENGL, _rmt_BindOpenGL())

#define rmt_UnbindOpenGL()                                                  \
    RMT_OPTIONAL(RMT_USE_OPENGL, _rmt_UnbindOpenGL())

#define rmt_BeginOpenGLSample(name)                                         \
    RMT_OPTIONAL(RMT_USE_OPENGL, {                                          \
        static rmtU32 rmt_sample_hash_##name = 0;                           \
        _rmt_BeginOpenGLSample(#name, &rmt_sample_hash_##name);             \
    })

#define rmt_BeginOpenGLSampleDynamic(namestr)                               \
    RMT_OPTIONAL(RMT_USE_OPENGL, _rmt_BeginOpenGLSample(namestr, NULL))

#define rmt_EndOpenGLSample()                                               \
    RMT_OPTIONAL(RMT_USE_OPENGL, _rmt_EndOpenGLSample())


#define rmt_BindMetal(command_buffer)                                       \
    RMT_OPTIONAL(RMT_USE_METAL, _rmt_BindMetal(command_buffer));

#define rmt_UnbindMetal()                                                   \
    RMT_OPTIONAL(RMT_USE_METAL, _rmt_UnbindMetal());

#define rmt_BeginMetalSample(name)                                          \
    RMT_OPTIONAL(RMT_USE_METAL, {                                           \
        static rmtU32 rmt_sample_hash_##name = 0;                           \
        _rmt_BeginMetalSample(#name, &rmt_sample_hash_##name);              \
    })

#define rmt_BeginMetalSampleDynamic(namestr)                                \
    RMT_OPTIONAL(RMT_USE_METAL, _rmt_BeginMetalSample(namestr, NULL))

#define rmt_EndMetalSample()                                                \
    RMT_OPTIONAL(RMT_USE_METAL, _rmt_EndMetalSample())




/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   C++ Public Interface Extensions
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/



#ifdef __cplusplus


#if RMT_ENABLED

// Types that end samples in their destructors
extern "C" RMT_API void _rmt_EndCPUSample(void);
struct rmt_EndCPUSampleOnScopeExit
{
    ~rmt_EndCPUSampleOnScopeExit()
    {
        _rmt_EndCPUSample();
    }
};
#if RMT_USE_CUDA
extern "C" RMT_API void _rmt_EndCUDASample(void* stream);
struct rmt_EndCUDASampleOnScopeExit
{
    rmt_EndCUDASampleOnScopeExit(void* stream) : stream(stream)
    {
    }
    ~rmt_EndCUDASampleOnScopeExit()
    {
        _rmt_EndCUDASample(stream);
    }
    void* stream;
};
#endif
#if RMT_USE_D3D11
extern "C" RMT_API void _rmt_EndD3D11Sample(void);
struct rmt_EndD3D11SampleOnScopeExit
{
    ~rmt_EndD3D11SampleOnScopeExit()
    {
        _rmt_EndD3D11Sample();
    }
};
#endif

#if RMT_USE_OPENGL
extern "C" RMT_API void _rmt_EndOpenGLSample(void);
struct rmt_EndOpenGLSampleOnScopeExit
{
    ~rmt_EndOpenGLSampleOnScopeExit()
    {
        _rmt_EndOpenGLSample();
    }
};
#endif

#if RMT_USE_METAL
extern "C" RMT_API void _rmt_EndMetalSample(void);
struct rmt_EndMetalSampleOnScopeExit
{
    ~rmt_EndMetalSampleOnScopeExit()
    {
        _rmt_EndMetalSample();
    }
};
#endif

#endif



// Pairs a call to rmt_Begin<TYPE>Sample with its call to rmt_End<TYPE>Sample when leaving scope
#define rmt_ScopedCPUSample(name, flags)                                                                \
        RMT_OPTIONAL(RMT_ENABLED, rmt_BeginCPUSample(name, flags));                                     \
        RMT_OPTIONAL(RMT_ENABLED, rmt_EndCPUSampleOnScopeExit rmt_ScopedCPUSample##name);
#define rmt_ScopedCUDASample(name, stream)                                                              \
        RMT_OPTIONAL(RMT_USE_CUDA, rmt_BeginCUDASample(name, stream));                                  \
        RMT_OPTIONAL(RMT_USE_CUDA, rmt_EndCUDASampleOnScopeExit rmt_ScopedCUDASample##name(stream));
#define rmt_ScopedD3D11Sample(name)                                                                     \
        RMT_OPTIONAL(RMT_USE_D3D11, rmt_BeginD3D11Sample(name));                                        \
        RMT_OPTIONAL(RMT_USE_D3D11, rmt_EndD3D11SampleOnScopeExit rmt_ScopedD3D11Sample##name);
#define rmt_ScopedOpenGLSample(name)                                                                    \
        RMT_OPTIONAL(RMT_USE_OPENGL, rmt_BeginOpenGLSample(name));                                      \
        RMT_OPTIONAL(RMT_USE_OPENGL, rmt_EndOpenGLSampleOnScopeExit rmt_ScopedOpenGLSample##name);
#define rmt_ScopedMetalSample(name)                                                                     \
        RMT_OPTIONAL(RMT_USE_METAL, rmt_BeginMetalSample(name));                                        \
        RMT_OPTIONAL(RMT_USE_METAL, rmt_EndMetalSampleOnScopeExit rmt_ScopedMetalSample##name);

#endif



/*
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
   Private Interface - don't directly call these
------------------------------------------------------------------------------------------------------------------------
------------------------------------------------------------------------------------------------------------------------
*/



#if RMT_ENABLED

#ifdef __cplusplus
extern "C" {
#endif

RMT_API rmtSettings* _rmt_Settings( void );
RMT_API enum rmtError _rmt_CreateGlobalInstance(Remotery** remotery);
RMT_API void _rmt_DestroyGlobalInstance(Remotery* remotery);
RMT_API void _rmt_SetGlobalInstance(Remotery* remotery);
RMT_API Remotery* _rmt_GetGlobalInstance(void);
RMT_API void _rmt_SetCurrentThreadName(rmtPStr thread_name);
RMT_API void _rmt_LogText(rmtPStr text);
RMT_API void _rmt_BeginCPUSample(rmtPStr name, rmtU32 flags, rmtU32* hash_cache);
RMT_API void _rmt_EndCPUSample(void);

#if RMT_USE_CUDA
RMT_API void _rmt_BindCUDA(const rmtCUDABind* bind);
RMT_API void _rmt_BeginCUDASample(rmtPStr name, rmtU32* hash_cache, void* stream);
RMT_API void _rmt_EndCUDASample(void* stream);
#endif

#if RMT_USE_D3D11
RMT_API void _rmt_BindD3D11(void* device, void* context);
RMT_API void _rmt_UnbindD3D11(void);
RMT_API void _rmt_BeginD3D11Sample(rmtPStr name, rmtU32* hash_cache);
RMT_API void _rmt_EndD3D11Sample(void);
#endif

#if RMT_USE_OPENGL
RMT_API void _rmt_BindOpenGL();
RMT_API void _rmt_UnbindOpenGL(void);
RMT_API void _rmt_BeginOpenGLSample(rmtPStr name, rmtU32* hash_cache);
RMT_API void _rmt_EndOpenGLSample(void);
#endif

#if RMT_USE_METAL
RMT_API void _rmt_BeginMetalSample(rmtPStr name, rmtU32* hash_cache);
RMT_API void _rmt_EndMetalSample(void);
#endif

// Iterator
RMT_API void                _rmt_IterateChildren(rmtSampleIterator* iter, rmtSample* sample);
RMT_API rmtBool             _rmt_IterateNext(rmtSampleIterator* iter);

// SampleTree accessors
RMT_API const char*         _rmt_SampleTreeGetThreadName(rmtSampleTree* sample_tree);
RMT_API rmtSample*          _rmt_SampleTreeGetRootSample(rmtSampleTree* sample_tree);

// Sample accessors
RMT_API const char*         _rmt_SampleGetName(rmtSample* sample);
RMT_API rmtU32              _rmt_SampleGetNameHash(rmtSample* sample);
RMT_API rmtU32              _rmt_SampleGetCallCount(rmtSample* sample);
RMT_API rmtU64              _rmt_SampleGetStart(rmtSample* sample);
RMT_API rmtU64              _rmt_SampleGetTime(rmtSample* sample);
RMT_API rmtU64              _rmt_SampleGetSelfTime(rmtSample* sample);
RMT_API void                _rmt_SampleGetColour(rmtSample* sample, rmtU8* r, rmtU8* g, rmtU8* b);
RMT_API rmtSampleType       _rmt_SampleGetType(rmtSample* sample);

#ifdef __cplusplus

}
#endif

#if RMT_USE_METAL
#ifdef __OBJC__
RMT_API void _rmt_BindMetal(id command_buffer);
RMT_API void _rmt_UnbindMetal();
#endif
#endif

#endif  // RMT_ENABLED


#endif
