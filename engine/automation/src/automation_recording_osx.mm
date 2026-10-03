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

#include "automation_recording.h"

#if defined(DM_PLATFORM_MACOS)

#import <AVFoundation/AVFoundation.h>
#import <AppKit/AppKit.h>
#import <ScreenCaptureKit/ScreenCaptureKit.h>

#include <dmsdk/dlib/time.h>
#include <dmsdk/graphics/graphics_native.h>
#include <dispatch/dispatch.h>
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>

// Every macOS 15 API below is protected by the runtime availability gate in the
// public entry points. ScreenCaptureKit itself is present from macOS 12.3.
#pragma clang diagnostic ignored "-Wunguarded-availability-new"

@interface AutomationBridgeRecordingDelegate : NSObject <SCRecordingOutputDelegate>
{
@public
    dispatch_semaphore_t m_Started;
    dispatch_semaphore_t m_Finished;
    NSError*             m_Error;
}
@end

@implementation AutomationBridgeRecordingDelegate

- (id)init
{
    self = [super init];
    if (self)
    {
        m_Started = dispatch_semaphore_create(0);
        m_Finished = dispatch_semaphore_create(0);
        m_Error = nil;
    }
    return self;
}

- (void)dealloc
{
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
    }
    dispatch_semaphore_signal(m_Started);
    dispatch_semaphore_signal(m_Finished);
}

- (void)recordingOutputDidFinishRecording:(SCRecordingOutput*)recordingOutput
{
    (void)recordingOutput;
    dispatch_semaphore_signal(m_Finished);
}

@end

namespace dmAutomation
{
    // AppKit includes the rounded bottom edge in NSWindow's content rect. The
    // window server masks roughly 14-15 points there on current macOS releases,
    // so exclude a narrow safety row to produce a fully rectangular game frame.
    static const CGFloat WINDOW_BOTTOM_CORNER_CROP_POINTS = 16.0;

