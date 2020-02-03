#if !defined(__NX__)
#error "Unsupported platform"
#endif

#include <sys/time.h>

#include <assert.h>
#include <dlib/time.h>

//#include <nn/time/time_StandardSteadyClock.h>

#include <nn/nn_TimeSpan.h>
#include <nn/os/os_ThreadApi.h>

// TODO: Needs init/finalize??
// nn::time::Initialize();
// nn::time::Finalize()

namespace dmTime
{
    void Sleep(uint32_t useconds)
    {
        nn::os::SleepThread(nn::TimeSpan::FromMicroSeconds((int64_t)useconds));
    }

    uint64_t GetTime()
    {
        // // Since there is no gettimeofday on this platform, we have
        // // to come up with a replacement.
        // // Although this function is mostly used for delta times, we should still try to make it as accurate as possible
        // // Solution from here: https://developer.nintendo.com/group/development/g1kr9vj6/forums/english/-/gts_message_boards/thread/286490010#1018030
        // // with a possible 3 second discrepancy
        // static uint64_t offset = 0;
        // if (offset == 0)
        // {
        //     offset = clockContext.offset + clockContext.timeStamp.value;
        // }

        // ((double)nn::os::GetSystemTick() / nn::os::GetSystemTickFrequency() + offset)

        timeval tv;
        gettimeofday(&tv, 0);
        return ((uint64_t) tv.tv_sec) * 1000000U + tv.tv_usec;
    }
}
