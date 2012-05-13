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
            Result r =  Mkdir(path, 0755);
            if (r == RESULT_EXIST)
                return RESULT_OK;
            else
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

