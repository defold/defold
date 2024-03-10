// Copyright 2020-2024 The Defold Foundation
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

#ifndef DM_JNI_SIGNAL_HANDLER_H
#define DM_JNI_SIGNAL_HANDLER_H

#include <stdint.h>
#include <jni.h>

#if defined(_WIN32)
    #include <Windows.h>
    #include <Dbghelp.h>
    #include <excpt.h>
#endif

namespace dmJNI
{
    void EnableDefaultSignalHandlers(JavaVM* vm);
    void EnableSignalHandlers(void* ctx, void (*callback)(int signal, void* ctx));
    void DisableSignalHandlers();

    // Used to enable a jni context for a small period of time
    bool IsContextAdded(JNIEnv* env);
    void AddContext(JNIEnv* env);
    void RemoveContext(JNIEnv* env);

    struct SignalContextScope
    {
        JNIEnv* m_Env;
        SignalContextScope(JNIEnv* env) : m_Env(env)
        {
            AddContext(m_Env);
        }
        ~SignalContextScope()
        {
            RemoveContext(m_Env);
        }
    };


#if defined(_WIN32)
    LONG WINAPI ExceptionHandler(EXCEPTION_POINTERS* ptr);

    #define DM_JNI_GUARD_SCOPE_BEGIN() \
        __try { \

    #define DM_JNI_GUARD_SCOPE_END(...) \
        } __except (dmJNI::ExceptionHandler(GetExceptionInformation())) { \
            __VA_ARGS__ \
        }

#else
    #define DM_JNI_GUARD_SCOPE_BEGIN() { int _scope_result = 0;
    #define DM_JNI_GUARD_SCOPE_END(...) \
            if (_scope_result) { \
                __VA_ARGS__ \
            } \
        }

#endif

    // Caller has to free() the memory
    char* GenerateCallstack(char* buffer, uint32_t buffer_length);

// for testing
// Pass in a string containing any of the SIG* numbers
    void TestSignalFromString(const char* signal);

}

#endif
