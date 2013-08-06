#include <assert.h>
#include <stdint.h>
#include <sys/utsname.h>
#import <Foundation/NSFileManager.h>
#include "log.h"
#include "sys.h"
#include "sys_private.h"
#include "dstrings.h"

#ifdef __arm__
#import <UIKit/UIApplication.h>
#import <UIKit/UIKit.h>
#else
#import <AppKit/NSWorkspace.h>
#endif

namespace dmSys
{
#ifdef __arm__
    // Only on iOS for now. No autorelease pool etc setup on OSX in dlib
    Result GetApplicationSupportPath(const char* application_name, char* path, uint32_t path_len)
    {
    	assert(path_len > 0);
        NSFileManager* shared_fm = [NSFileManager defaultManager];
        NSArray* urls = [shared_fm URLsForDirectory:NSApplicationSupportDirectory
                                          inDomains:NSUserDomainMask];


        if ([urls count] > 0)
        {
            NSURL* app_support_dir = [urls objectAtIndex:0];
            dmStrlCpy(path, [[app_support_dir path] UTF8String], path_len);

            path[path_len-1] = '\0';

            if (dmStrlCat(path, "/", path_len) >= path_len)
                return RESULT_INVAL;
            if (dmStrlCat(path, application_name, path_len) >= path_len)
                return RESULT_INVAL;

            NSString* ns_path = [NSString stringWithUTF8String: path];
            Result r;
            if ([shared_fm createDirectoryAtPath: ns_path withIntermediateDirectories: TRUE attributes: nil error: nil] == YES)
            {
                r = RESULT_OK;
            }
            else
            {
                r = RESULT_UNKNOWN;
            }
            [ns_path release];
            return r;
        }
        else
        {
        	dmLogError("Unable to locate NSApplicationSupportDirectory");
            return RESULT_UNKNOWN;
        }
    }

    Result OpenURL(const char* url)
    {
        NSString* ns_url = [NSString stringWithUTF8String: url];
        BOOL ret = [[UIApplication sharedApplication] openURL:[NSURL URLWithString: ns_url]];
        if (ret == YES)
        {
            return RESULT_OK;
        }
        else
        {
            return RESULT_UNKNOWN;
        }
    }

    void GetSystemInfo(struct SystemInfo* info)
    {
        UIDevice* d = [UIDevice currentDevice];
        struct utsname uts;
        uname(&uts);

        dmStrlCpy(info->m_DeviceModel, uts.machine, sizeof(info->m_DeviceModel));
        dmStrlCpy(info->m_SystemName, [d.systemName UTF8String], sizeof(info->m_SystemName));
        dmStrlCpy(info->m_SystemVersion, [d.systemVersion UTF8String], sizeof(info->m_SystemVersion));

        NSLocale* locale = [NSLocale currentLocale];
        const char* lang = [locale.localeIdentifier UTF8String];
        FillLanguageTerritory(lang, info);
    }

#else

    Result OpenURL(const char* url)
    {
        NSString* ns_url = [NSString stringWithUTF8String: url];
        BOOL ret = [[NSWorkspace sharedWorkspace] openURL:[NSURL URLWithString: ns_url]];
        if (ret == YES)
        {
            return RESULT_OK;
        }
        else
        {
            return RESULT_UNKNOWN;
        }
    }

#endif
}

