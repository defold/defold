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

#include "record_private.h"

#if defined(DM_PLATFORM_MACOS)

#import <AVFoundation/AVFoundation.h>
#import <AppKit/AppKit.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <dmsdk/dlib/time.h>
#include <dispatch/dispatch.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Every macOS 15 API below is protected by the runtime availability gate in the
// public entry points. ScreenCaptureKit itself is present from macOS 12.3.
#pragma clang diagnostic ignored "-Wunguarded-availability-new"

// A copied completion block retains this object until the callback returns.
// Timeouts can release the waiting thread's ownership without invalidating the
// semaphore or leaking results delivered after that thread has moved on.
@interface DefoldRecordingCompletion : NSObject
{
@public
    dispatch_semaphore_t m_Completed;
    id                   m_Result;
    NSError*             m_Error;
}
- (void)completeWithResult:(id)result error:(NSError*)error;
@end

@implementation DefoldRecordingCompletion
- (id)init
{
    self = [super init];
    if (self)
    {
        m_Completed = dispatch_semaphore_create(0);
    }
    return self;
}

- (void)completeWithResult:(id)result error:(NSError*)error
{
    m_Result = [result retain];
    m_Error = [error retain];
    dispatch_semaphore_signal(m_Completed);
}

- (void)dealloc
{
    dispatch_release(m_Completed);
    [m_Result release];
    [m_Error release];
    [super dealloc];
}
@end

@interface DefoldRecordingDelegate : NSObject <SCRecordingOutputDelegate, SCStreamDelegate>
{
@public
    dispatch_semaphore_t m_Started;
    dispatch_semaphore_t m_Finished;
    NSError*             m_Error;
    bool                 m_RecordingFinished;
    bool                 m_StreamStopped;
}
@end

@implementation DefoldRecordingDelegate

- (id)init
{
    self = [super init];
    if (self)
    {
        m_Started = dispatch_semaphore_create(0);
        m_Finished = dispatch_semaphore_create(0);
        m_Error = nil;
        m_RecordingFinished = false;
        m_StreamStopped = false;
    }
    return self;
}

- (void)dealloc
{
    dispatch_release(m_Started);
    dispatch_release(m_Finished);
    [m_Error release];
    [super dealloc];
}

- (void)recordingOutputDidStartRecording:(SCRecordingOutput*)recordingOutput
{
    (void)recordingOutput;
    dispatch_semaphore_signal(m_Started);
}

- (void)recordingOutput:(SCRecordingOutput*)recordingOutput didFailWithError:(NSError*)error
{
    (void)recordingOutput;
    @synchronized(self)
    {
        [m_Error release];
        m_Error = [error retain];
        m_RecordingFinished = true;
    }
    dispatch_semaphore_signal(m_Started);
    dispatch_semaphore_signal(m_Finished);
}

- (void)recordingOutputDidFinishRecording:(SCRecordingOutput*)recordingOutput
{
    (void)recordingOutput;
    @synchronized(self)
    {
        m_RecordingFinished = true;
    }
    dispatch_semaphore_signal(m_Finished);
}

- (void)stream:(SCStream*)stream didStopWithError:(NSError*)error
{
    (void)stream;
    @synchronized(self)
    {
        m_StreamStopped = true;
        // Stopping from the system recording UI is a normal completion. The
        // output delegate still tells us when the MP4 has finished writing.
        if (![[error domain] isEqualToString:SCStreamErrorDomain] || [error code] != SCStreamErrorUserStopped)
        {
            [m_Error release];
            m_Error = [error retain];
        }
    }
    dispatch_semaphore_signal(m_Started);
}

@end

namespace dmRecord
{
    // Only the main thread changes the window. Capture workers never dispatch
    // back to it, so shutdown can join the worker before restoring decorations.
    static NSWindow*         g_BorderlessWindow = nil;
    static NSWindowStyleMask g_WindowStyle;

