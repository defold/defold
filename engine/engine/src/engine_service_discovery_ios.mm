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
#include <dlib/time.h>

#include "engine_service_discovery.h"
#include "engine_service_private.h"

namespace dmEngineService
{
    struct DiscoveryService;
    static void DiscoveryServiceDidPublish(DiscoveryService* discovery_service, NSNetService* sender);
    static void DiscoveryServiceDidNotPublish(DiscoveryService* discovery_service, NSNetService* sender, NSDictionary* errorDict);
}

@interface DefoldDiscoveryServiceDelegate : NSObject<NSNetServiceDelegate>
{
@public
    dmEngineService::DiscoveryService* m_DiscoveryService;
}
- (id)initWithDiscoveryService:(dmEngineService::DiscoveryService*)discovery_service;
@end

@implementation DefoldDiscoveryServiceDelegate

- (id)initWithDiscoveryService:(dmEngineService::DiscoveryService*)discovery_service
{
    self = [super init];
    if (self)
    {
        m_DiscoveryService = discovery_service;
    }
    return self;
}

- (void)netServiceDidPublish:(NSNetService*)sender
{
    dmEngineService::DiscoveryServiceDidPublish(m_DiscoveryService, sender);
}

- (void)netService:(NSNetService*)sender didNotPublish:(NSDictionary*)errorDict
{
    dmEngineService::DiscoveryServiceDidNotPublish(m_DiscoveryService, sender, errorDict);
}

@end

namespace dmEngineService
{
    static NSString* const DISCOVERY_DOMAIN = @"local.";
    // NSNetService expects the service type as a regtype label ending with '.'
    // while the domain is provided separately. On the wire this still becomes
    // the same "_defold._tcp.local" service type consumed by MDNS.java and the
    // native mdns.cpp browser.
    static NSString* const DISCOVERY_SERVICE_TYPE = @"_defold._tcp.";

    enum DiscoveryPublishState
    {
        DISCOVERY_PUBLISH_STATE_PENDING,
        DISCOVERY_PUBLISH_STATE_PUBLISHED,
        DISCOVERY_PUBLISH_STATE_FAILED,
    };

    struct DiscoveryService
    {
        DiscoveryService()
        : m_Service(0)
        , m_Delegate(0)
        , m_ServiceName(0)
        , m_TxtRecordData(0)
        , m_Port(0)
        , m_PublishState(DISCOVERY_PUBLISH_STATE_PENDING)
        , m_FailureCount(0)
        , m_NextRetryTime(0)
        {
        }

        NSNetService*                    m_Service;
        DefoldDiscoveryServiceDelegate* m_Delegate;
        NSString*                        m_ServiceName;
        NSData*                          m_TxtRecordData;
        uint16_t                         m_Port;
        DiscoveryPublishState            m_PublishState;
        uint32_t                         m_FailureCount;
        uint64_t                         m_NextRetryTime;
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
            return [NSData data];

        NSString* string_value = ToNSString(value);
        if (string_value == nil)
            return nil;

        return [string_value dataUsingEncoding:NSUTF8StringEncoding];
    }

    static NSRunLoop* DiscoveryRunLoop()
    {
        return [NSRunLoop mainRunLoop];
    }

    static void ScheduleDiscoveryService(NSNetService* service)
    {
        if (service == nil)
            return;

        NSRunLoop* run_loop = DiscoveryRunLoop();

        // initWithDomain:type:name:port: implicitly attaches the service to the
        // current run loop's default mode. Remove that registration before
        // rescheduling in common modes so teardown stays explicit and the editor
        // browsers keep seeing the same _defold._tcp.local service/TXT contract.
        [service removeFromRunLoop:run_loop forMode:NSDefaultRunLoopMode];
        [service scheduleInRunLoop:run_loop forMode:NSRunLoopCommonModes];
    }

    static void UnscheduleDiscoveryService(NSNetService* service)
    {
        if (service == nil)
            return;

        NSRunLoop* run_loop = DiscoveryRunLoop();
        [service removeFromRunLoop:run_loop forMode:NSRunLoopCommonModes];
        [service removeFromRunLoop:run_loop forMode:NSDefaultRunLoopMode];
    }

    static void ReleaseBonjourService(DiscoveryService* discovery_service)
    {
        if (discovery_service == 0 || discovery_service->m_Service == 0)
            return;

        [discovery_service->m_Service stop];
        UnscheduleDiscoveryService(discovery_service->m_Service);
        [discovery_service->m_Service setDelegate:nil];
        [discovery_service->m_Service release];
        discovery_service->m_Service = 0;
    }

    static void ReleaseRetainedDiscoveryObjects(DiscoveryService* discovery_service)
    {
        if (discovery_service == 0)
            return;

        ReleaseBonjourService(discovery_service);

        if (discovery_service->m_Delegate)
        {
            [discovery_service->m_Delegate release];
            discovery_service->m_Delegate = 0;
        }

        if (discovery_service->m_ServiceName)
        {
            [discovery_service->m_ServiceName release];
            discovery_service->m_ServiceName = 0;
        }

        if (discovery_service->m_TxtRecordData)
        {
            [discovery_service->m_TxtRecordData release];
            discovery_service->m_TxtRecordData = 0;
        }
    }

