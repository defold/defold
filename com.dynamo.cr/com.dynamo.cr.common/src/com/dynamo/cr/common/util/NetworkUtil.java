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

package com.dynamo.cr.common.util;

import java.net.Inet4Address;
import java.net.InetAddress;
import java.net.NetworkInterface;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Collection;
import java.util.Collections;
import java.util.Enumeration;
import java.util.Vector;

public class NetworkUtil {

    /**
     * Returns a list of the local inet addresses which are IPv4.
     *
     * @return list of local inet addresses
     * @throws SocketException
     * @throws UnknownHostException
     */
    public static Collection<InetAddress> getValidHostAddresses() throws SocketException, UnknownHostException {
        Enumeration<NetworkInterface> interfaces = NetworkInterface.getNetworkInterfaces();
        Vector<InetAddress> result = new Vector<InetAddress>();
        while (interfaces.hasMoreElements()) {
            NetworkInterface netInterface = interfaces.nextElement();
            if (netInterface.isUp()) {
                Enumeration<InetAddress> addresses = netInterface.getInetAddresses();
                while (addresses.hasMoreElements()) {
                    InetAddress address = addresses.nextElement();
                    if (address instanceof Inet4Address) {
                        result.add(address);
                    }
                }
            }
        }
        if (result.isEmpty()) {
            return Collections.singleton(InetAddress.getByName("127.0.0.1"));
        }
        return result;
    }

    // Rank address according to most significant bytes
    // Perhaps better to rank according to most significant bites but this is probably good enough
    private static int compareAddresses(InetAddress a, InetAddress b)
    {
        int rank = 0;
        byte[] aa = a.getAddress();
        byte[] ab = b.getAddress();

        for (int i = 0; i < Math.min(aa.length, ab.length); ++i)
        {
            if (aa[i] == ab[i]) {
                rank++;
            } else {
                break;
            }
        }

        return rank;
    }

    /**
     * Returns the address which most closely matches the target address.
     *
     * @return Localhost IP
     * @throws SocketException
     * @throws UnknownHostException
     */
    public static InetAddress getClosestAddress(Collection<InetAddress> addresses, InetAddress targetAddress)
                                                                                                             throws SocketException,
                                                                                                             UnknownHostException {
        if (addresses.isEmpty()) {
            return InetAddress.getByName("127.0.0.1");
        }
        // Return first if there is no target
        if (targetAddress == null) {
            return addresses.iterator().next();
        }

        ArrayList<InetAddress> tmp = new ArrayList<InetAddress>(addresses);
        InetAddress ret = tmp.get(0);
        int max = 0;
        int i = 0;
        for (InetAddress a : addresses) {
            int r = compareAddresses(a, targetAddress);
            if (r > max) {
                ret = tmp.get(i);
                max = r;
            }
            i++;
        }

        return ret;
    }
}
