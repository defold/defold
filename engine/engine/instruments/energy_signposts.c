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

#include <os/log.h>
#include <os/signpost.h>
#include <stdio.h>

// The capture runner supplies normalized powermetrics samples over stdin.
// This separate process lets Instruments collect power alongside the target's
// annotations without adding an energy sampler to the engine.
int main(void)
{
    os_log_t           log = os_log_create("com.defold.energy", OS_LOG_CATEGORY_POINTS_OF_INTEREST);
    const char*        components[] = { "CPU", "GPU", "ANE" };
    char               kind;
    unsigned int       index;
    double             value;
    unsigned long long elapsed_ns;
    int                fields;
    // Acknowledgement prevents the runner from resuming the game before this
    // process has initialized its signpost channel.
    fputs("R", stdout);
    fflush(stdout);
    while ((fields = scanf(" %c %u %lf %llu", &kind, &index, &value, &elapsed_ns)) == 4)
    {
        if (kind == 'S' && index < sizeof(components) / sizeof(components[0]))
            os_signpost_event_emit(log, OS_SIGNPOST_ID_EXCLUSIVE, "System Power", "component: %{public}s, watts: %f, window: %llu ns", components[index], value, elapsed_ns);
        else if (kind == 'A')
            os_signpost_event_emit(log, OS_SIGNPOST_ID_EXCLUSIVE, "Application Energy", "pid: %u, impact per second: %f, window: %llu ns", index, value, elapsed_ns);
        else
            return 1;
    }
    os_release(log);
    return fields == EOF ? 0 : 1;
}
