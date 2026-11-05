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

#include "engine_service_darwin.h"

#include <string.h>
#include <dlib/dstrings.h>
#include <dlib/log.h>
#include <dmsdk/graphics/graphics.h>
#include <dmsdk/graphics/graphics_native.h>

#import <AppKit/AppKit.h>
#import <Foundation/Foundation.h>
#import <Metal/Metal.h>
#import <QuartzCore/CAMetalLayer.h>

namespace dmEngineService
{
    static void SendError(dmWebServer::Request* request, int status, const char* code, const char* message)
    {
        // Callers supply fixed error codes and messages. Native errors are serialized with capture status.
        char response[512];
        int length = dmSnPrintf(response, sizeof(response), "{\"ok\":false,\"error\":{\"status\":%d,\"code\":\"%s\",\"message\":\"%s\"}}", status, code, message);
        dmWebServer::SetStatusCode(request, status);
        dmWebServer::SendAttribute(request, "Content-Type", "application/json");
        dmWebServer::SendAttribute(request, "Cache-Control", "no-store");
        if (status == 405)
            dmWebServer::SendAttribute(request, "Allow", "GET, POST, DELETE");
        dmWebServer::Send(request, response, length);
    }

    static bool IsSupported()
    {
        if (dmGraphics::GetInstalledAdapterFamily() != dmGraphics::ADAPTER_FAMILY_METAL || !dmGraphics::GetInstalledContext())
            return false;

        if (@available(macOS 10.15, *))
            return [[MTLCaptureManager sharedCaptureManager] supportsDestination:MTLCaptureDestinationGPUTraceDocument];
        return false;
    }

    static const char* StateName(GPUTraceState state)
    {
        switch (state)
        {
            case GPU_TRACE_PENDING:
                return "pending";
            case GPU_TRACE_CAPTURING:
                return "capturing";
            case GPU_TRACE_COMPLETE:
                return "complete";
            case GPU_TRACE_CANCELED:
                return "canceled";
            case GPU_TRACE_FAILED:
                return "failed";
            default:
                return "idle";
        }
    }

    static void SendStatus(GPUTrace* trace, dmWebServer::Request* request)
    {
        NSMutableDictionary* status = [NSMutableDictionary dictionary];
        [status setObject:[NSString stringWithUTF8String:StateName(trace->m_State)] forKey:@"state"];
        [status setObject:[NSString stringWithUTF8String:trace->m_Path] forKey:@"path"];
        [status setObject:[NSNumber numberWithUnsignedInt:trace->m_Frames] forKey:@"frames"];
        [status setObject:[NSNumber numberWithUnsignedInt:trace->m_FramesCaptured] forKey:@"frames_captured"];
        [status setObject:[NSNumber numberWithBool:trace->m_StopRequested] forKey:@"stop_requested"];
        if (trace->m_Error[0])
        {
            NSString* error = [NSString stringWithUTF8String:trace->m_Error];
            [status setObject:error ? error : @"Metal capture failed" forKey:@"error"];
        }
        NSDictionary* response = [NSDictionary dictionaryWithObjectsAndKeys:
                                               [NSNumber numberWithBool:YES], @"ok", status, @"data", nil];
        NSData* json = [NSJSONSerialization dataWithJSONObject:response options:0 error:nil];
        dmWebServer::SetStatusCode(request, 200);
        dmWebServer::SendAttribute(request, "Content-Type", "application/json");
        dmWebServer::SendAttribute(request, "Cache-Control", "no-store");
        dmWebServer::Send(request, [json bytes], [json length]);
    }

    static bool ParseFrames(id value, uint32_t* frames)
    {
        if (!value)
        {
            *frames = 1;
            return true;
        }
        if (![value isKindOfClass:[NSString class]] &&
            (![value isKindOfClass:[NSNumber class]] || CFGetTypeID((CFTypeRef)value) == CFBooleanGetTypeID()))
            return false;

        const char* text = [[value description] UTF8String];
        uint32_t result = 0;
        for (const char* p = text; *p; ++p)
        {
            if (*p < '0' || *p > '9')
                return false;
            result = result * 10 + (*p - '0');
            if (result > 10000)
                return false;
        }
        if (result == 0)
            return false;
        *frames = result;
        return true;
    }

