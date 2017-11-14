#include <stdint.h>
#include <unistd.h>
#include "../dlib/sys.h"

#if defined(__arm__) || defined(__arm64__) || defined(IOS_SIMULATOR)
#import <UIKit/UIKit.h>
#else
#import <Cocoa/Cocoa.h>
#endif

int main(int argc, char **argv)
{
    // NOTE: This example doesn't work on iOS. Is UIApplication main required?
    NSAutoreleasePool*pool = [[NSAutoreleasePool alloc] init];
    dmSys::OpenURL("http://www.google.com");
    [pool drain];
}
