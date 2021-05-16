#ifndef BIKESHED_INCLUDEGUARD_PRIVATE_H
#define BIKESHED_INCLUDEGUARD_PRIVATE_H

/*
bikeshed.h - public domain - Dan Engelbrecht @DanEngelbrecht, 2019

# BIKESHED

Lock free hierarchical work scheduler, builds with MSVC, Clang and GCC, header only, C99 compliant, MIT license.

See github for latest version: https://github.com/DanEngelbrecht/bikeshed

See design blogs at: https:/danengelbrecht.github.io

## Version history

### Version v0.3 1/5 2019

**Pre-release 3**

#### Fixes

- Ready callback is now called when a task is readied via dependency resolve
- Tasks are readied in batch when possible

### Version v0.2 29/4 2019

**Pre-release 2**

#### Fixes

- Internal cleanups
- Fixed warnings and removed clang warning suppressions
  - `-Wno-sign-conversion`
  - `-Wno-unused-macros`
  - `-Wno-c++98-compat`
  - `-Wno-implicit-fallthrough`
- Made it compile cleanly with clang++ on Windows

### Version v0.1 26/4 2019

**Pre-release 1**

## Usage
In *ONE* source file, put:

```C
#define BIKESHED_IMPLEMENTATION
// Define any overrides of platform specific implementations before including bikeshed.h.
#include "bikeshed.h"
```

Other source files should just #include "bikeshed.h"

## Macros

BIKESHED_IMPLEMENTATION
Define in one compilation unit to include implementation.

BIKESHED_ASSERTS
Define if you want Bikeshed to validate API usage.

BIKESHED_ATOMICADD
Macro for platform specific implementation of an atomic addition. Returns the result of the operation.
#define BIKESHED_ATOMICADD(value, amount) return MyAtomicAdd(value, amount)

BIKESHED_ATOMICCAS
Macro for platform specific implementation of an atomic compare and swap. Returns the previous value of "store".
#define BIKESHED_ATOMICCAS(store, compare, value) return MyAtomicCAS(store, compare, value);

## Notes
Macros, typedefs, structs and functions suffixed with "_private" are internal and subject to change.

## Dependencies
Standard C library headers:
#include <stdint.h>
#include <string.h>

If either BIKESHED_ATOMICADD or BIKESHED_ATOMICCAS is not defined the MSVC/Windows implementation will
include <Windows.h> to get access to _InterlockedExchangeAdd and _InterlockedCompareExchange respectively.

The GCC/clang implementation relies on the compiler intrisics __sync_fetch_and_add and __sync_val_compare_and_swap.

## License

MIT License

Copyright (c) 2019 Dan Engelbrecht

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.
*/

#include <stdint.h>
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif // __cplusplus

//  ------------- Public API Begin

// Custom assert hook
// If BIKESHED_ASSERTS is defined, Bikeshed will validate input parameters to make sure
// correct API usage.
// The callback will be called if an API error is detected and the library function will bail.
//
// Warning: If an API function asserts the state of the Bikeshed might be compromised.
// To reset the assert callback call Bikeshed_SetAssert(0);
typedef void (*Bikeshed_Assert)(const char* expression, const char* file, int line);
void Bikeshed_SetAssert(Bikeshed_Assert assert_func);

// Bikeshed handle
typedef struct Bikeshed_Shed_private* Bikeshed;

// Callback instance and hook for the library user to implement.
// Called when one or more tasks are "readied" - either by a call to Bikeshed_ReadyTasks or if
// a task has been resolved due to all dependencies have finished.
//
// struct MyReadyCallback {
//      // Add the Bikeshed_ReadyCallback struct *first* in your struct
//      struct Bikeshed_ReadyCallback cb = {MyReadyCallback::Ready};
//      // Add any custom data here
//      static void Ready(struct Bikeshed_ReadyCallback* ready_callback, uint32_t ready_count)
//      {
//          MyReadyCallback* _this = (MyReadyCallback*)ready_callback;
//          // Your code goes here
//      }
// };
// MyReadyCallback myCallback;
// bikeshed = CreateBikeshed(mem, max_task_count, max_dependency_count, channel_count, &myCallback.cb);
//
// The Bikeshed_ReadyCallback instance must have a lifetime that starts before and ends after the Bikeshed instance.
struct Bikeshed_ReadyCallback
{
    void (*SignalReady)(struct Bikeshed_ReadyCallback* ready_callback, uint32_t ready_count);
};

// Task identifier
typedef uint32_t Bikeshed_TaskID;

