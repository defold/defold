#include "log.h"

#import <Foundation/Foundation.h>

void __ios_log_print(dmLogSeverity severity, const char* str_buf)
{
    NSLog(@"%@", @(str_buf));
}
