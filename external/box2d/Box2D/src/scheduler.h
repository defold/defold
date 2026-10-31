// SPDX-FileCopyrightText: 2026 Erin Catto
// SPDX-License-Identifier: MIT

#pragma once

#ifdef __cplusplus
extern "C"
{
#endif

typedef void b2TaskCallbackFcn( void* taskContext );
typedef struct b2Scheduler b2Scheduler;

// The scheduler owns each returned handle. joinThread must wait for the thread and release its handle.
typedef struct b2SchedulerThreadInterface
{
	void* ( *createThread )( void ( *function )( void* ), void* context, const char* name );
	void ( *joinThread )( void* thread );
} b2SchedulerThreadInterface;

// NULL selects native threads. The interface is copied; callbacks must remain valid until destruction.
b2Scheduler* b2CreateScheduler( int workerCount, const b2SchedulerThreadInterface* threadInterface );
void b2DestroyScheduler( b2Scheduler* scheduler );
void b2ResetScheduler( b2Scheduler* scheduler );

// See b2EnqueueTaskCallbackFcn and b2FinishTaskCallbackFcn
void* b2SchedulerEnqueueTask( b2TaskCallbackFcn* task, void* taskContext, void* userContext );
void b2SchedulerFinishTask( void* userTask, void* userContext );

#ifdef __cplusplus
}
#endif