// Result codes for task execution
enum Bikeshed_TaskResult
{
    BIKESHED_TASK_RESULT_COMPLETE, // Task is complete, dependecies will be resolved and the task is freed
    BIKESHED_TASK_RESULT_BLOCKED   // Task is blocked, call ReadyTasks on the task id when ready to execute again
};

// Task execution callback
// A task execution function may:
//  - Create new tasks
//  - Add new dependencies to tasks that are not "Ready" or "Executing"
//  - Ready other tasks that are not "Ready" or "Executing"
//  - Deallocate any memory associated with the context
//
// A task execution function may not:
//  - Add dependencies to the executing task
//  - Ready the executing task
//
// A task execution function should not:
//  - Block, if the function blocks the thread that called Bikeshed_ExecuteOne will block, return BIKESHED_TASK_RESULT_BLOCKED instead
//  - Call Bikeshed_ExecuteOne, this works but makes little sense
//  - Destroy the Bikeshed instance
typedef enum Bikeshed_TaskResult (*BikeShed_TaskFunc)(Bikeshed shed, Bikeshed_TaskID task_id, uint8_t channel, void* context);

// Calculates the memory needed for a Bikeshed instance
// BIKESHED_SIZE is a macro which allows the Bikeshed to be allocated on the stack without heap allocations
//
// max_task_count: 1 to 8 388 607 tasks
// max_dependency_count: 0 to 8 388 607 dependencies
// channel_count: 1 to 255 channels
#define BIKESHED_SIZE(max_task_count, max_dependency_count, channel_count)

// Create a Bikeshed at the provided memory location
// Use BIKESHED_SIZE to get the required size of the memory block
// max_task_count, max_dependency_count and channel_count must match call to BIKESHED_SIZE
//
// ready_callback is optional and may be 0
//
// The returned Bikeshed is a typed pointer that points to same address as mem
Bikeshed Bikeshed_Create(void* mem, uint32_t max_task_count, uint32_t max_dependency_count, uint8_t channel_count, struct Bikeshed_ReadyCallback* ready_callback);

// Clones the state of a Bikeshed
// Use BIKESHED_SIZE to get the required size of the memory block
// max_task_count, max_dependency_count and channel_count must match call to BIKESHED_SIZE
//
// Makes a complete copy of the current state of a Bikeshed, executing on the clone copy will not affect the
// bikeshed state of the original.
//
// The returned Bikeshed is a typed pointer that points to same address as mem
Bikeshed Bikeshed_CloneState(void* mem, Bikeshed original, uint32_t shed_size);

// Creates one or more tasks
// Reserves task_count number of tasks from the internal Bikeshed state
// Caller is resposible for making sure the context pointers are valid until the corresponding task is executed
// Tasks will not be executed until they are readied with Bikeshed_ReadyTasks or if it has dependencies that have all been resolved
// Returns:
//  1 - Success
//  0 - Not enough free task instances in the bikeshed
int Bikeshed_CreateTasks(Bikeshed shed, uint32_t task_count, BikeShed_TaskFunc* task_functions, void** contexts, Bikeshed_TaskID* out_task_ids);

// Set the channel of one or more tasks
// The default channel for a task is 0.
//
// The channel is used when calling Bikeshed_ExecuteOne
void Bikeshed_SetTasksChannel(Bikeshed shed, uint32_t task_count, Bikeshed_TaskID* task_ids, uint8_t channel);

// Readies one or more tasks for execution
// Tasks must not have any unresolved dependencies
// Task execution order is not guarranteed - use dependecies to eforce task execution order
// The Bikeshed_ReadyCallback of the shed (if any is set) will be called with task_count as number of readied tasks
void Bikeshed_ReadyTasks(Bikeshed shed, uint32_t task_count, const Bikeshed_TaskID* task_ids);

// Add dependencies to a task
// A task can have zero or more dependencies
// If a task has been made ready with Bikeshed_ReadyTasks you may not add dependencies to the task.
// A task that has been made ready may not be added as a dependency to another task.
// A task may have multiple "parents" - it is valid to add the same task as child to more than one task.
// Creating a task hierarchy that has circular dependencies makes it impossible to resolve.
//
// Returns:
//  1 - Success
//  0 - Not enough free dependency instances in the bikeshed
int Bikeshed_AddDependencies(Bikeshed shed, Bikeshed_TaskID task_id, uint32_t task_count, const Bikeshed_TaskID* dependency_task_ids);

// Execute one task
// Checks the ready queue of channel and executes the task callback
// Any parent dependencies are resolved and if any parent gets all its dependencies resolved they will be made ready for execution.
//
// Returns:
//  1 - Executed one task
//  2 - No task are ready for execution
int Bikeshed_ExecuteOne(Bikeshed shed, uint8_t channel);

