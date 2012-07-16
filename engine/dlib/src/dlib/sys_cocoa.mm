#include <assert.h>
#include <stdint.h>
#import <Foundation/NSFileManager.h>
#include "log.h"
#include "sys.h"
#include "dstrings.h"

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
            [[app_support_dir path] getCString:path maxLength:path_len];

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
#endif
}

