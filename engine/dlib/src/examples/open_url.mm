#include <stdint.h>
#include <unistd.h>
#include "../dlib/sys.h"

#ifdef __arm__
#import <UIKit/UIKit.h>
#else
#import <Cocoa/Cocoa.h>
#endif

int main(int argc, char **argv)
{
    NSAutoreleasePool*pool = [[NSAutoreleasePool alloc] init];
    dmSys::OpenURL("http://www.google.com");

    for (int i = 0; i < 10; ++i)
    {
        CFRunLoopRunInMode(kCFRunLoopDefaultMode, 0, TRUE);
        usleep(1000 * 1000);

    }

    [pool drain];
}