//  ------------- Public API End


typedef uint32_t Bikeshed_TaskIndex_private;
typedef uint32_t Bikeshed_DependencyIndex_private;
typedef uint32_t Bikeshed_ReadyIndex_private;

struct Bikeshed_Dependency_private
{
    Bikeshed_TaskIndex_private       m_ParentTaskIndex;
    Bikeshed_DependencyIndex_private m_NextDependencyIndex;
};

struct Bikeshed_Task_private
{
    long volatile                       m_ChildDependencyCount;
    Bikeshed_TaskID                     m_TaskID;
    uint32_t                            m_Channel  : 8;
    uint32_t                            m_FirstDependencyIndex : 24;
    BikeShed_TaskFunc                   m_TaskFunc;
    void*                               m_TaskContext;
};

struct Bikeshed_Shed_private
{
    struct Bikeshed_Task_private*       m_Tasks;
    struct Bikeshed_Dependency_private* m_Dependencies;
    struct Bikeshed_ReadyCallback*      m_ReadyCallback;
    long*                               m_TaskIndexes;
    long*                               m_DependencyIndexes;
    long volatile*                      m_ReadyHeads;
    long*                               m_ReadyIndexes;
    long volatile                       m_TaskIndexHead;
    long volatile                       m_DependencyIndexHead;
    long volatile                       m_TaskGeneration;
    long volatile                       m_TaskIndexGeneration;
    long volatile                       m_DependencyIndexGeneration;
    long volatile                       m_ReadyGeneration;
};

#define BIKESHED_ALIGN_SIZE_PRIVATE(x, align) (((x) + ((align)-1)) & ~((align)-1))

#undef BIKESHED_SIZE
#define BIKESHED_SIZE(max_task_count, max_dependency_count, channel_count) \
        ((uint32_t)BIKESHED_ALIGN_SIZE_PRIVATE((uint32_t)sizeof(struct Bikeshed_Shed_private), 8u) + \
        (uint32_t)BIKESHED_ALIGN_SIZE_PRIVATE((uint32_t)(sizeof(struct Bikeshed_Task_private) * (max_task_count)), 8u) + \
        (uint32_t)BIKESHED_ALIGN_SIZE_PRIVATE((uint32_t)(sizeof(struct Bikeshed_Dependency_private) * (max_dependency_count)), 4u) + \
        (uint32_t)BIKESHED_ALIGN_SIZE_PRIVATE((uint32_t)(sizeof(long volatile) * (max_task_count)), 4u) + \
        (uint32_t)BIKESHED_ALIGN_SIZE_PRIVATE((uint32_t)(sizeof(long volatile) * (max_dependency_count)), 4u) + \
        (uint32_t)BIKESHED_ALIGN_SIZE_PRIVATE((uint32_t)(sizeof(long volatile) * (channel_count)), 4u) + \
        (uint32_t)BIKESHED_ALIGN_SIZE_PRIVATE((uint32_t)(sizeof(long volatile) * (max_task_count)), 4u))

#if defined(BIKESHED_IMPLEMENTATION)

#if !defined(BIKESHED_ATOMICADD)
    #if defined(__clang__) || defined(__GNUC__)
        #define BIKESHED_ATOMICADD(value, amount) (__sync_add_and_fetch (value, amount))
    #elif defined(_MSC_VER)
        #if !defined(_WINDOWS_)
            #define WIN32_LEAN_AND_MEAN
            #include <Windows.h>
            #undef WIN32_LEAN_AND_MEAN
        #endif

        #define BIKESHED_ATOMICADD(value, amount) (_InterlockedExchangeAdd(value, amount) + amount)
    #endif
#endif

#if !defined(BIKESHED_ATOMICCAS)
    #if defined(__clang__) || defined(__GNUC__)
        #define BIKESHED_ATOMICCAS(store, compare, value) __sync_val_compare_and_swap(store, compare, value)
    #elif defined(_MSC_VER)
        #if !defined(_WINDOWS_)
            #define WIN32_LEAN_AND_MEAN
            #include <Windows.h>
            #undef WIN32_LEAN_AND_MEAN
        #endif

        #define BIKESHED_ATOMICCAS(store, compare, value) _InterlockedCompareExchange(store, value, compare)
    #endif
#endif

