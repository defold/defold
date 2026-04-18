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

package com.dynamo.discovery;

import java.util.Collections;
import java.util.Map;

public record MDNSServiceInfo(long expires,
                              String id,
                              String instanceName,
                              String serviceName,
                              String host,
                              String address,
                              String localAddress,
                              int port,
                              String logPort,
                              Map<String, String> txt) {

    public MDNSServiceInfo {
        txt = Collections.unmodifiableMap(txt);
    }

    // Equality is used by MDNS.rebuildDiscovered() when comparing the next
    // discovery snapshot against the cached map. id, instanceName, and logPort
    // are derived from txt/serviceName, while expires is expected to change as
    // records are refreshed.
    @Override
    public int hashCode() {
        int result = serviceName.hashCode();
        result = 31 * result + address.hashCode();
        result = 31 * result + (host != null ? host.hashCode() : 0);
        result = 31 * result + (localAddress != null ? localAddress.hashCode() : 0);
        result = 31 * result + port;
        result = 31 * result + txt.hashCode();
        return result;
    }

    @Override
    public boolean equals(Object other) {
        if (other instanceof MDNSServiceInfo s) {
            return serviceName.equals(s.serviceName)
                    && address.equals(s.address)
                    && (host != null ? host.equals(s.host) : s.host == null)
                    && (localAddress != null ? localAddress.equals(s.localAddress) : s.localAddress == null)
                    && port == s.port
                    && txt.equals(s.txt);
        }
        return false;
    }
}