    static bool Schedule(GPUTrace* trace, dmWebServer::Request* request, const char* body, uint32_t body_size)
    {
        NSMutableDictionary* params = [NSMutableDictionary dictionary];
        if (body_size)
        {
            NSData* data = [NSData dataWithBytes:body length:body_size];
            id json = [NSJSONSerialization JSONObjectWithData:data options:0 error:nil];
            if (![json isKindOfClass:[NSDictionary class]])
            {
                SendError(request, 400, "bad_request", "provide a JSON object");
                return false;
            }
            [params addEntriesFromDictionary:json];
        }

        NSString* resource = [NSString stringWithUTF8String:request->m_Resource];
        NSURLComponents* url = resource ? [NSURLComponents componentsWithString:resource] : nil;
        if (!url)
        {
            SendError(request, 400, "bad_request", "invalid query parameters");
            return false;
        }
        // Query parameters take precedence over body fields, as in the bridge endpoint.
        url.percentEncodedQuery = [[url percentEncodedQuery] stringByReplacingOccurrencesOfString:@"+" withString:@"%20"];
        for (NSURLQueryItem* item in [[url queryItems] reverseObjectEnumerator])
            [params setObject:[item value] ? [item value] : @"" forKey:[item name]];

        id path = [params objectForKey:@"path"];
        if (![path isKindOfClass:[NSString class]] || ![path hasPrefix:@"/"] || ![path hasSuffix:@".gputrace"])
        {
            SendError(request, 400, "bad_request", "provide an absolute path ending in .gputrace");
            return false;
        }
        NSUInteger path_size = [path lengthOfBytesUsingEncoding:NSUTF8StringEncoding];
        const char* path_text = [path UTF8String];
        if (memchr(path_text, 0, path_size))
        {
            SendError(request, 400, "bad_request", "path must not contain a null character");
            return false;
        }
        if (path_size >= sizeof(trace->m_Path))
        {
            SendError(request, 414, "gputrace_path_too_long", "GPU trace path is too long");
            return false;
        }
        uint32_t frames;
        if (!ParseFrames([params objectForKey:@"frames"], &frames))
        {
            SendError(request, 400, "bad_request", "frames must be a positive integer no greater than 10000");
            return false;
        }
        if (trace->m_State == GPU_TRACE_PENDING || trace->m_State == GPU_TRACE_CAPTURING)
        {
            SendError(request, 409, "gputrace_active", "a GPU trace is already pending or active");
            return false;
        }

        dmStrlCpy(trace->m_Path, path_text, sizeof(trace->m_Path));
        trace->m_Frames = frames;
        trace->m_FramesCaptured = 0;
        trace->m_StopRequested = false;
        trace->m_Error[0] = 0;
        trace->m_State = GPU_TRACE_PENDING;
        return true;
    }

