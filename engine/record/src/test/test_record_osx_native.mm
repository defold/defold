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

#define JC_TEST_IMPLEMENTATION
#include <jc_test/jc_test.h>

// Exercise the real delegate and native lifecycle with a controlled stream,
// without opening a window or requesting screen-capture permission.
#include "../record/record_osx.mm"

static unsigned g_RemoveCount;
static unsigned g_StopCount;
static NSError* g_StopError;

@interface RecordTestStream : NSObject
@end

@implementation RecordTestStream
- (BOOL)removeRecordingOutput:(SCRecordingOutput*)output error:(NSError**)error
{
    (void)output;
    (void)error;
    ++g_RemoveCount;
    [dmRecord::g_Delegate recordingOutputDidFinishRecording:output];
    return YES;
}

- (void)stopCaptureWithCompletionHandler:(void (^)(NSError*))completion
{
    ++g_StopCount;
    completion(g_StopError);
}
@end

class RecordNativeTest : public jc_test_base_class
{
protected:
    char m_Path[1024];

    void SetUp()
    {
        const char* tmpdir = getenv("TMPDIR");
        if (!tmpdir || !tmpdir[0])
        {
            tmpdir = "/tmp";
        }
        ASSERT_LT(snprintf(m_Path, sizeof(m_Path), "%s/record-native-XXXXXX", tmpdir), (int)sizeof(m_Path));
        int file = mkstemp(m_Path);
        ASSERT_GE(file, 0);
        ASSERT_EQ(3, write(file, "mp4", 3));
        ASSERT_EQ(0, close(file));
        dmRecord::RecordingLock();
        memset(&dmRecord::g_Status, 0, sizeof(dmRecord::g_Status));
        dmRecord::g_Status.m_Active = true;
        snprintf(dmRecord::g_Status.m_Path, sizeof(dmRecord::g_Status.m_Path), "%s", m_Path);
        dmRecord::g_Stream = (SCStream*)[[RecordTestStream alloc] init];
        dmRecord::g_Output = (SCRecordingOutput*)[[NSObject alloc] init];
        dmRecord::g_Delegate = [[DefoldRecordingDelegate alloc] init];
        g_RemoveCount = g_StopCount = 0;
        g_StopError = nil;
    }

    void TearDown()
    {
        dmRecord::ReleaseRecorderObjects();
        ASSERT_EQ(0, unlink(m_Path));
    }
};

// A native output completion must request worker cleanup and preserve the finished file.
TEST_F(RecordNativeTest, ExternalOutputCompletion)
{
    if (!dmRecord::IsSupported())
    {
        SKIP();
    }
    [dmRecord::g_Delegate recordingOutputDidFinishRecording:dmRecord::g_Output];
    dmRecord::Status status;
    dmRecord::GetNativeStatus(&status);
    ASSERT_TRUE(status.m_Active);
    ASSERT_TRUE(status.m_Stopping);
    ASSERT_EQ(0, status.m_Failure[0]);
    ASSERT_TRUE(dmRecord::StopNative(&status));
    ASSERT_FALSE(status.m_Active);
    ASSERT_TRUE(status.m_Finalized);
    ASSERT_EQ(0U, g_RemoveCount);
    ASSERT_EQ(1U, g_StopCount);
    ASSERT_TRUE(dmRecord::g_Stream == nil);
    ASSERT_TRUE(dmRecord::g_Output == nil);
    ASSERT_TRUE(dmRecord::g_Delegate == nil);
}

// System UI cancellation is successful only after the output reports that the MP4 is ready.
TEST_F(RecordNativeTest, UserStoppedStream)
{
    if (!dmRecord::IsSupported())
    {
        SKIP();
    }
    NSError* error = [NSError errorWithDomain:SCStreamErrorDomain code:SCStreamErrorUserStopped userInfo:nil];
    [dmRecord::g_Delegate stream:dmRecord::g_Stream didStopWithError:error];
    dmRecord::Status status;
    dmRecord::GetNativeStatus(&status);
    ASSERT_TRUE(status.m_Stopping);
    ASSERT_FALSE(status.m_Finalized);
    ASSERT_EQ(0, status.m_Failure[0]);
    [dmRecord::g_Delegate recordingOutputDidFinishRecording:dmRecord::g_Output];
    ASSERT_TRUE(dmRecord::StopNative(&status));
    ASSERT_TRUE(status.m_Finalized);
    ASSERT_EQ(0, status.m_Failure[0]);
    ASSERT_EQ(0U, g_RemoveCount);
    ASSERT_EQ(0U, g_StopCount);
}

