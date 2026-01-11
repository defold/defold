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

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import org.junit.After;
import org.junit.Before;
import org.junit.BeforeClass;
import org.junit.Test;

import com.dynamo.upnp.SSDP;

public class SSDPTest {
    ISSDP ssdp;

    private static String USN1 = "uuid:00000001-3d4f-339c-8c4d-f7c6da6771c8::upnp:rootdevice";
    private static String USN2 = "uuid:00000002-3d4f-339c-8c4d-f7c6da6771c8::upnp:rootdevice";

    @BeforeClass
    public static void setUpBaseClass() {
        if (System.getProperty("USN1") != null) {
            SSDPTest.USN1 = System.getProperty("USN1");
        }

        if (System.getProperty("USN2") != null) {
            SSDPTest.USN2 = System.getProperty("USN2");
        }

        System.out.println("(JAVA) USN1 = '" + SSDPTest.USN1 + "'");
        System.out.println("(JAVA) USN2 = '" + SSDPTest.USN2 + "'");
    }

    @Before
    public void setUp() throws Exception {
        ssdp = new SSDP();
        ssdp.setup();
        Thread.sleep(100);
        ssdp.update(false);
        ssdp.clearDiscovered();
    }

    @After
    public void tearDown() throws Exception {
        ssdp.dispose();
    }

    void assertDevice(String d) {
        assertNotNull(ssdp.getDeviceInfo(d));
    }

    void assertNotDevice(String d) {
        assertNull(ssdp.getDeviceInfo(d));
    }

    /*
     * The loops in tests are completely arbitrary
     * Don't know what you should expect about UDP package delivery
     * between processes
     */

    @Test
    public void testSearch() throws Exception {
        ssdp.update(false);
        assertNotDevice(USN2);

        for (int i = 0; i < 5; ++i) {
            // Search in the first iteration
            ssdp.update(i == 0);
            Thread.sleep(100);
        }
        assertDevice(USN2);
    }

    @Test
    public void testAnnounce() throws Exception {
        assertNotDevice(USN1);

        for (int i = 0; i < 15; ++i) {
            Thread.sleep(100);
            ssdp.update(false);
        }
        assertDevice(USN1);
    }

    @Test
    public void testExpiration() throws Exception {
        ssdp.update(false);
        assertNotDevice(USN2);

        for (int i = 0; i < 5; ++i) {
            // Search in the first iteration
            ssdp.update(i == 0);
            Thread.sleep(100);
        }

        Thread.sleep(2001);
        ssdp.update(false);
        // The device should now be expired
        // NOTE: If someone else on the network would issue a M-SEARCH
        // this test might fail. A bit fragile.
        assertNotDevice(USN2);
    }
}

