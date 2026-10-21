// Copyright 2020-2026 The Defold Foundation
// Copyright 2014-2020 King
// Copyright 2009-2014 Ragnar Svensson, Christian Murray
// Licensed under the Defold License version 1.0 (the "License"); you may not use
// this file except in compliance with the License.
//
// You may obtain a copy of the License, together with FAQs at
// https://www.defold.com/license
//
// Unless required by applicable law or agreed to in writing, software distributed
// under the License is distributed on an "AS IS" BASIS, WITHOUT WARRANTIES OR
// CONDITIONS OF ANY KIND, either express or implied. See the License for the
// specific language governing permissions and limitations under the License.

#ifndef DMSDK_DATASTORE_SCHEDULER_H
#define DMSDK_DATASTORE_SCHEDULER_H

#include <dmsdk/datastore/types.h>
#include <dmsdk/dlib/hash.h>
#include <dmsdk/dlib/jobsystem.h>

#define DS_SYSTEM_MAX_DEPENDENCIES 16
#define DS_EVENT_GAMEOBJECT_PRE_UPDATE dmHashString64("engine.gameobject.pre_update")

#ifdef __cplusplus
extern "C"
{
#endif

    typedef void (*DSSystemProcess)(HDSQuery query, uint32_t first, uint32_t num_rows, void* context);

    typedef struct DSSystemDesc
    {
        dmhash_t        m_Name;
        dmhash_t        m_DependsOn[DS_SYSTEM_MAX_DEPENDENCIES];
        HDSQuery        m_Query;
        DSSystemProcess m_Process;
        void*           m_Context;
    } DSSystemDesc;

    typedef struct DSEventDesc
    {
        dmhash_t m_Name;
        dmhash_t m_DependsOn[DS_SYSTEM_MAX_DEPENDENCIES];
    } DSEventDesc;

    DSResult DSRegisterFunction(HDSContext store, const DSSystemDesc* desc);
    DSResult DSUnregisterFunction(HDSContext store, dmhash_t name);
    DSResult DSRegisterEvent(HDSContext store, const DSEventDesc* desc);
    DSResult DSUnregisterEvent(HDSContext store, dmhash_t name);
    DSResult DSBeginSchedule(HDSContext store, HJobContext jobs);
    DSResult DSSchedulerEvent(HDSContext store, dmhash_t name);
    DSResult DSSchedulerUpdate(HDSContext store);
    DSResult DSEndSchedule(HDSContext store);

#ifdef __cplusplus
}
#endif

// API documentation

/*# Data scheduler API
 *
 * Named query callbacks and engine events owned by [type:HDSContext].
 * The scheduler submits ready functions to a borrowed job context and tracks completion.
 * The caller signals stages and pumps updates on the main thread; it owns the job context.
 * Call scheduler operations outside job and completion callbacks.
 *
 * @document
 * @name DSScheduler
 * @language C
 */

/*# Maximum function or event dependencies
 *
 * @constant
 * @name DS_SYSTEM_MAX_DEPENDENCIES
 * @value 16
 */

/*# Before game-object updates
 *
 * Registered by the engine in its DATASTORE_CONTEXT_NAME store. Each frame, after
 * input dispatch and before game-object updates, the engine starts a schedule and
 * signals this event. It waits for submitted functions and their dependents before
 * updating components. Callbacks run on a dedicated engine job context; do not access game
 * objects or Lua from workers. This does not refresh world transforms.
 * This macro computes the event hash; cache it during initialization when signaling
 * repeatedly. Standalone stores must register and signal their own events.
 *
 * @constant
 * @name DS_EVENT_GAMEOBJECT_PRE_UPDATE
 * @value dmHashString64("engine.gameobject.pre_update")
 */

/*# Process a query range
 *
 * The scheduler reserves the query and submits one job calling this function for all
 * matching rows, including an empty query. Create a DSQueryIterRange iterator for
 * the supplied range. Compatible registered functions may run concurrently.
 * Returning finishes this function's work; do not leave child jobs running or retain
 * iterators/pointers. The scheduler releases query access and schedules dependents.
 * Synchronize shared context writes and do not call scheduler operations from workers.
 *
 * @typedef
 * @name DSSystemProcess
 * @param query [type:HDSQuery] Registered query with active access.
 * @param first [type:uint32_t] First row in the query's reserved range; currently zero.
 * @param num_rows [type:uint32_t] Number of rows to process; currently the complete query.
 * @param context [type:void*] Borrowed user context from registration; may be zero.
 */

/*# System registration
 *
 * Hashes are copied during registration. The query must belong to the supplied store.
 * Keep the query and context alive until unregistration or store teardown. At teardown,
 * finish callbacks, end the schedule and destroy queries before the store.
 * Dependencies name complete updates and may refer to updates registered later.
 * [ref:DSBeginSchedule] resolves hashes to indices and rejects cycles.
 * Use dmHashString64 during setup; reverse hashes are available for diagnostics.
 * Prerequisites may name functions or events. Every prerequisite must be registered;
 * the engine registers [ref:DS_EVENT_GAMEOBJECT_PRE_UPDATE] in its own store.
 *
 * @struct
 * @name DSSystemDesc
 * @member m_Name [type:dmhash_t] Nonzero name hash, unique among functions and events in the store.
 * @member m_DependsOn [type:dmhash_t[]] Prerequisite hashes, packed from index zero. Stop at the first zero or use all DS_SYSTEM_MAX_DEPENDENCIES entries.
 * @member m_Query [type:HDSQuery] Borrowed query.
 * @member m_Process [type:DSSystemProcess] Required processing callback.
 * @member m_Context [type:void*] Borrowed user context passed unchanged to the callback.
 */

