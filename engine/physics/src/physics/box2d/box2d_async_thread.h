// Copyright 2020-2026 The Defold Foundation
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

#ifndef DM_BOX2D_ASYNC_THREAD_H
#define DM_BOX2D_ASYNC_THREAD_H

// Single-worker job runner for the async physics path. One job may be in flight at a time; the
// physics-world step is submitted here and the main thread waits for it at the next frame boundary.
// No physics knowledge lives in this module: it moves a (function, context) pair onto a background
// thread and provides a precise block-until-done wait, which the N-1 frame boundary needs.
//
// On a platform without threads (Emscripten built without pthreads) the worker degrades to running
// each submitted job inline on the calling thread, so callers get the same observable behaviour
// (Submit runs the work, Wait returns immediately) without a background thread.

namespace dmPhysics
{
    typedef void (*AsyncWorkerJob)(void* context);

    struct AsyncWorker;

    // Create a worker with a background thread (or the inline fallback where threads are absent).
    // name labels the thread for debuggers/profilers.
    AsyncWorker* NewAsyncWorker(const char* name);

    // Wait for any in-flight job, stop the thread, and free the worker.
    void DeleteAsyncWorker(AsyncWorker* worker);

    // Hand a job to the worker. The caller must ensure the worker is idle (no job in flight); the
    // async pipeline guarantees this by waiting before each submit. The job runs on the worker
    // thread; in the no-threads fallback it runs synchronously before Submit returns.
    void AsyncWorkerSubmit(AsyncWorker* worker, AsyncWorkerJob job, void* context);

    // Block until the in-flight job (if any) has finished. Returns immediately when the worker is idle.
    void AsyncWorkerWait(AsyncWorker* worker);

    // True when no job is in flight.
    bool AsyncWorkerIsIdle(AsyncWorker* worker);
}

#endif // DM_BOX2D_ASYNC_THREAD_H
