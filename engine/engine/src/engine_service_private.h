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

#ifndef DM_ENGINE_SERVICE_PRIVATE_H
#define DM_ENGINE_SERVICE_PRIVATE_H

#include <stdint.h>
#include <string.h>

#include <dlib/dstrings.h>

namespace dmEngineService
{
    // DNS-SD instance names are a single DNS label (RFC 6763, Section 4.1.1),
    // so they inherit the DNS label limit of 63 octets from RFC 1035, Section 2.3.4.
    static const uint32_t MDNS_MAX_LABEL_LENGTH = 63;

    static inline void SanitizeMDNSLabel(const char* value, const char* fallback, bool allow_leading_dash, char leading_dash_replacement, char* out, uint32_t out_size)
    {
        if (out_size == 0)
            return;

        const char* source = (value && value[0]) ? value : fallback;
        uint32_t offset = 0;
        bool last_was_dash = !allow_leading_dash;
        while (*source && offset + 1 < out_size)
        {
            char c = *source++;
            if (c >= 'A' && c <= 'Z')
                c = (char) (c - 'A' + 'a');

            const bool is_alnum = (c >= 'a' && c <= 'z') || (c >= '0' && c <= '9');
            if (is_alnum)
            {
                out[offset++] = c;
                last_was_dash = false;
            }
            else if (!last_was_dash)
            {
                out[offset++] = '-';
                last_was_dash = true;
            }
        }

        while (offset > 0 && out[offset - 1] == '-')
            --offset;

        if (offset == 0)
        {
            dmStrlCpy(out, fallback, out_size);
            return;
        }

        // Some callers preserve a leading separator while sanitizing, but DNS
        // host labels still need to start with an alphanumeric character.
        if (leading_dash_replacement != 0 && out[0] == '-')
            out[0] = leading_dash_replacement;

        out[offset] = 0;
    }

    static inline bool IsLoopbackDiscoveryIdentity(const char* value)
    {
        return value == 0
            || value[0] == 0
            || strcmp(value, "localhost") == 0
            || strcmp(value, "127.0.0.1") == 0
            || strcmp(value, "::1") == 0;
    }

    static inline void BuildManufacturerModelName(const char* manufacturer, const char* device_model, char* out, uint32_t out_size)
    {
        if (out_size == 0)
            return;

        out[0] = 0;

        if (manufacturer && manufacturer[0])
            dmStrlCpy(out, manufacturer, out_size);

        if (device_model && device_model[0])
        {
            if (out[0])
                dmStrlCat(out, "-", out_size);
            dmStrlCat(out, device_model, out_size);
        }
    }

    static inline void BuildDiscoveryIdentity(const char* local_address, const char* hostname, const char* manufacturer, const char* device_model, const char* system_name, char* out, uint32_t out_size)
    {
        if (out_size == 0)
            return;

        out[0] = 0;

        if (!IsLoopbackDiscoveryIdentity(local_address))
        {
            dmStrlCpy(out, local_address, out_size);
            return;
        }

        if (hostname && hostname[0] && strcmp(hostname, "localhost") != 0)
        {
            dmStrlCpy(out, hostname, out_size);
            return;
        }

        BuildManufacturerModelName(manufacturer, device_model, out, out_size);
        if (out[0])
        {
            return;
        }

        dmStrlCpy(out, (system_name && system_name[0]) ? system_name : "defold", out_size);
    }

    static inline void BuildServiceInstanceName(const char* local_address, const char* port_text, const char* suffix_text, char* out, uint32_t out_size)
    {
        if (out_size == 0)
            return;

        char sanitized_address[MDNS_MAX_LABEL_LENGTH + 1];
        SanitizeMDNSLabel(local_address, "localhost", false, 0, sanitized_address, sizeof(sanitized_address));

        static const char prefix[] = "defold";
        const uint32_t prefix_length = (uint32_t) strlen(prefix);
        uint32_t max_label_length = out_size - 1;
        if (max_label_length > MDNS_MAX_LABEL_LENGTH)
            max_label_length = MDNS_MAX_LABEL_LENGTH;

        uint32_t port_component_length = 0;
        if (port_text && port_text[0])
        {
            port_component_length = 1 + (uint32_t) strlen(port_text);
            if (prefix_length + port_component_length > max_label_length)
                port_component_length = 0;
        }

        // The startup suffix is formatted as 8 lowercase hex digits.
        const uint32_t suffix_component_length = (suffix_text && suffix_text[0]) ? 9 : 0;

        // Spend any remaining label budget on the address and drop it entirely
        // before we would emit a dangling "defold-" separator.
        uint32_t max_address_length = 0;
        if (prefix_length + port_component_length + suffix_component_length + 1 < max_label_length)
            max_address_length = max_label_length - prefix_length - port_component_length - suffix_component_length - 1;

        if ((uint32_t) strlen(sanitized_address) > max_address_length)
            sanitized_address[max_address_length] = 0;

        char address_component[MDNS_MAX_LABEL_LENGTH + 2];
        address_component[0] = 0;
        if (sanitized_address[0])
            dmSnPrintf(address_component, sizeof(address_component), "-%s", sanitized_address);

        char port_component[MDNS_MAX_LABEL_LENGTH + 2];
        port_component[0] = 0;
        if (port_component_length != 0)
            dmSnPrintf(port_component, sizeof(port_component), "-%s", port_text);

        char suffix_component[MDNS_MAX_LABEL_LENGTH + 2];
        suffix_component[0] = 0;
        if (suffix_component_length != 0)
            dmSnPrintf(suffix_component, sizeof(suffix_component), "-%s", suffix_text);

        dmSnPrintf(out, out_size, "%s%s%s%s", prefix, address_component, port_component, suffix_component);
    }
}

#endif // DM_ENGINE_SERVICE_PRIVATE_H
