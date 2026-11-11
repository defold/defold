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

#include <string.h>
#include "data.h"

enum DataStoreSystemState
{
    DATASTORE_SYSTEM_PENDING,
    DATASTORE_SYSTEM_RUNNING,
    DATASTORE_SYSTEM_COMPLETE
};

static void ScheduleFunctions(HDataStoreContext store);

static int32_t ProcessFunction(HJobContext, HJob, void* context, void* data)
{
    DataStoreSystemRegistration* system = (DataStoreSystemRegistration*)context;
    const DataStoreSystemDesc&   desc = system->m_Desc;
    // The job carries its first row as integer user data; no range allocation is needed.
    uint32_t first = (uint32_t)(uintptr_t)data;
    uint32_t num_rows = DataStoreQueryGetRowCount(desc.m_Query) - first;
    if (desc.m_RowsPerJob && num_rows > desc.m_RowsPerJob)
    {
        num_rows = desc.m_RowsPerJob;
    }
    desc.m_Process(desc.m_Query, first, num_rows, desc.m_Context);
    return 0;
}

static void CompleteFunction(HJobContext, HJob, JobSystemStatus status, void* context, void*, int32_t)
{
    assert(status == JOBSYSTEM_STATUS_FINISHED);
    DataStoreSystemRegistration* system = (DataStoreSystemRegistration*)context;
    if (--system->m_PendingJobs)
    {
        return;
    }
    HDataStoreContext store = system->m_Desc.m_Query->m_Store;
    DataStoreQueryEnd(system->m_Desc.m_Query);
    system->m_State = DATASTORE_SYSTEM_COMPLETE;
    ScheduleFunctions(store);
}

static uint32_t FindSystem(HDataStoreContext store, dmhash_t name)
{
    for (uint32_t i = 0; i < store->m_Systems.Size(); ++i)
    {
        if (store->m_Systems[i].m_Desc.m_Name == name)
        {
            return i;
        }
    }
    return UINT32_MAX;
}

static DataStoreResult RegisterSystem(HDataStoreContext store, const DataStoreSystemDesc* desc)
{
    if (!desc->m_Name)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    if (FindSystem(store, desc->m_Name) != UINT32_MAX)
    {
        return DATASTORE_RESULT_ALREADY_EXISTS;
    }

    uint32_t num_dependencies = 0;
    while (num_dependencies < DATASTORE_SYSTEM_MAX_DEPENDENCIES && desc->m_DependsOn[num_dependencies])
    {
        ++num_dependencies;
    }

    DataStoreSystemRegistration system = {
        *desc,
        0,
        0,
        (uint8_t)num_dependencies
    };

    if (store->m_Systems.Full())
    {
        store->m_Systems.OffsetCapacity(8);
    }
    store->m_Systems.Push(system);
    store->m_ScheduleDirty = true;
    return DATASTORE_RESULT_OK;
}

DataStoreResult DataStoreFunctionAdd(HDataStoreContext store, const DataStoreSystemDesc* desc)
{
    if (store->m_ScheduleActive)
    {
        return DATASTORE_RESULT_LOCKED;
    }
    if (!desc->m_Query || !desc->m_Process || desc->m_Query->m_Store != store)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    return RegisterSystem(store, desc);
}

DataStoreResult DataStoreEventAdd(HDataStoreContext store, const DataStoreEventDesc* desc)
{
    if (store->m_ScheduleActive)
    {
        return DATASTORE_RESULT_LOCKED;
    }
    DataStoreSystemDesc system = { desc->m_Name };
    memcpy(system.m_DependsOn, desc->m_DependsOn, sizeof(system.m_DependsOn));
    return RegisterSystem(store, &system);
}