    static NSLock*                  g_Lock = nil;
    static bool                     g_Starting = false;
    static SCStream*                g_Stream = nil;
    static SCRecordingOutput*       g_Output = nil;
    static DefoldRecordingDelegate* g_Delegate = nil;
    static Status                   g_Status;

    static NSLock* RecordingLock()
    {
        static dispatch_once_t once;
        dispatch_once(&once, ^{
          g_Lock = [[NSLock alloc] init];
          memset(&g_Status, 0, sizeof(g_Status));
        });
        return g_Lock;
    }

    static void CopyText(char* output, size_t output_size, NSString* value)
    {
        const char* text = value ? [value UTF8String] : "";
        CopyTextUtf8(output, output_size, text);
    }

    static void SetFailure(Status* status, NSString* failure)
    {
        CopyText(status->m_Failure, sizeof(status->m_Failure), failure);
    }

    bool PrepareOutputFile(const char* path, Status* status)
    {
        @autoreleasepool
        {
            NSString* output_path = [NSString stringWithUTF8String:path];
            if (!output_path)
            {
                SetFailure(status, @"recording path must be valid UTF-8");
                return false;
            }
            NSURL* output_url = [NSURL fileURLWithPath:output_path];
            NSFileManager* files = [NSFileManager defaultManager];
            NSError* error = nil;
            if (![files createDirectoryAtURL:[output_url URLByDeletingLastPathComponent]
                    withIntermediateDirectories:YES
                                     attributes:nil
                                          error:&error])
            {
                SetFailure(status, [error localizedDescription]);
                return false;
            }
            // unlink only removes files, even if the target becomes a directory
            // after the caller supplied it. NSFileManager removal is recursive.
            if (unlink([output_url fileSystemRepresentation]) != 0 && errno != ENOENT)
            {
                SetFailure(status, [[NSError errorWithDomain:NSPOSIXErrorDomain code:errno userInfo:nil] localizedDescription]);
                return false;
            }
            return true;
        }
    }

    static NSError* DelegateError(DefoldRecordingDelegate* delegate)
    {
        @synchronized(delegate)
        {
            return [[delegate->m_Error retain] autorelease];
        }
    }

    static bool RecordingFinished(DefoldRecordingDelegate* delegate)
    {
        @synchronized(delegate)
        {
            return delegate->m_RecordingFinished;
        }
    }

    static bool StreamStopped(DefoldRecordingDelegate* delegate)
    {
        @synchronized(delegate)
        {
            return delegate->m_StreamStopped;
        }
    }

    static bool Wait(dispatch_semaphore_t semaphore, uint64_t seconds)
    {
        return dispatch_semaphore_wait(semaphore,
                                       dispatch_time(DISPATCH_TIME_NOW, (int64_t)(seconds * NSEC_PER_SEC))) == 0;
    }

    bool GetWindow(void* native_window, Window* target)
    {
        NSWindow* window = (NSWindow*)native_window;
        if (!window)
        {
            return false;
        }
        NSRect frame = [window frame];
        NSRect content = [window contentRectForFrameRect:frame];
        target->m_Handle = (uintptr_t)[window windowNumber];
        target->m_X = NSMinX(content) - NSMinX(frame);
        target->m_Y = NSMaxY(frame) - NSMaxY(content);
        target->m_Width = NSWidth(content);
        target->m_Height = NSHeight(content);
        target->m_Scale = [window backingScaleFactor];
        return target->m_Width > 0 && target->m_Height > 0;
    }

    static void SetWindowStyle(NSWindow* window, NSWindowStyleMask style)
    {
        // Keep the rendered content at the same screen position and size while
        // AppKit rebuilds the surrounding frame. Preserve keyboard input too.
        NSRect content = [window contentRectForFrameRect:[window frame]];
        NSResponder* responder = [[window firstResponder] retain];
        [window setStyleMask:style];
        [window setFrame:[window frameRectForContentRect:content] display:YES];
        [window makeFirstResponder:responder];
        [responder release];
    }

