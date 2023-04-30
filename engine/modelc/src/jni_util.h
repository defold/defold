#ifndef DM_JNI_SIGNAL_HANDLER_H
#define DM_JNI_SIGNAL_HANDLER_H

#include <stdint.h>
#include <jni.h>

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

    // Caller has to free() the memory
    char* GenerateCallstack(char* buffer, uint32_t buffer_length);

// for testing
// Pass in a string containing any of the SIG* numbers
    void TestSignalFromString(const char* signal);

}

#endif