static DataStoreResult UnregisterSystem(HDataStoreContext store, dmhash_t name, bool event)
{
    if (store->m_ScheduleActive)
    {
        return DATASTORE_RESULT_LOCKED;
    }
    uint32_t index = FindSystem(store, name);
    if (index == UINT32_MAX)
    {
        return DATASTORE_RESULT_NOT_FOUND;
    }
    if ((store->m_Systems[index].m_Desc.m_Process == 0) != event)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    store->m_Systems.EraseSwap(index);
    store->m_ScheduleDirty = true;
    return DATASTORE_RESULT_OK;
}

DataStoreResult DataStoreFunctionRemove(HDataStoreContext store, dmhash_t name)
{
    return UnregisterSystem(store, name, false);
}

DataStoreResult DataStoreEventRemove(HDataStoreContext store, dmhash_t name)
{
    return UnregisterSystem(store, name, true);
}

static bool DependenciesComplete(HDataStoreContext store, const DataStoreSystemRegistration& system)
{
    for (uint32_t i = 0; i < system.m_DependencyCount; ++i)
    {
        uint32_t dependency = store->m_SystemDependencies[system.m_DependencyStart + i];
        if (store->m_Systems[dependency].m_State != DATASTORE_SYSTEM_COMPLETE)
        {
            return false;
        }
    }
    return true;
}

static DataStoreResult ResolveDependencies(HDataStoreContext store)
{
    store->m_SystemDependencies.SetSize(0);
    for (uint32_t i = 0; i < store->m_Systems.Size(); ++i)
    {
        DataStoreSystemRegistration& system = store->m_Systems[i];
        system.m_State = DATASTORE_SYSTEM_PENDING;
        system.m_DependencyStart = store->m_SystemDependencies.Size();
        Reserve(store->m_SystemDependencies, system.m_DependencyStart + system.m_DependencyCount);
        for (uint32_t d = 0; d < system.m_DependencyCount; ++d)
        {
            uint32_t dependency = FindSystem(store, system.m_Desc.m_DependsOn[d]);
            if (dependency == UINT32_MAX)
            {
                return DATASTORE_RESULT_NOT_FOUND;
            }
            store->m_SystemDependencies.Push(dependency);
        }
    }

    // Check the graph without recursion or invoking callbacks. Begin resets these
    // states for execution; resolved indices stay valid until registration changes.
    uint32_t complete = 0;
    while (complete < store->m_Systems.Size())
    {
        uint32_t previous = complete;
        for (uint32_t i = 0; i < store->m_Systems.Size(); ++i)
        {
            DataStoreSystemRegistration& system = store->m_Systems[i];
            if (system.m_State == DATASTORE_SYSTEM_PENDING && DependenciesComplete(store, system))
            {
                system.m_State = DATASTORE_SYSTEM_COMPLETE;
                ++complete;
            }
        }
        if (complete == previous)
        {
            return DATASTORE_RESULT_INVALID_ARGUMENT;
        }
    }
    store->m_ScheduleDirty = false;
    return DATASTORE_RESULT_OK;
}

DataStoreResult DataStoreSchedulerBegin(HDataStoreContext store, HJobContext jobs)
{
    {
        DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
        if (store->m_ScheduleActive)
        {
            return DATASTORE_RESULT_LOCKED;
        }
        if (!jobs)
        {
            return DATASTORE_RESULT_INVALID_ARGUMENT;
        }
        if (store->m_ScheduleDirty)
        {
            DataStoreResult result = ResolveDependencies(store);
            if (result != DATASTORE_RESULT_OK)
            {
                return result;
            }
        }
        for (uint32_t i = 0; i < store->m_Systems.Size(); ++i)
        {
            store->m_Systems[i].m_State = DATASTORE_SYSTEM_PENDING;
        }
        store->m_ScheduleJobs = jobs;
        store->m_ScheduleActive = true;
    }
    ScheduleFunctions(store);
    return DATASTORE_RESULT_OK;
}