// An already stopped stream must not turn a successfully finalized MP4 into an error.
TEST_F(RecordNativeTest, OutputFinishesBeforeStreamStopNotification)
{
    if (!dmRecord::IsSupported())
    {
        SKIP();
    }
    [dmRecord::g_Delegate recordingOutputDidFinishRecording:dmRecord::g_Output];
    g_StopError = [NSError errorWithDomain:SCStreamErrorDomain code:SCStreamErrorAttemptToStopStreamState userInfo:nil];
    dmRecord::Status status;
    ASSERT_TRUE(dmRecord::StopNative(&status));
    ASSERT_TRUE(status.m_Finalized);
    ASSERT_EQ(0, status.m_Failure[0]);
    ASSERT_EQ(0U, g_RemoveCount);
    ASSERT_EQ(1U, g_StopCount);
}

// Unexpected stream termination must retain its failure even if the output finishes writing.
TEST_F(RecordNativeTest, StreamFailure)
{
    if (!dmRecord::IsSupported())
    {
        SKIP();
    }
    NSError* error = [NSError errorWithDomain:SCStreamErrorDomain
                                         code:SCStreamErrorInternalError
                                     userInfo:@ { NSLocalizedDescriptionKey : @"capture failed" }];
    [dmRecord::g_Delegate stream:dmRecord::g_Stream didStopWithError:error];
    [dmRecord::g_Delegate recordingOutputDidFinishRecording:dmRecord::g_Output];
    dmRecord::Status status;
    dmRecord::GetNativeStatus(&status);
    ASSERT_TRUE(status.m_Stopping);
    ASSERT_STREQ("capture failed", status.m_Failure);
    ASSERT_FALSE(dmRecord::StopNative(&status));
    ASSERT_FALSE(status.m_Active);
    ASSERT_FALSE(status.m_Finalized);
    ASSERT_STREQ("capture failed", status.m_Failure);
}

// An explicit stop must still detach the output, finalize the file and stop capture.
TEST_F(RecordNativeTest, ExplicitStop)
{
    if (!dmRecord::IsSupported())
    {
        SKIP();
    }
    dmRecord::Status status;
    ASSERT_TRUE(dmRecord::StopNative(&status));
    ASSERT_FALSE(status.m_Active);
    ASSERT_TRUE(status.m_Finalized);
    ASSERT_EQ(1U, g_RemoveCount);
    ASSERT_EQ(1U, g_StopCount);
}

static void SemaphoreReleased(void* context)
{
    dispatch_semaphore_signal((dispatch_semaphore_t)context);
}

// Both delegate semaphores must be destroyed when the recording releases its delegate.
TEST(RecordOwnership, DelegateSemaphores)
{
    dispatch_semaphore_t released = dispatch_semaphore_create(0);
    DefoldRecordingDelegate* delegate = [[DefoldRecordingDelegate alloc] init];
    dispatch_set_context(delegate->m_Started, released);
    dispatch_set_finalizer_f(delegate->m_Started, SemaphoreReleased);
    dispatch_set_context(delegate->m_Finished, released);
    dispatch_set_finalizer_f(delegate->m_Finished, SemaphoreReleased);
    [delegate release];
    ASSERT_TRUE(dmRecord::Wait(released, 5));
    ASSERT_TRUE(dmRecord::Wait(released, 5));
    dispatch_release(released);
}

// A late completion must keep its semaphore alive after a timeout, then release it.
TEST(RecordOwnership, CompletionAfterTimeout)
{
    dispatch_semaphore_t released = dispatch_semaphore_create(0);
    DefoldRecordingCompletion* completion = [[DefoldRecordingCompletion alloc] init];
    dispatch_set_context(completion->m_Completed, released);
    dispatch_set_finalizer_f(completion->m_Completed, SemaphoreReleased);
    void (^callback)(NSError*) = [^(NSError* error) {
      [completion completeWithResult:nil error:error];
    } copy];
    ASSERT_FALSE(dmRecord::Wait(completion->m_Completed, 0));
    [completion release];
    ASSERT_FALSE(dmRecord::Wait(released, 0));
    callback(nil);
    [callback release];
    ASSERT_TRUE(dmRecord::Wait(released, 5));
    dispatch_release(released);
}

int main(int argc, char** argv)
{
    @autoreleasepool
    {
        jc_test_init(&argc, argv);
        return jc_test_run_all();
    }
}
