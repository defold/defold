#if !defined(__NX__)
#error "Unsupported platform"
#endif

#include <assert.h>
#include <stdlib.h>
#include <dlib/thread.h>
#include <dlib/log.h>

#include <nn/nn_Result.h>
#include <nn/os/os_ThreadTypes.h>
#include <nn/os/os_ThreadApi.h>

namespace dmThread
{
    Thread New(ThreadStart thread_start, uint32_t stack_size, void* arg, const char* name)
    {
        assert(stack_size % nn::os::ThreadStackAlignment == 0);

        nn::os::ThreadType* thread = (nn::os::ThreadType*)malloc(sizeof(nn::os::ThreadType));

        // We simply pass it in here, leaving it as is.
        // Currently no way to get stack pointer, but do we really need to free it when the app goes down?
        void* stack = malloc(stack_size);

        nn::Result result = nn::os::CreateThread(thread, (nn::os::ThreadFunction)thread_start, arg, stack, stack_size, nn::os::DefaultThreadPriority);
        if (!result.IsSuccess())
        {
            dmLogError("Failed to create thread %s", name);
            assert(false);
        }

        nn::os::SetThreadName(thread, name);

        nn::os::StartThread(thread);
        return thread;
    }

    void Join(Thread thread)
    {
        nn::os::WaitThread(thread);
    }

    Thread GetCurrentThread()
    {
        return nn::os::GetCurrentThread();
    }

    void SetThreadName(Thread thread, const char* name)
    {
        (void)nn::os::SetThreadName(thread, name);
    }

    // https://developer.nintendo.com/html/online-docs/nx-en/g1kr9vj6-en/Packages/SDK/NintendoSDK/Documents/Package/contents/external.html?file=../../Api/HtmlNX/index.html
    TlsKey AllocTls()
    {
        TlsKey key;
        nn::Result result = nn::os::AllocateTlsSlot(&key, 0);
        assert(result.IsSuccess());
        return key;
    }

    void FreeTls(TlsKey key)
    {
        nn::os::FreeTlsSlot(key);
    }

    void SetTlsValue(TlsKey key, void* value)
    {
        nn::os::SetTlsValue(key, (uintptr_t)value);
    }

    void* GetTlsValue(TlsKey key)
    {
        return (void*)nn::os::GetTlsValue(key);
    }
}