static void ScheduleFunctions(HDataStoreContext store)
{
    for (uint32_t i = 0; i < store->m_Systems.Size(); ++i)
    {
        DataStoreSystemRegistration& system = store->m_Systems[i];
        if (!system.m_Desc.m_Process || system.m_State != DATASTORE_SYSTEM_PENDING || !DependenciesComplete(store, system))
        {
            continue;
        }
        if (DataStoreQueryTryBegin(system.m_Desc.m_Query) == DATASTORE_RESULT_BUSY)
        {
            continue;
        }
        system.m_State = DATASTORE_SYSTEM_RUNNING;
        uint32_t num_rows = DataStoreQueryGetRowCount(system.m_Desc.m_Query);
        uint32_t rows_per_job = system.m_Desc.m_RowsPerJob ? system.m_Desc.m_RowsPerJob : num_rows;
        uint32_t num_jobs = num_rows ? 1 + (num_rows - 1) / rows_per_job : 1;
        system.m_PendingJobs = num_jobs;
        for (uint32_t j = 0; j < num_jobs; ++j)
        {
            uint32_t        first = j * rows_per_job;
            Job             job = { ProcessFunction, CompleteFunction, &system, (void*)(uintptr_t)first };
            HJob            handle = JobSystemCreateJob(store->m_ScheduleJobs, &job);
            JobSystemResult result = JobSystemPushJob(store->m_ScheduleJobs, handle);
            assert(result == JOBSYSTEM_RESULT_OK);
            (void)result;
        }
    }
}

DataStoreResult DataStoreSchedulerUpdate(HDataStoreContext store)
{
    if (!store->m_ScheduleActive)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    JobSystemUpdate(store->m_ScheduleJobs, 0);
    // A reservation held outside the scheduler may have ended since the last event.
    ScheduleFunctions(store);
    for (uint32_t i = 0; i < store->m_Systems.Size(); ++i)
    {
        const DataStoreSystemRegistration& system = store->m_Systems[i];
        if (system.m_Desc.m_Process && system.m_State != DATASTORE_SYSTEM_COMPLETE)
        {
            return DATASTORE_RESULT_BUSY;
        }
    }
    return DATASTORE_RESULT_OK;
}

DataStoreResult DataStoreSchedulerEvent(HDataStoreContext store, dmhash_t name)
{
    if (!store->m_ScheduleActive)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    uint32_t index = FindSystem(store, name);
    if (index == UINT32_MAX)
    {
        return DATASTORE_RESULT_NOT_FOUND;
    }
    DataStoreSystemRegistration& system = store->m_Systems[index];
    if (system.m_Desc.m_Process || system.m_State != DATASTORE_SYSTEM_PENDING)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    if (!DependenciesComplete(store, system))
    {
        return DATASTORE_RESULT_BUSY;
    }
    system.m_State = DATASTORE_SYSTEM_COMPLETE;
    ScheduleFunctions(store);
    return DATASTORE_RESULT_OK;
}

static bool IsScheduleBusy(HDataStoreContext store)
{
    for (uint32_t i = 0; i < store->m_Systems.Size(); ++i)
    {
        const DataStoreSystemRegistration& system = store->m_Systems[i];
        if (system.m_State == DATASTORE_SYSTEM_RUNNING ||
            (system.m_Desc.m_Process && system.m_State == DATASTORE_SYSTEM_PENDING && DependenciesComplete(store, system)))
        {
            return true;
        }
    }
    return false;
}

DataStoreResult DataStoreSchedulerSync(HDataStoreContext store)
{
    if (!store->m_ScheduleActive)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    while (IsScheduleBusy(store))
    {
        DataStoreSchedulerUpdate(store);
    }
    return DATASTORE_RESULT_OK;
}

DataStoreResult DataStoreSchedulerEnd(HDataStoreContext store)
{
    DM_MUTEX_SCOPED_LOCK(store->m_Mutex);
    if (!store->m_ScheduleActive)
    {
        return DATASTORE_RESULT_INVALID_ARGUMENT;
    }
    if (IsScheduleBusy(store))
    {
        return DATASTORE_RESULT_BUSY;
    }
    store->m_ScheduleActive = false;
    store->m_ScheduleJobs = 0;
    return DATASTORE_RESULT_OK;
}
