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

package com.dynamo.upnp;

import java.util.Collections;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

public class DeviceInfo {

    private static Pattern pattern = Pattern.compile(".*?max-age=(\\d+).*");
    public final long expires;
    public final Map<String, String> headers;
    public String address;
    // Host address from which the device can be reached
    public String localAddress;

    public DeviceInfo(long expires, Map<String, String> headers, String address, String localAddress) {
        this.expires = expires;
        this.headers = Collections.unmodifiableMap(headers);
        this.address = address;
        this.localAddress = localAddress;
    }

    @Override
    public int hashCode() {
        return headers.hashCode() + address.hashCode();
    }

    @Override
    public boolean equals(Object other) {
        if (other instanceof DeviceInfo) {
            DeviceInfo di = (DeviceInfo) other;
            return address.equals(di.address) && headers.equals(di.headers);
        }
        return false;
    }

    public static DeviceInfo create(Map<String, String> headers, String address, String localAddress) {
        int maxAge = 1800;

        String cacheControl = headers.get("CACHE-CONTROL");
        if (cacheControl != null) {
            Matcher m = pattern.matcher(cacheControl);

            if (m.matches()) {
                maxAge = Integer.parseInt(m.group(1));
            }
        }

        long expires = System.currentTimeMillis() + maxAge * 1000;

        return new DeviceInfo(expires, headers, address, localAddress);
    }

}