    bool PrepareWindow(void* native_window, Window* target)
    {
        NSWindow* window = (NSWindow*)native_window;
        if (!window)
        {
            return false;
        }
        NSWindowStyleMask style = [window styleMask];
        // Fullscreen and already-borderless windows need no decoration changes.
        if ((style & NSWindowStyleMaskTitled) && !(style & NSWindowStyleMaskFullScreen))
        {
            g_BorderlessWindow = [window retain];
            g_WindowStyle = style;
            SetWindowStyle(window, style & ~(NSWindowStyleMaskTitled | NSWindowStyleMaskClosable));
        }
        return GetWindow(native_window, target);
    }

    void RestoreWindow()
    {
        NSWindow* window = g_BorderlessWindow;
        g_BorderlessWindow = nil;
        if (window)
        {
            // Preserve content geometry if the user moved or resized the window.
            SetWindowStyle(window, g_WindowStyle);
            [window release];
        }
    }

    static void ReleaseRecorderObjects()
    {
        [g_Stream release];
        [g_Output release];
        [g_Delegate release];
        g_Stream = nil;
        g_Output = nil;
        g_Delegate = nil;
    }

    bool IsSupported()
    {
        if (@available(macOS 15.0, *))
        {
            return true;
        }
        return false;
    }

    bool IsAudioSupported()
    {
        return IsSupported();
    }

    const char* GetBackendName()
    {
        return "screen_capture_kit";
    }

    const char* GetMinimumPlatformVersion()
    {
        return "macOS 15.0";
    }

    const char* GetUnsupportedReason()
    {
        return "native video recording requires macOS 15 or newer";
    }

    void GetNativeStatus(Status* status)
    {
        @autoreleasepool
        {
            [RecordingLock() lock];
            *status = g_Status;
            status->m_Supported = IsSupported();
            if (!status->m_Supported && status->m_Failure[0] == 0)
            {
                SetFailure(status, [NSString stringWithUTF8String:GetUnsupportedReason()]);
            }
            else if (g_Delegate)
            {
                NSError* error = DelegateError(g_Delegate);
                if (error)
                {
                    SetFailure(status, [error localizedDescription]);
                }
                if (error || RecordingFinished(g_Delegate) || StreamStopped(g_Delegate))
                {
                    status->m_Stopping = status->m_Active;
                }
            }
            [RecordingLock() unlock];
        }
    }