/*# Engine event registration
 *
 * An externally signaled milestone in the same dependency graph as query callbacks.
 * Hashes are copied. Dependencies may name functions or events registered later;
 * DSBeginSchedule rejects missing prerequisites and cycles.
 *
 * @struct
 * @name DSEventDesc
 * @member m_Name [type:dmhash_t] Nonzero name hash, unique among functions and events in the store.
 * @member m_DependsOn [type:dmhash_t[]] Prerequisite hashes that must complete before signaling succeeds. Packed from index zero; stop at the first zero or use all DS_SYSTEM_MAX_DEPENDENCIES entries.
 */

/*# Register a query callback
 *
 * Copies the descriptor and hashes; registration does not invoke the callback.
 *
 * @name DSRegisterFunction
 * @param store [type:HDSContext] Store receiving the registration.
 * @param desc [type:const DSSystemDesc*] Callback, query, context and dependency hashes.
 * @return result [type:DSResult] OK, LOCKED during a schedule, ALREADY_EXISTS for a duplicate name, or INVALID_ARGUMENT for missing required values, a zero name hash or a query from another store.
 */

/*# Unregister a query callback
 *
 * Removes the named registration between schedules. The query
 * and user context remain caller-owned. Dependent registrations retain their hashes.
 *
 * @name DSUnregisterFunction
 * @param store [type:HDSContext] Store containing the registration.
 * @param name [type:dmhash_t] Name hash supplied when registering the callback.
 * @return result [type:DSResult] OK, LOCKED during a schedule, NOT_FOUND for an unknown name, or INVALID_ARGUMENT if the name identifies an event.
 */

/*# Register an engine event
 *
 * Copies the descriptor and hashes between schedules. Events have no query or callback;
 * the caller signals them explicitly with DSSchedulerEvent.
 *
 * @name DSRegisterEvent
 * @param store [type:HDSContext] Store receiving the registration.
 * @param desc [type:const DSEventDesc*] Event hash and prerequisite hashes.
 * @return result [type:DSResult] OK, LOCKED during a schedule, ALREADY_EXISTS for a duplicate name, or INVALID_ARGUMENT for a zero name hash.
 */

/*# Unregister an engine event
 *
 * Removes the named event between schedules. Dependent registrations retain their hashes.
 *
 * @name DSUnregisterEvent
 * @param store [type:HDSContext] Store containing the event.
 * @param name [type:dmhash_t] Name hash supplied when registering the event.
 * @return result [type:DSResult] OK, LOCKED during a schedule, NOT_FOUND for an unknown name, or INVALID_ARGUMENT if the name identifies a function.
 */

/*# Begin a schedule
 *
 * Starts one invocation of all registered functions and events. Resolves dependencies
 * after registration changes and clears function completion and event signals from the previous invocation.
 * Reserves and submits one job for each ready function. Borrowed job context must
 * outlive this invocation. Zero-worker contexts execute work during DSSchedulerUpdate.
 * Pair success with DSEndSchedule; no threads are created or destroyed here.
 *
 * @name DSBeginSchedule
 * @param store [type:HDSContext] Store containing the registrations.
 * @param jobs [type:HJobContext] Caller-owned job context. Pump it on the same main thread as scheduler operations.
 * @return result [type:DSResult] OK, LOCKED if already active, NOT_FOUND for a missing prerequisite, or INVALID_ARGUMENT for a null job context or dependency cycle. Failure leaves the schedule inactive except when already active.
 */

/*# Signal an engine event
 *
 * Signals an engine milestone, such as the start of an update stage, once its own
 * prerequisites are satisfied. The scheduler immediately submits every ready function,
 * subject to remaining dependencies and query access checks. This call does not wait
 * for jobs. BUSY leaves the event pending: pump DSSchedulerUpdate and retry before
 * entering an engine stage that requires those prerequisites.
 * Signal each event once per invocation; DSBeginSchedule clears previous signals.
 * Events do not reserve data or protect engine accesses outside registered queries.
 *
 * @name DSSchedulerEvent
 * @param store [type:HDSContext] Store with an active schedule.
 * @param name [type:dmhash_t] Registered event hash.
 * @return result [type:DSResult] OK, BUSY while prerequisites are incomplete, NOT_FOUND for an unknown name, or INVALID_ARGUMENT without an active schedule, for a function name or for an already signaled event.
 */

/*# Update scheduled work
 *
 * Pumps the borrowed job context's completion callbacks on the main thread. In a
 * zero-worker context, also executes at most one queued job. This may service other
 * users of the same job context. Do not call recursively from a job or completion callback.
 *
 * Job completion releases query access and automatically submits newly ready functions.
 * Also retries functions blocked by external query reservations. Read/read and disjoint
 * writes may overlap; conflicting accesses wait. Use m_DependsOn when order matters.
 * The same query handle cannot have two active reservations. Unsignaled events do
 * not keep this call BUSY unless a pending function depends on them.
 *
 * @name DSSchedulerUpdate
 * @param store [type:HDSContext] Store with an active schedule.
 * @return result [type:DSResult] OK when all functions finished, BUSY while functions await jobs, events or reservations, or INVALID_ARGUMENT without an active schedule.
 */

/*# End a schedule
 *
 * Ends the invocation, discarding unsignaled events and functions waiting on them.
 * Returns BUSY while jobs await completion or ready functions await query access;
 * keep pumping DSSchedulerUpdate before retrying. A failed end retains reservations
 * and dependencies.
 * Registration changes are allowed after success. The next begin starts fresh.
 *
 * @name DSEndSchedule
 * @param store [type:HDSContext] Store with an active schedule.
 * @return result [type:DSResult] OK, BUSY while functions are running or ready to run, or INVALID_ARGUMENT without an active schedule.
 */

#endif // DMSDK_DATASTORE_SCHEDULER_H
