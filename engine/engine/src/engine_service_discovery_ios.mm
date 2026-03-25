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

#if defined(DM_PLATFORM_IOS)

#import <Foundation/Foundation.h>
#include <dispatch/dispatch.h>

#include <dlib/log.h>

#include "engine_service_discovery.h"

@interface DefoldDiscoveryServiceDelegate : NSObject<NSNetServiceDelegate>
@end

@implementation DefoldDiscoveryServiceDelegate

- (void)netServiceDidPublish:(NSNetService*)sender
{
    dmLogInfo("Published Bonjour service '%s'", [[sender name] UTF8String]);
}

- (void)netService:(NSNetService*)sender didNotPublish:(NSDictionary*)errorDict
{
    NSNumber* error_code = [errorDict objectForKey:NSNetServicesErrorCode];
    NSNumber* error_domain = [errorDict objectForKey:NSNetServicesErrorDomain];
    dmLogWarning("Unable to publish Bonjour service '%s' (%ld/%ld)",
                 [[sender name] UTF8String],
                 (long) [error_domain integerValue],
                 (long) [error_code integerValue]);
}

@end

namespace dmEngineService
{
    static NSString* const DISCOVERY_DOMAIN = @"local.";
    static NSString* const DISCOVERY_SERVICE_TYPE = @"_defold._tcp.";

    struct DiscoveryService
    {
        DiscoveryService()
        : m_Service(0)
        , m_Delegate(0)
        {
        }

        NSNetService*                 m_Service;
        DefoldDiscoveryServiceDelegate* m_Delegate;
    };

    static void RunOnMainThreadSync(dispatch_block_t block)
    {
        // NSNetService is bound to the run loop it is scheduled on, so create,
        // publish, stop, and teardown all on the main run loop.
        if ([NSThread isMainThread])
        {
            block();
        }
        else
        {
            dispatch_sync(dispatch_get_main_queue(), block);
        }
    }

    static NSString* ToNSString(const char* value)
    {
        return value ? [NSString stringWithUTF8String:value] : nil;
    }

    static NSData* ToUTF8Data(const char* value)
    {
        if (value == 0)
            return nil;

        NSString* string_value = ToNSString(value);
        if (string_value == nil)
            return nil;

        return [string_value dataUsingEncoding:NSUTF8StringEncoding];
    }

    HDiscoveryService DiscoveryServiceNew(const char* service_id, const char* instance_name, uint16_t port, const DiscoveryTxtEntry* txt_entries, uint32_t txt_count)
    {
        // Bonjour registration on iOS is handled by NSNetService; the stable
        // Defold identity still travels in the TXT payload built by the caller.
        (void) service_id;

        if (instance_name == 0 || port == 0 || txt_entries == 0)
            return 0;

        DiscoveryService* discovery_service = new DiscoveryService();
        __block bool created = false;

        RunOnMainThreadSync(^{
            @autoreleasepool
            {
                NSString* service_name = ToNSString(instance_name);
                if (service_name == nil)
                {
                    dmLogWarning("Unable to publish Bonjour service, invalid instance name");
                    return;
                }

                NSNetService* service = [[NSNetService alloc] initWithDomain:DISCOVERY_DOMAIN
                                                                        type:DISCOVERY_SERVICE_TYPE
                                                                        name:service_name
                                                                        port:(int) port];
                if (service == nil)
                {
                    dmLogWarning("Unable to create Bonjour service");
                    return;
                }

                NSMutableDictionary* txt_record_dict = [NSMutableDictionary dictionaryWithCapacity:txt_count];
                for (uint32_t i = 0; i < txt_count; ++i)
                {
                    NSString* key = ToNSString(txt_entries[i].m_Key);
                    NSData* value = ToUTF8Data(txt_entries[i].m_Value);
                    if (key != nil && value != nil)
                    {
                        [txt_record_dict setObject:value forKey:key];
                    }
                }

                NSData* txt_record_data = [NSNetService dataFromTXTRecordDictionary:txt_record_dict];
                if (txt_record_data != nil && ![service setTXTRecordData:txt_record_data])
                {
                    dmLogWarning("Unable to set Bonjour TXT record data");
                    [service release];
                    return;
                }

                DefoldDiscoveryServiceDelegate* delegate = [[DefoldDiscoveryServiceDelegate alloc] init];
                [service setDelegate:delegate];
                [service scheduleInRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
                [service publish];

                discovery_service->m_Service = service;
                discovery_service->m_Delegate = delegate;
                created = true;
            }
        });

        if (!created)
        {
            delete discovery_service;
            return 0;
        }

        return discovery_service;
    }

    void DiscoveryServiceDelete(HDiscoveryService discovery_service)
    {
        if (discovery_service == 0)
            return;

        RunOnMainThreadSync(^{
            @autoreleasepool
            {
                if (discovery_service->m_Service)
                {
                    [discovery_service->m_Service stop];
                    [discovery_service->m_Service removeFromRunLoop:[NSRunLoop mainRunLoop] forMode:NSRunLoopCommonModes];
                    [discovery_service->m_Service setDelegate:nil];
                    [discovery_service->m_Service release];
                }

                if (discovery_service->m_Delegate)
                {
                    [discovery_service->m_Delegate release];
                }
            }
        });

        delete discovery_service;
    }

    void DiscoveryServiceUpdate(HDiscoveryService discovery_service)
    {
        (void) discovery_service;
    }
}

#endif // DM_PLATFORM_IOS
