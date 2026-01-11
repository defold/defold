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

package com.dynamo.cr.common.util.test;

import static org.hamcrest.core.Is.is;
import static org.junit.Assert.assertThat;

import java.net.InetAddress;
import java.net.SocketException;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Collection;

import org.junit.Test;

import com.dynamo.cr.common.util.NetworkUtil;

public class NetworkUtilTest {

    private Collection<InetAddress> getAddresses(String[] addresses) throws UnknownHostException {
        Collection<InetAddress> result = new ArrayList<InetAddress>(addresses.length);
        for (String address : addresses) {
            result.add(InetAddress.getByName(address));
        }
        return result;
    }

    private void assertAddresses(String[] hostAddresses, String target, String expected) throws UnknownHostException,
                                                                                        SocketException {
        InetAddress t = InetAddress.getByName(target);
        InetAddress host = NetworkUtil.getClosestAddress(getAddresses(hostAddresses), t);
        assertThat(expected, is(host.getHostAddress()));
    }

    @Test
    public void testGetHostAddress() throws UnknownHostException, SocketException {
        // basic test
        assertAddresses(new String[] { "127.0.0.1", "192.168.1.1" }, "192.168.1.2", "192.168.1.1");
        // reversed order
        assertAddresses(new String[] { "192.168.1.1", "127.0.0.1" }, "192.168.1.2", "192.168.1.1");
        // byte order (most significant first)
        assertAddresses(new String[] { "193.168.1.2", "192.0.0.1" }, "192.168.1.2", "192.0.0.1");
        // edge case of only local host
        assertAddresses(new String[] { "127.0.0.1" }, "192.168.1.2", "127.0.0.1");

        // additional test for bug 2442
        assertAddresses(new String[] { "127.0.0.1", "192.168.1.1", "1.1.1.1", "2.2.2.2", "3.3.3.3" }, "192.168.1.2", "192.168.1.1");

    }
}