    static NSLock* g_Lock = nil;
    static bool g_Starting = false;
    static SCStream* g_Stream = nil;
    static SCRecordingOutput* g_Output = nil;
    static AutomationBridgeRecordingDelegate* g_Delegate = nil;
    static RecordingStatus g_Status;

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
        snprintf(output, output_size, "%s", text ? text : "");
    }

    static void SetFailure(RecordingStatus* status, NSString* failure)
    {
        CopyText(status->m_Failure, sizeof(status->m_Failure), failure);
    }

    static NSError* DelegateError(AutomationBridgeRecordingDelegate* delegate)
    {
        @synchronized(delegate)
        {
            return [[delegate->m_Error retain] autorelease];
        }
    }

    static bool Wait(dispatch_semaphore_t semaphore, uint64_t seconds)
    {
        return dispatch_semaphore_wait(semaphore,
            dispatch_time(DISPATCH_TIME_NOW, (int64_t)(seconds * NSEC_PER_SEC))) == 0;
    }

    bool GetRecordingWindow(RecordingWindow* target)
    {
        NSWindow* window = dmGraphics::GetNativeOSXNSWindow();
        if (!window) return false;
        NSRect frame = [window frame];
        NSRect content = [window contentRectForFrameRect:frame];
        target->m_Handle = (uintptr_t)[window windowNumber];
        target->m_X = NSMinX(content) - NSMinX(frame);
        target->m_Y = NSMaxY(frame) - NSMaxY(content);
        target->m_Width = NSWidth(content);
        target->m_Height = NSHeight(content) - MIN(WINDOW_BOTTOM_CORNER_CROP_POINTS, NSHeight(content) * 0.25);
        target->m_Scale = [window backingScaleFactor];
        return target->m_Width > 0 && target->m_Height > 0;
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

    bool IsNativeRecordingSupported()
    {
        if (@available(macOS 15.0, *))
        {
            return true;
        }
        return false;
    }

    bool IsNativeRecordingAudioSupported()
    {
        return IsNativeRecordingSupported();
    }

    const char* NativeRecordingBackendName()
    {
        return "screen_capture_kit";
    }

    const char* NativeRecordingMinimumPlatformVersion()
    {
        return "macOS 15.0";
    }

    const char* NativeRecordingUnsupportedReason()
    {
        return "native video recording requires macOS 15 or newer";
    }

    void GetNativeRecordingStatus(RecordingStatus* status)
    {
        @autoreleasepool
        {
            [RecordingLock() lock];
            *status = g_Status;
            status->m_Supported = IsNativeRecordingSupported();
            if (!status->m_Supported && status->m_Failure[0] == 0)
            {
                SetFailure(status, [NSString stringWithUTF8String:NativeRecordingUnsupportedReason()]);
            }
            else if (g_Delegate)
            {
                NSError* error = DelegateError(g_Delegate);
                if (error)
                {
                    SetFailure(status, [error localizedDescription]);
                }
            }
            [RecordingLock() unlock];
        }
    }

    bool StartNativeRecording(const RecordingWindow* target, const char* path, uint32_t requested_width, uint32_t requested_height,
                              uint32_t fps, bool audio, RecordingStatus* status)
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
            g_Status.m_Supported = IsNativeRecordingSupported();
            g_Starting = true;
            [RecordingLock() unlock];

            if (!IsNativeRecordingSupported())
            {
                [RecordingLock() lock];
                SetFailure(&g_Status, [NSString stringWithUTF8String:NativeRecordingUnsupportedReason()]);
                g_Starting = false;
                *status = g_Status;
                [RecordingLock() unlock];
                return false;
            }

            __block SCShareableContent* shareable = nil;
            __block NSError* setup_error = nil;
            dispatch_semaphore_t content_ready = dispatch_semaphore_create(0);
            [SCShareableContent getShareableContentExcludingDesktopWindows:NO
                                                       onScreenWindowsOnly:YES
                                                        completionHandler:^(SCShareableContent* content, NSError* error) {
                shareable = [content retain];
                setup_error = [error retain];
                dispatch_semaphore_signal(content_ready);
            }];

            NSString* failure = nil;
            if (!Wait(content_ready, 15))
            {
                failure = @"timed out while requesting screen-capture access";
            }
            else if (setup_error)
            {
                failure = [setup_error localizedDescription];
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
            AutomationBridgeRecordingDelegate* delegate = nil;
            uint32_t width = 0;
            uint32_t height = 0;
            if (!failure)
            {
                CGRect source_rect = CGRectMake(target->m_X, target->m_Y, target->m_Width, target->m_Height);
                CGFloat scale = target->m_Scale;
                if (!failure)
                {
                    width = requested_width ? requested_width : (uint32_t)llround(source_rect.size.width * scale);
                    height = requested_height ? requested_height : (uint32_t)llround(source_rect.size.height * scale);

                    SCStreamConfiguration* configuration = [[SCStreamConfiguration alloc] init];
                [configuration setWidth:width];
                [configuration setHeight:height];
                [configuration setSourceRect:source_rect];
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
                    stream = [[SCStream alloc] initWithFilter:filter configuration:configuration delegate:nil];
                [filter release];
                [configuration release];

                    NSString* output_path = [NSString stringWithUTF8String:path];
                NSURL* output_url = [NSURL fileURLWithPath:output_path];
                NSFileManager* files = [NSFileManager defaultManager];
                [files createDirectoryAtURL:[output_url URLByDeletingLastPathComponent]
                 withIntermediateDirectories:YES attributes:nil error:nil];
                [files removeItemAtURL:output_url error:nil];

                    SCRecordingOutputConfiguration* recording_configuration = [[SCRecordingOutputConfiguration alloc] init];
                [recording_configuration setOutputURL:output_url];
                [recording_configuration setOutputFileType:AVFileTypeMPEG4];
                [recording_configuration setVideoCodecType:AVVideoCodecTypeH264];
                    delegate = [[AutomationBridgeRecordingDelegate alloc] init];
                    output = [[SCRecordingOutput alloc] initWithConfiguration:recording_configuration delegate:delegate];
                [recording_configuration release];

                NSError* add_error = nil;
                    if (![stream addRecordingOutput:output error:&add_error])
                {
                    failure = [add_error localizedDescription];
                }
                }
            }

            if (!failure)
            {
                __block NSError* start_error = nil;
                dispatch_semaphore_t capture_started = dispatch_semaphore_create(0);
                [stream startCaptureWithCompletionHandler:^(NSError* error) {
                    start_error = [error retain];
                    dispatch_semaphore_signal(capture_started);
                }];
                if (!Wait(capture_started, 15))
                {
                    failure = @"timed out while starting screen capture";
                }
                else if (start_error)
                {
                    failure = [start_error localizedDescription];
                }
                [start_error release];
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
            [setup_error release];
            [shareable release];
            return failure == nil;
        }
    }

    bool StopNativeRecording(RecordingStatus* status)
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
            AutomationBridgeRecordingDelegate* delegate = [g_Delegate retain];
            [RecordingLock() unlock];

            NSString* failure = nil;
            NSError* remove_error = nil;
            if (![stream removeRecordingOutput:output error:&remove_error])
            {
                failure = [remove_error localizedDescription];
            }
            if (!failure && !Wait(delegate->m_Finished, 15))
            {
                failure = @"timed out while finalizing the MP4 file";
            }

            __block NSError* stop_error = nil;
            dispatch_semaphore_t capture_stopped = dispatch_semaphore_create(0);
            [stream stopCaptureWithCompletionHandler:^(NSError* error) {
                stop_error = [error retain];
                dispatch_semaphore_signal(capture_stopped);
            }];
            if (!Wait(capture_stopped, 15) && !failure)
            {
                failure = @"timed out while stopping screen capture";
            }
            else if (stop_error && !failure)
            {
                failure = [stop_error localizedDescription];
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
                attributesOfItemAtPath:[NSString stringWithUTF8String:g_Status.m_Path] error:nil];
            g_Status.m_Finalized = !failure && [[attributes objectForKey:NSFileSize] unsignedLongLongValue] > 0;
            if (failure)
            {
                SetFailure(&g_Status, failure);
            }
            ReleaseRecorderObjects();
            *status = g_Status;
            [RecordingLock() unlock];

            [stop_error release];
            [stream release];
            [output release];
            [delegate release];
            return failure == nil && status->m_Finalized;
        }
    }

    void FinalizeNativeRecording()
    {
        RecordingStatus status;
        GetNativeRecordingStatus(&status);
        if (status.m_Active)
        {
            StopNativeRecording(&status);
        }
    }
}

#endif