#if defined(BIKESHED_ASSERTS)
#    define BIKESHED_FATAL_ASSERT_PRIVATE(x, bail) \
        if (!(x)) \
        { \
            if (Bikeshed_Assert_private) \
            { \
                Bikeshed_Assert_private(#x, __FILE__, __LINE__); \
            } \
            bail; \
        }
#else // defined(BIKESHED_ASSERTS)
#    define BIKESHED_FATAL_ASSERT_PRIVATE(x, y)
#endif // defined(BIKESHED_ASSERTS)

#if defined(BIKESHED_ASSERTS)
static Bikeshed_Assert Bikeshed_Assert_private = 0;
#endif // defined(BIKESHED_ASSERTS)

void Bikeshed_SetAssert(Bikeshed_Assert assert_func)
{
#if defined(BIKESHED_ASSERTS)
    Bikeshed_Assert_private = assert_func;
#else  // defined(BIKESHED_ASSERTS)
    assert_func = 0;
#endif // defined(BIKESHED_ASSERTS)
}

static const uint32_t BIKSHED_GENERATION_SHIFT_PRIVATE = 23u;
static const uint32_t BIKSHED_INDEX_MASK_PRIVATE       = 0x007fffffu;
static const uint32_t BIKSHED_GENERATION_MASK_PRIVATE  = 0xff800000u;

#define BIKESHED_TASK_ID_PRIVATE(index, generation) (((Bikeshed_TaskID)(generation) << BIKSHED_GENERATION_SHIFT_PRIVATE) + index)
#define BIKESHED_TASK_GENERATION_PRIVATE(task_id) ((long)(task_id >> BIKSHED_GENERATION_SHIFT_PRIVATE))
#define BIKESHED_TASK_INDEX_PRIVATE(task_id) ((Bikeshed_TaskIndex_private)(task_id & BIKSHED_INDEX_MASK_PRIVATE))

inline void Bikeshed_PoolPush_private(long volatile* generation, long volatile* head, long volatile* items, uint32_t index)
{
    uint32_t gen = (((uint32_t)BIKESHED_ATOMICADD(generation, 1)) << BIKSHED_GENERATION_SHIFT_PRIVATE) & BIKSHED_GENERATION_MASK_PRIVATE;
    uint32_t new_head = gen | index;

    uint32_t current_head   = (uint32_t)*head;
    items[index-1]          = (long)(BIKESHED_TASK_INDEX_PRIVATE(current_head));

    while (BIKESHED_ATOMICCAS(head, (long)current_head, (long)new_head) != (long)current_head)
    {
        current_head    = (uint32_t)*head;
        items[index-1]  = (long)(BIKESHED_TASK_INDEX_PRIVATE(current_head));
    }
}

inline uint32_t Bikeshed_PoolPop_private(long volatile* head, long volatile* items)
{
    do
    {
        uint32_t current_head   = (uint32_t)*head;
        uint32_t head_index     = BIKESHED_TASK_INDEX_PRIVATE(current_head);
        if (head_index == 0)
        {
            return 0;
        }

        uint32_t next       = (uint32_t)items[head_index - 1];
        uint32_t new_head   = (current_head & BIKSHED_GENERATION_MASK_PRIVATE) | next;

        if (BIKESHED_ATOMICCAS(head, (long)current_head, (long)new_head) == (long)current_head)
        {
            return head_index;
        }
    } while(1);
}

static void Bikeshed_PoolInitialize_private(long volatile* generation, long volatile* head, long volatile* items, uint32_t fill_count)
{
    *generation = 0;
    if (fill_count == 0)
    {
        *head = 0;
        return;
    }
    *head = 1;
    for (uint32_t i = 0; i < fill_count - 1; ++i)
    {
        items[i] = (long)(i + 2);
    }
    items[fill_count - 1] = 0;
}

static void Bikeshed_FreeTask_private(Bikeshed shed, Bikeshed_TaskID task_id)
{
    Bikeshed_TaskIndex_private task_index  = BIKESHED_TASK_INDEX_PRIVATE(task_id);
    struct Bikeshed_Task_private* task     = &shed->m_Tasks[task_index - 1];
    BIKESHED_FATAL_ASSERT_PRIVATE(task_id == task->m_TaskID, return )
    BIKESHED_FATAL_ASSERT_PRIVATE(0 == task->m_ChildDependencyCount, return )

    task->m_TaskID                                    = 0;
    Bikeshed_DependencyIndex_private dependency_index = task->m_FirstDependencyIndex;
    while (dependency_index != 0)
    {
        struct Bikeshed_Dependency_private* dependency         = &shed->m_Dependencies[dependency_index - 1];
        Bikeshed_DependencyIndex_private next_dependency_index = dependency->m_NextDependencyIndex;
        Bikeshed_PoolPush_private(&shed->m_DependencyIndexGeneration, &shed->m_DependencyIndexHead, shed->m_DependencyIndexes, dependency_index);
        dependency_index    = next_dependency_index;
    }
    Bikeshed_PoolPush_private(&shed->m_TaskIndexGeneration, &shed->m_TaskIndexHead, shed->m_TaskIndexes, task_index);
}

static void Bikeshed_ReadyRange_private(Bikeshed shed, uint32_t gen, Bikeshed_TaskID head_task_id, Bikeshed_TaskIndex_private tail_task_index, uint8_t channel)
{
    Bikeshed_TaskIndex_private index            = BIKESHED_TASK_INDEX_PRIVATE(head_task_id);
    uint32_t new_head                           = gen | index;
    long volatile* head                         = &shed->m_ReadyHeads[channel];
    uint32_t current_head                       = (uint32_t)*head;
    shed->m_ReadyIndexes[tail_task_index-1]     = (long)(BIKESHED_TASK_INDEX_PRIVATE(current_head));

    while (BIKESHED_ATOMICCAS(head, (long)current_head, (long)new_head) != (long)current_head)
    {
        current_head                            = (uint32_t)*head;
        shed->m_ReadyIndexes[tail_task_index-1] = (long)(BIKESHED_TASK_INDEX_PRIVATE(current_head));
    }
}

static void Bikeshed_ResolveTask_private(Bikeshed shed, Bikeshed_TaskID task_id)
{
    Bikeshed_TaskIndex_private task_index               = BIKESHED_TASK_INDEX_PRIVATE(task_id);
    struct Bikeshed_Task_private* task                  = &shed->m_Tasks[task_index - 1];
    BIKESHED_FATAL_ASSERT_PRIVATE(task_id == task->m_TaskID, return )
    Bikeshed_DependencyIndex_private dependency_index   = task->m_FirstDependencyIndex;

    Bikeshed_TaskID             head_resolved_task_id = 0;
    Bikeshed_TaskIndex_private  tail_resolved_task_index = 0;
    uint32_t                    resolved_task_count = 0;
    uint8_t                     channel = 0;

    while (dependency_index != 0)
    {
        struct Bikeshed_Dependency_private* dependency  = &shed->m_Dependencies[dependency_index - 1];
        Bikeshed_TaskIndex_private  parent_task_index   = dependency->m_ParentTaskIndex;
        struct Bikeshed_Task_private* parent_task       = &shed->m_Tasks[parent_task_index - 1];
        long child_dependency_count                     = BIKESHED_ATOMICADD(&parent_task->m_ChildDependencyCount, -1);
        if (child_dependency_count == 0)
        {
            BIKESHED_FATAL_ASSERT_PRIVATE(0x20000000 == BIKESHED_ATOMICADD(&shed->m_Tasks[parent_task_index - 1].m_ChildDependencyCount, 0x20000000), return)
            if (resolved_task_count == 0)
            {
                head_resolved_task_id       = parent_task->m_TaskID;
                channel                     = (uint8_t)parent_task->m_Channel;
            }
            else if (channel == parent_task->m_Channel)
            {
                shed->m_ReadyIndexes[tail_resolved_task_index - 1]   = parent_task_index;
            }
            else
            {
                uint32_t gen = (((uint32_t)BIKESHED_ATOMICADD(&shed->m_ReadyGeneration, 1)) << BIKSHED_GENERATION_SHIFT_PRIVATE) & BIKSHED_GENERATION_MASK_PRIVATE;
                Bikeshed_ReadyRange_private(shed, gen, head_resolved_task_id, tail_resolved_task_index, channel);
                head_resolved_task_id       = parent_task->m_TaskID;
                channel                     = (uint8_t)parent_task->m_Channel;
            }
            tail_resolved_task_index    = parent_task_index;
            ++resolved_task_count;
        }
        dependency_index = dependency->m_NextDependencyIndex;
    }

    if (resolved_task_count > 0)
    {
        uint32_t gen = (((uint32_t)BIKESHED_ATOMICADD(&shed->m_ReadyGeneration, 1)) << BIKSHED_GENERATION_SHIFT_PRIVATE) & BIKSHED_GENERATION_MASK_PRIVATE;
        Bikeshed_ReadyRange_private(shed, gen, head_resolved_task_id, tail_resolved_task_index, channel);
        if (shed->m_ReadyCallback)
        {
            shed->m_ReadyCallback->SignalReady(shed->m_ReadyCallback, resolved_task_count);
        }
    }
}

Bikeshed Bikeshed_Create(void* mem, uint32_t max_task_count, uint32_t max_dependency_count, uint8_t channel_count, struct Bikeshed_ReadyCallback* sync_primitive)
{
    BIKESHED_FATAL_ASSERT_PRIVATE(max_task_count > 0, return 0)
    BIKESHED_FATAL_ASSERT_PRIVATE(max_task_count == BIKESHED_TASK_INDEX_PRIVATE(max_task_count), return 0)
    BIKESHED_FATAL_ASSERT_PRIVATE(max_dependency_count == BIKESHED_TASK_INDEX_PRIVATE(max_dependency_count), return 0)
    BIKESHED_FATAL_ASSERT_PRIVATE(channel_count >= 1, return 0)

    Bikeshed shed                = (Bikeshed)mem;
    shed->m_TaskGeneration       = 1;
    shed->m_ReadyCallback        = sync_primitive;
    uint8_t* p                   = (uint8_t*)mem;
    p                           += BIKESHED_ALIGN_SIZE_PRIVATE((uint32_t)sizeof(struct Bikeshed_Shed_private), 8u);
    shed->m_Tasks                = (struct Bikeshed_Task_private*)((void*)p);
    p                           += BIKESHED_ALIGN_SIZE_PRIVATE((uint32_t)(sizeof(struct Bikeshed_Task_private) * max_task_count), 8u);
    shed->m_Dependencies         = (struct Bikeshed_Dependency_private*)((void*)p);
    p                           += BIKESHED_ALIGN_SIZE_PRIVATE((uint32_t)(sizeof(struct Bikeshed_Dependency_private) * max_dependency_count), 4u);
    shed->m_TaskIndexes          = (long*)(void*)p;
    p                           += BIKESHED_ALIGN_SIZE_PRIVATE((uint32_t)(sizeof(long) * max_task_count), 4u);
    shed->m_DependencyIndexes    = (long*)(void*)p;
    p                           += BIKESHED_ALIGN_SIZE_PRIVATE((uint32_t)(sizeof(long) * max_dependency_count), 4u);
    shed->m_ReadyHeads           = (long volatile*)(void*)p;
    p                           += BIKESHED_ALIGN_SIZE_PRIVATE((uint32_t)(sizeof(long volatile) * channel_count), 4u);
    shed->m_ReadyIndexes         = (long*)(void*)p;
    p                           += BIKESHED_ALIGN_SIZE_PRIVATE((uint32_t)(sizeof(long) * max_task_count), 4u);
    BIKESHED_FATAL_ASSERT_PRIVATE(p == &((uint8_t*)mem)[BIKESHED_SIZE(max_task_count, max_dependency_count, channel_count)], return 0)

    Bikeshed_PoolInitialize_private(&shed->m_TaskIndexGeneration, &shed->m_TaskIndexHead, shed->m_TaskIndexes, max_task_count);
    Bikeshed_PoolInitialize_private(&shed->m_DependencyIndexGeneration, &shed->m_DependencyIndexHead, shed->m_DependencyIndexes, max_dependency_count);
    for (uint8_t channel = 0; channel < channel_count; ++channel)
    {
        shed->m_ReadyHeads[channel] = 0;
    }

    return shed;
}

Bikeshed Bikeshed_CloneState(void* mem, Bikeshed original, uint32_t shed_size)
{
    memcpy(mem, original, shed_size);

    Bikeshed shed               = (Bikeshed)mem;
    uint8_t* p                  = (uint8_t*)mem;
    shed->m_Tasks               = (struct Bikeshed_Task_private*)(void*)(&p[(uintptr_t)original->m_Tasks - (uintptr_t)original]);
    shed->m_Dependencies        = (struct Bikeshed_Dependency_private*)(void*)(&p[(uintptr_t)original->m_Dependencies - (uintptr_t)original]);
    shed->m_TaskIndexes         = (long*)(void*)(&p[(uintptr_t)original->m_TaskIndexes - (uintptr_t)original]);
    shed->m_DependencyIndexes   = (long*)(void*)(&p[(uintptr_t)original->m_DependencyIndexes - (uintptr_t)original]);
    shed->m_ReadyHeads          = (long volatile*)(void*)(&p[(uintptr_t)original->m_ReadyHeads - (uintptr_t)original]);
    shed->m_ReadyIndexes        = (long*)(void*)(&p[(uintptr_t)original->m_ReadyIndexes - (uintptr_t)original]);

    return shed;
}

int Bikeshed_CreateTasks(Bikeshed shed, uint32_t task_count, BikeShed_TaskFunc* task_functions, void** contexts, Bikeshed_TaskID* out_task_ids)
{
    BIKESHED_FATAL_ASSERT_PRIVATE(task_count > 0, return 0)
    long generation = BIKESHED_ATOMICADD(&shed->m_TaskGeneration, 1);
    for (uint32_t i = 0; i < task_count; ++i)
    {
        BIKESHED_FATAL_ASSERT_PRIVATE(task_functions[i] != 0, return 0)
        Bikeshed_TaskIndex_private task_index = (Bikeshed_TaskIndex_private)Bikeshed_PoolPop_private(&shed->m_TaskIndexHead, shed->m_TaskIndexes);
        if (task_index == 0)
        {
            while (i > 0)
            {
                --i;
                Bikeshed_PoolPush_private(&shed->m_TaskIndexGeneration, &shed->m_TaskIndexHead, shed->m_TaskIndexes, BIKESHED_TASK_INDEX_PRIVATE(out_task_ids[i]));
                out_task_ids[i] = 0;
            }
            return 0;
        }
        Bikeshed_TaskID task_id            = BIKESHED_TASK_ID_PRIVATE(task_index, generation);
        out_task_ids[i]                    = task_id;
        struct Bikeshed_Task_private* task = &shed->m_Tasks[task_index - 1];
        task->m_TaskID                     = task_id;
        task->m_ChildDependencyCount       = 0;
        task->m_FirstDependencyIndex       = 0;
        task->m_Channel                    = 0;
        task->m_TaskFunc                   = task_functions[i];
        task->m_TaskContext                = contexts[i];
    }
    return 1;
}

void Bikeshed_SetTasksChannel(Bikeshed shed, uint32_t task_count, Bikeshed_TaskID* task_ids, uint8_t channel)
{
    for (uint32_t t = 0; t < task_count; ++t)
    {
        Bikeshed_TaskIndex_private task_index   = BIKESHED_TASK_INDEX_PRIVATE(task_ids[t]);
        struct Bikeshed_Task_private* task      = &shed->m_Tasks[task_index - 1];
        BIKESHED_FATAL_ASSERT_PRIVATE(task_ids[t] == task->m_TaskID, return)
        BIKESHED_FATAL_ASSERT_PRIVATE(task->m_ChildDependencyCount < 0x20000000, return)
        BIKESHED_FATAL_ASSERT_PRIVATE(&shed->m_ReadyHeads[channel] < shed->m_ReadyIndexes, return)
        task->m_Channel  = channel;
    }
}

int Bikeshed_AddDependencies(Bikeshed shed, Bikeshed_TaskID task_id, uint32_t task_count, const Bikeshed_TaskID* dependency_task_ids)
{
    Bikeshed_TaskIndex_private task_index   = BIKESHED_TASK_INDEX_PRIVATE(task_id);
    struct Bikeshed_Task_private* task      = &shed->m_Tasks[task_index - 1];
    BIKESHED_FATAL_ASSERT_PRIVATE(task_id == task->m_TaskID, return 0)
    BIKESHED_FATAL_ASSERT_PRIVATE(task->m_ChildDependencyCount < 0x20000000, return 0)

    for (uint32_t i = 0; i < task_count; ++i)
    {
        Bikeshed_TaskID dependency_task_id                  = dependency_task_ids[i];
        Bikeshed_TaskIndex_private dependency_task_index    = BIKESHED_TASK_INDEX_PRIVATE(dependency_task_id);
        struct Bikeshed_Task_private* dependency_task       = &shed->m_Tasks[dependency_task_index - 1];
        BIKESHED_FATAL_ASSERT_PRIVATE(dependency_task_id == dependency_task->m_TaskID, return 0)
        Bikeshed_DependencyIndex_private dependency_index   = (Bikeshed_DependencyIndex_private)Bikeshed_PoolPop_private(&shed->m_DependencyIndexHead, shed->m_DependencyIndexes);
        if (dependency_index == 0)
        {
            while (i > 0)
            {
                --i;

                dependency_task_id                              = dependency_task_ids[i];
                dependency_task_index                           = BIKESHED_TASK_INDEX_PRIVATE(dependency_task_id);
                dependency_task                                 = &shed->m_Tasks[dependency_task_index - 1];
                dependency_index                                = dependency_task->m_FirstDependencyIndex;
                struct Bikeshed_Dependency_private* dependency  = &shed->m_Dependencies[dependency_index - 1];
                dependency_task->m_FirstDependencyIndex         = dependency->m_NextDependencyIndex;
                Bikeshed_PoolPush_private(&shed->m_DependencyIndexGeneration, &shed->m_DependencyIndexHead, shed->m_DependencyIndexes, dependency_index);
            }
            return 0;
        }
        struct Bikeshed_Dependency_private* dependency  = &shed->m_Dependencies[dependency_index - 1];
        dependency->m_ParentTaskIndex                   = task_index;
        dependency->m_NextDependencyIndex               = dependency_task->m_FirstDependencyIndex;
        dependency_task->m_FirstDependencyIndex         = dependency_index;
    }
    BIKESHED_ATOMICADD(&task->m_ChildDependencyCount, (long)task_count);

    return 1;
}

void Bikeshed_ReadyTasks(Bikeshed shed, uint32_t task_count, const Bikeshed_TaskID* task_ids)
{
    BIKESHED_FATAL_ASSERT_PRIVATE(task_count > 0, return)
    Bikeshed_TaskID head_task_id                = task_ids[0];
    Bikeshed_TaskIndex_private tail_task_index  = BIKESHED_TASK_INDEX_PRIVATE(head_task_id);
    struct Bikeshed_Task_private* prev_task     = &shed->m_Tasks[tail_task_index - 1];
    BIKESHED_FATAL_ASSERT_PRIVATE(head_task_id == prev_task->m_TaskID, return )
    BIKESHED_FATAL_ASSERT_PRIVATE(0x20000000 == BIKESHED_ATOMICADD(&shed->m_Tasks[tail_task_index - 1].m_ChildDependencyCount, 0x20000000), return)

    uint8_t channel = (uint8_t)prev_task->m_Channel;
    uint32_t gen = (((uint32_t)BIKESHED_ATOMICADD(&shed->m_ReadyGeneration, 1)) << BIKSHED_GENERATION_SHIFT_PRIVATE) & BIKSHED_GENERATION_MASK_PRIVATE;
    uint32_t i = 1;
    while (i < task_count)
    {
        Bikeshed_TaskID next_task_id                = task_ids[i];
        Bikeshed_TaskIndex_private next_task_index  = BIKESHED_TASK_INDEX_PRIVATE(next_task_id);
        struct Bikeshed_Task_private* next_task     = &shed->m_Tasks[next_task_index - 1];
        BIKESHED_FATAL_ASSERT_PRIVATE(next_task_id == next_task->m_TaskID, return )
        BIKESHED_FATAL_ASSERT_PRIVATE(0x20000000 == BIKESHED_ATOMICADD(&shed->m_Tasks[next_task_index - 1].m_ChildDependencyCount, 0x20000000), return)

        if (next_task->m_Channel != channel)
        {
            Bikeshed_ReadyRange_private(shed, gen, head_task_id, tail_task_index, channel);

            channel         = (uint8_t)next_task->m_Channel;
            head_task_id    = next_task_id;
            tail_task_index = next_task_index;
            ++i;
            continue;
        }

        shed->m_ReadyIndexes[tail_task_index - 1] = (long)next_task_index;
        tail_task_index                           = next_task_index;
        prev_task                                 = next_task;
        ++i;
    }

    Bikeshed_ReadyRange_private(shed, gen, head_task_id, tail_task_index, channel);

    if (shed->m_ReadyCallback)
    {
        shed->m_ReadyCallback->SignalReady(shed->m_ReadyCallback, task_count);
    }
}

int Bikeshed_ExecuteOne(Bikeshed shed, uint8_t channel)
{
    BIKESHED_FATAL_ASSERT_PRIVATE(&shed->m_ReadyHeads[channel] < shed->m_ReadyIndexes, return 0)
    uint32_t task_index = Bikeshed_PoolPop_private(&shed->m_ReadyHeads[channel], shed->m_ReadyIndexes);
    if (task_index == 0)
    {
        return 0;
    }

    struct Bikeshed_Task_private* task      = &shed->m_Tasks[task_index - 1];
    Bikeshed_TaskID task_id                 = task->m_TaskID;

    BIKESHED_FATAL_ASSERT_PRIVATE(channel == task->m_Channel, return 0)
    enum Bikeshed_TaskResult task_result    = task->m_TaskFunc(shed, task_id, channel, task->m_TaskContext);

    BIKESHED_FATAL_ASSERT_PRIVATE(0 == BIKESHED_ATOMICADD(&task->m_ChildDependencyCount, -0x20000000), return 0)

    if (task_result == BIKESHED_TASK_RESULT_COMPLETE)
    {
        if (task->m_FirstDependencyIndex)
        {
            Bikeshed_ResolveTask_private(shed, task_id);
        }
        Bikeshed_FreeTask_private(shed, task_id);
    }
    else
    {
        BIKESHED_FATAL_ASSERT_PRIVATE(BIKESHED_TASK_RESULT_BLOCKED == task_result, return 0)
    }

    return 1;
}

#endif // !defined(BIKESHED_IMPLEMENTATION)

#ifdef __cplusplus
}
#endif // __cplusplus

#endif // BIKESHED_INCLUDEGUARD_PRIVATE_H