    bool StartNative(const Window* target, const char* path, uint32_t fps, bool audio, Status* status)
    {
        @autoreleasepool
        {
            [RecordingLock() lock];
            if (g_Starting || g_Stream)
            {
                *status = g_Status;
                SetFailure(status, @"a video recording is already active");
                [RecordingLock() unlock];
                return false;
            }
            memset(&g_Status, 0, sizeof(g_Status));
            g_Status.m_Supported = IsSupported();
            g_Starting = true;
            [RecordingLock() unlock];

            if (!IsSupported())
            {
                [RecordingLock() lock];
                SetFailure(&g_Status, [NSString stringWithUTF8String:GetUnsupportedReason()]);
                g_Starting = false;
                *status = g_Status;
                [RecordingLock() unlock];
                return false;
            }

            DefoldRecordingCompletion* content_ready = [[[DefoldRecordingCompletion alloc] init] autorelease];
            [SCShareableContent getShareableContentExcludingDesktopWindows:NO
                                                       onScreenWindowsOnly:YES
                                                         completionHandler:^(SCShareableContent* content, NSError* error) {
                                                           [content_ready completeWithResult:content error:error];
                                                         }];

            NSString* failure = nil;
            SCShareableContent* shareable = nil;
            if (!Wait(content_ready->m_Completed, 15))
            {
                failure = @"timed out while requesting screen-capture access";
            }
            else if (content_ready->m_Error)
            {
                failure = [content_ready->m_Error localizedDescription];
            }
            else
            {
                shareable = content_ready->m_Result;
            }

            SCWindow* selected_window = nil;
            if (!failure)
            {
                for (SCWindow* window in [shareable windows])
                {
                    if ([window windowID] == target->m_Handle)
                    {
                        selected_window = window;
                        break;
                    }
                }
                if (!selected_window)
                {
                    failure = @"could not find an on-screen window owned by the Defold process";
                }
            }

            SCStream* stream = nil;
            SCRecordingOutput* output = nil;
            DefoldRecordingDelegate* delegate = nil;
            uint32_t width = (uint32_t)llround(target->m_Width * target->m_Scale) & ~1U;
            uint32_t height = (uint32_t)llround(target->m_Height * target->m_Scale) & ~1U;
            if (!failure && (!width || !height))
            {
                failure = @"recording requires a content area of at least 2x2 pixels";
            }
            if (!failure)
            {
                // Match source and output pixels, cropping only codec alignment.
                CGRect source_rect = CGRectMake(target->m_X, target->m_Y, width / target->m_Scale, height / target->m_Scale);

                SCStreamConfiguration* configuration = [[SCStreamConfiguration alloc] init];
                [configuration setWidth:width];
                [configuration setHeight:height];
                [configuration setSourceRect:source_rect];
                [configuration setScalesToFit:NO];
                [configuration setMinimumFrameInterval:CMTimeMake(1, (int32_t)fps)];
                [configuration setQueueDepth:6];
                [configuration setPixelFormat:kCVPixelFormatType_32BGRA];
                [configuration setShowsCursor:NO];
                [configuration setCapturesAudio:audio ? YES : NO];
                [configuration setExcludesCurrentProcessAudio:NO];
                [configuration setSampleRate:48000];
                [configuration setChannelCount:2];
                [configuration setIgnoreShadowsSingleWindow:YES];
                [configuration setShouldBeOpaque:YES];

                SCContentFilter* filter = [[SCContentFilter alloc] initWithDesktopIndependentWindow:selected_window];
                delegate = [[DefoldRecordingDelegate alloc] init];
                stream = [[SCStream alloc] initWithFilter:filter configuration:configuration delegate:delegate];
                [filter release];
                [configuration release];

                Status file_status = {};
                if (!PrepareOutputFile(path, &file_status))
                {
                    failure = [NSString stringWithUTF8String:file_status.m_Failure];
                }
            }

            if (!failure)
            {
                NSURL* output_url = [NSURL fileURLWithPath:[NSString stringWithUTF8String:path]];
                SCRecordingOutputConfiguration* recording_configuration = [[SCRecordingOutputConfiguration alloc] init];
                [recording_configuration setOutputURL:output_url];
                [recording_configuration setOutputFileType:AVFileTypeMPEG4];
                [recording_configuration setVideoCodecType:AVVideoCodecTypeH264];
                output = [[SCRecordingOutput alloc] initWithConfiguration:recording_configuration delegate:delegate];
                [recording_configuration release];

                NSError* add_error = nil;
                if (![stream addRecordingOutput:output error:&add_error])
                {
                    failure = [add_error localizedDescription];
                }
            }

            if (!failure)
            {
                DefoldRecordingCompletion* capture_started = [[[DefoldRecordingCompletion alloc] init] autorelease];
                [stream startCaptureWithCompletionHandler:^(NSError* error) {
                  [capture_started completeWithResult:nil error:error];
                }];
                if (!Wait(capture_started->m_Completed, 15))
                {
                    failure = @"timed out while starting screen capture";
                }
                else if (capture_started->m_Error)
                {
                    failure = [capture_started->m_Error localizedDescription];
                }
            }

            if (!failure && !Wait(delegate->m_Started, 15))
            {
                failure = @"timed out waiting for video recording to start; check Screen Recording permission";
            }
            if (!failure)
            {
                NSError* delegate_error = DelegateError(delegate);
                if (delegate_error)
                {
                    failure = [delegate_error localizedDescription];
                }
            }

            [RecordingLock() lock];
            g_Starting = false;
            if (!failure)
            {
                g_Stream = stream;
                g_Output = output;
                g_Delegate = delegate;
                g_Status.m_Active = true;
                g_Status.m_Finalized = false;
                g_Status.m_Audio = audio;
                g_Status.m_Width = width;
                g_Status.m_Height = height;
                g_Status.m_Fps = fps;
                g_Status.m_StartedWallTime = dmTime::GetTime();
                g_Status.m_StartedMonotonicTime = dmTime::GetMonotonicTime();
                snprintf(g_Status.m_Path, sizeof(g_Status.m_Path), "%s", path);
            }
            else
            {
                SetFailure(&g_Status, failure);
            }
            *status = g_Status;
            [RecordingLock() unlock];

            if (failure)
            {
                if (stream)
                {
                    [stream stopCaptureWithCompletionHandler:nil];
                }
                [stream release];
                [output release];
                [delegate release];
            }
            return failure == nil;
        }
    }

