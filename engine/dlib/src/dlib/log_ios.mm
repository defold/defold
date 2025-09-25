// Copyright 2020-2025 The Defold Foundation
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

#include "log.h"

#import <Foundation/Foundation.h>
#import <os/log.h>

namespace dmLog
{

void __ios_log_print(LogSeverity severity, const char* str_buf)
{
    if (@available(iOS 10, *))
    {
        // https://developer.apple.com/documentation/os/os_log_type_t/os_log_type_default?language=objc
        os_log_type_t log_type;
        switch (severity)
        {
            case LOG_SEVERITY_DEBUG:
            case LOG_SEVERITY_USER_DEBUG:
                log_type = OS_LOG_TYPE_DEBUG;
                break;

            case LOG_SEVERITY_INFO:
                log_type = OS_LOG_TYPE_INFO;
                break;

            case LOG_SEVERITY_WARNING:
                log_type = OS_LOG_TYPE_DEFAULT;
                break;

            case LOG_SEVERITY_ERROR:
                log_type = OS_LOG_TYPE_ERROR;
                break;

            case LOG_SEVERITY_FATAL:
                log_type = OS_LOG_TYPE_FAULT;
                break;

            default:
                log_type = OS_LOG_TYPE_INFO;
                break;
        }
        os_log_with_type(OS_LOG_DEFAULT, log_type, "%s", str_buf);
    }
    else
    {
        NSLog(@"%@", @(str_buf));
    }
    
}

} //namespace dmLog