    static bool CreateBonjourService(DiscoveryService* discovery_service)
    {
        NSNetService* service = [[NSNetService alloc] initWithDomain:DISCOVERY_DOMAIN
                                                                type:DISCOVERY_SERVICE_TYPE
                                                                name:discovery_service->m_ServiceName
                                                                port:(int) discovery_service->m_Port];
        if (service == nil)
        {
            dmLogWarning("Unable to create Bonjour service");
            return false;
        }

        if (discovery_service->m_TxtRecordData != nil && ![service setTXTRecordData:discovery_service->m_TxtRecordData])
        {
            dmLogWarning("Unable to set Bonjour TXT record data");
            [service release];
            return false;
        }

        [service setDelegate:discovery_service->m_Delegate];
        ScheduleDiscoveryService(service);
        discovery_service->m_Service = service;
        return true;
    }

    static bool PublishBonjourService(DiscoveryService* discovery_service)
    {
        if (!CreateBonjourService(discovery_service))
            return false;

        discovery_service->m_PublishState = DISCOVERY_PUBLISH_STATE_PENDING;
        [discovery_service->m_Service publish];
        return true;
    }

    static void SchedulePublishRetry(DiscoveryService* discovery_service, NSNumber* error_domain, NSNumber* error_code)
    {
        const uint32_t failure_count = discovery_service->m_FailureCount++;
        const uint64_t retry_delay = GetDiscoveryRetryDelayUsec(failure_count);

        discovery_service->m_PublishState = DISCOVERY_PUBLISH_STATE_FAILED;
        discovery_service->m_NextRetryTime = dmTime::GetMonotonicTime() + retry_delay;

        dmLogWarning("Unable to publish Bonjour service '%s' (%ld/%ld), retrying in %.1f seconds",
                     [discovery_service->m_ServiceName UTF8String],
                     (long) [error_domain integerValue],
                     (long) [error_code integerValue],
                     (double) retry_delay / 1000000.0);
    }

    static void DiscoveryServiceDidPublish(DiscoveryService* discovery_service, NSNetService* sender)
    {
        if (discovery_service == 0 || sender != discovery_service->m_Service)
            return;

        discovery_service->m_PublishState = DISCOVERY_PUBLISH_STATE_PUBLISHED;
        discovery_service->m_FailureCount = 0;
        discovery_service->m_NextRetryTime = 0;

        dmLogInfo("Published Bonjour service '%s'", [[sender name] UTF8String]);
    }

    static void DiscoveryServiceDidNotPublish(DiscoveryService* discovery_service, NSNetService* sender, NSDictionary* errorDict)
    {
        if (discovery_service == 0 || sender != discovery_service->m_Service)
            return;

        NSNumber* error_code = [errorDict objectForKey:NSNetServicesErrorCode];
        NSNumber* error_domain = [errorDict objectForKey:NSNetServicesErrorDomain];
        SchedulePublishRetry(discovery_service, error_domain, error_code);
    }

    HDiscoveryService DiscoveryServiceNew(const char* service_id, const char* instance_name, uint16_t port, const DiscoveryTxtEntry* txt_entries, uint32_t txt_count)
    {
        if (service_id == 0 || instance_name == 0 || port == 0 || txt_entries == 0)
            return 0;

        if (txt_count > DISCOVERY_MAX_TXT_ENTRIES)
        {
            dmLogWarning("Unable to publish Bonjour service, too many TXT entries (%u)", txt_count);
            return 0;
        }

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

                // Keep the TXT key/value contract aligned with the native
                // mdns.cpp path so both discovery browsers derive the same id,
                // display name, log port, and schema from the wire records.
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
                discovery_service->m_ServiceName = [service_name retain];
                discovery_service->m_TxtRecordData = [txt_record_data retain];
                discovery_service->m_Port = port;

                DefoldDiscoveryServiceDelegate* delegate = [[DefoldDiscoveryServiceDelegate alloc] initWithDiscoveryService:discovery_service];
                discovery_service->m_Delegate = delegate;

                if (PublishBonjourService(discovery_service))
                {
                    created = true;
                }
            }
        });

        if (!created)
        {
            ReleaseRetainedDiscoveryObjects(discovery_service);
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
                ReleaseRetainedDiscoveryObjects(discovery_service);
            }
        });

        delete discovery_service;
    }

    void DiscoveryServiceUpdate(HDiscoveryService discovery_service)
    {
        if (discovery_service == 0)
            return;

        RunOnMainThreadSync(^{
            @autoreleasepool
            {
                // Drive retries from the engine update loop so teardown does not
                // have to coordinate with delayed main-queue publish callbacks.
                if (discovery_service->m_PublishState != DISCOVERY_PUBLISH_STATE_FAILED)
                    return;

                const uint64_t now = dmTime::GetMonotonicTime();
                if (now < discovery_service->m_NextRetryTime)
                    return;

                ReleaseBonjourService(discovery_service);

                if (PublishBonjourService(discovery_service))
                {
                    dmLogInfo("Retrying Bonjour service publish '%s'", [discovery_service->m_ServiceName UTF8String]);
                }
                else
                {
                    SchedulePublishRetry(discovery_service,
                                         [NSNumber numberWithInteger:NSNetServicesUnknownError],
                                         [NSNumber numberWithInteger:0]);
                }
            }
        });
    }
}

#endif // DM_PLATFORM_IOS
