
#include "output.h"
#import <Foundation/Foundation.h>

void DebugOutput::print(const char *message)
{
    NSLog(@"%s", message);
}
