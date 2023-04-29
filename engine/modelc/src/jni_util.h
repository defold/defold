#ifndef DM_JNI_SIGNAL_HANDLER_H
#define DM_JNI_SIGNAL_HANDLER_H

#include <jni.h>

namespace dmJNI
{
    void EnableDefaultSignalHandlers(JavaVM* vm);
    void EnableSignalHandlers(void* ctx, void (*callback)(int signal, void* ctx));
    void DisableSignalHandlers();

    // Caller has to free() the memory
    char* GenerateCallstack();

// for testing
// Pass in a string containing any of the SIG* numbers
    void TestSignalFromString(const char* signal);

}

#endif