    bool StopNative(Status* status)
    {
        @autoreleasepool
        {
            [RecordingLock() lock];
            if (!g_Stream || !g_Output || !g_Delegate)
            {
                *status = g_Status;
                SetFailure(status, @"no video recording is active");
                [RecordingLock() unlock];
                return false;
            }
            SCStream* stream = [g_Stream retain];
            SCRecordingOutput* output = [g_Output retain];
            DefoldRecordingDelegate* delegate = [g_Delegate retain];
            [RecordingLock() unlock];

            NSString* failure = nil;
            NSError* remove_error = nil;
            if (!RecordingFinished(delegate) && !StreamStopped(delegate) &&
                ![stream removeRecordingOutput:output error:&remove_error] &&
                !RecordingFinished(delegate) && !StreamStopped(delegate))
            {
                failure = [remove_error localizedDescription];
            }
            if (!failure && !Wait(delegate->m_Finished, 15))
            {
                failure = @"timed out while finalizing the MP4 file";
            }

            if (!StreamStopped(delegate))
            {
                DefoldRecordingCompletion* capture_stopped = [[[DefoldRecordingCompletion alloc] init] autorelease];
                [stream stopCaptureWithCompletionHandler:^(NSError* error) {
                  [capture_stopped completeWithResult:nil error:error];
                }];
                if (!Wait(capture_stopped->m_Completed, 15))
                {
                    if (!failure)
                    {
                        failure = @"timed out while stopping screen capture";
                    }
                }
                else if (capture_stopped->m_Error && !failure && !StreamStopped(delegate))
                {
                    // A completed output can precede the stream's stop callback.
                    NSError* stop_error = capture_stopped->m_Error;
                    bool already_stopped = RecordingFinished(delegate) &&
                        [[stop_error domain] isEqualToString:SCStreamErrorDomain] &&
                        [stop_error code] == SCStreamErrorAttemptToStopStreamState;
                    if (!already_stopped)
                    {
                        failure = [stop_error localizedDescription];
                    }
                }
            }
            NSError* delegate_error = DelegateError(delegate);
            if (delegate_error && !failure)
            {
                failure = [delegate_error localizedDescription];
            }

            [RecordingLock() lock];
            g_Status.m_Active = false;
            g_Status.m_StoppedWallTime = dmTime::GetTime();
            g_Status.m_StoppedMonotonicTime = dmTime::GetMonotonicTime();
            NSDictionary* attributes = [[NSFileManager defaultManager]
                attributesOfItemAtPath:[NSString stringWithUTF8String:g_Status.m_Path]
                                 error:nil];
            g_Status.m_Finalized = !failure && [[attributes objectForKey:NSFileSize] unsignedLongLongValue] > 0;
            if (failure)
            {
                SetFailure(&g_Status, failure);
            }
            ReleaseRecorderObjects();
            *status = g_Status;
            [RecordingLock() unlock];

            [stream release];
            [output release];
            [delegate release];
            return failure == nil && status->m_Finalized;
        }
    }

    void FinalizeNative()
    {
        Status status;
        GetNativeStatus(&status);
        if (status.m_Active)
        {
            StopNative(&status);
        }
    }
} // namespace dmRecord

#endif