    void GPUTraceRequest(void* context, dmWebServer::Request* request)
    {
        const uint32_t max_request_size = 64 * 1024;
        dmArray<char> body;
        dmWebServer::Result received = dmWebServer::RESULT_ERROR_INVAL;
        if (request->m_ContentLength < max_request_size)
        {
            body.SetCapacity(request->m_ContentLength);
            received = dmWebServer::ReceiveBuffered(request, body.Begin(), body.Capacity());
        }
        if (received == dmWebServer::RESULT_TRY_AGAIN)
            return;
        if (received == dmWebServer::RESULT_ERROR_INVAL)
        {
            // Do not block the engine waiting for a body that will be rejected.
            dmWebServer::CloseConnection(request);
            SendError(request, 413, "request_too_large", "request headers and body must be smaller than 65536 bytes");
            return;
        }
        if (received != dmWebServer::RESULT_OK)
        {
            dmWebServer::CloseConnection(request);
            SendError(request, 400, "bad_request", "could not read request body");
            return;
        }

        const char* suffix = request->m_Resource + strlen("/profiler/gputrace");
        if (*suffix != 0 && *suffix != '?')
        {
            SendError(request, 404, "not_found", "unknown GPU trace endpoint");
            return;
        }
        if (!dmStrEq(request->m_Method, "GET") && !dmStrEq(request->m_Method, "POST") && !dmStrEq(request->m_Method, "DELETE"))
        {
            SendError(request, 405, "method_not_allowed", "use GET, POST or DELETE");
            return;
        }
        GPUTrace* trace = (GPUTrace*)context;
        @autoreleasepool
        {
            if (!IsSupported())
            {
                SendError(request, 501, "gputrace_unsupported", "GPU trace capture requires macOS, Metal and METAL_CAPTURE_ENABLED=1");
                return;
            }
            if (dmStrEq(request->m_Method, "POST") && !Schedule(trace, request, body.Begin(), request->m_ContentLength))
                return;
            if (dmStrEq(request->m_Method, "DELETE"))
            {
                if (trace->m_State == GPU_TRACE_PENDING)
                    trace->m_State = GPU_TRACE_CANCELED;
                else if (trace->m_State == GPU_TRACE_CAPTURING)
                    trace->m_StopRequested = true;
                else
                {
                    SendError(request, 409, "gputrace_inactive", "no GPU trace is pending or active");
                    return;
                }
            }
            SendStatus(trace, request);
        }
    }

    static void Fail(GPUTrace* trace, const char* error)
    {
        dmStrlCpy(trace->m_Error, error, sizeof(trace->m_Error));
        trace->m_State = GPU_TRACE_FAILED;
        trace->m_StopRequested = false;
    }

    void GPUTraceBeginFrame(GPUTrace* trace)
    {
        if (trace->m_State != GPU_TRACE_PENDING)
            return;

        @autoreleasepool
        {
            NSWindow* window = (NSWindow*)dmGraphics::GetNativeOSXNSWindow();
            NSView* view = [window contentView];
            CAMetalLayer* layer = [[view layer] isKindOfClass:[CAMetalLayer class]] ? (CAMetalLayer*)[view layer] : nil;
            id<MTLDevice> device = [layer device];
            if (!device)
            {
                Fail(trace, "Defold Metal layer device is unavailable");
                return;
            }

            MTLCaptureDescriptor* descriptor = [[MTLCaptureDescriptor alloc] init];
            descriptor.captureObject = device;
            descriptor.destination = MTLCaptureDestinationGPUTraceDocument;
            descriptor.outputURL = [NSURL fileURLWithPath:[NSString stringWithUTF8String:trace->m_Path]];
            NSError* error = nil;
            BOOL started = [[MTLCaptureManager sharedCaptureManager] startCaptureWithDescriptor:descriptor error:&error];
            [descriptor release];
            if (!started)
            {
                Fail(trace, error ? [[error localizedDescription] UTF8String] : "Metal capture could not be started");
                return;
            }
            trace->m_State = GPU_TRACE_CAPTURING;
            dmLogInfo("GPU trace started: '%s' (%u frames)", trace->m_Path, trace->m_Frames);
        }
    }

    void GPUTraceEndFrame(GPUTrace* trace)
    {
        if (trace->m_State != GPU_TRACE_CAPTURING)
            return;

        ++trace->m_FramesCaptured;
        if (trace->m_StopRequested || trace->m_FramesCaptured >= trace->m_Frames)
            GPUTraceStop(trace);
    }

    void GPUTraceStop(GPUTrace* trace)
    {
        if (trace->m_State == GPU_TRACE_CAPTURING)
        {
            [[MTLCaptureManager sharedCaptureManager] stopCapture];
            trace->m_State = GPU_TRACE_COMPLETE;
            dmLogInfo("GPU trace completed: '%s'", trace->m_Path);
        }
        else if (trace->m_State == GPU_TRACE_PENDING)
        {
            trace->m_State = GPU_TRACE_CANCELED;
        }
        trace->m_StopRequested = false;
    }
} // namespace dmEngineService
