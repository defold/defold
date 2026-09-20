// Copyright 2020-2026 The Defold Foundation
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

#ifndef DM_MDNS_PRIVATE_H
#define DM_MDNS_PRIVATE_H

#include "mdns.h"

namespace dmMDNS
{
    // Update with an explicit microsecond clock for deterministic timer tests.
    // Time must not go backwards for a browser. Returns whether a query was produced.
    bool UpdateBrowser(HBrowser browser, uint64_t now);
} // namespace dmMDNS

#endif // DM_MDNS_PRIVATE_H
