package com.dynamo.upnp;

import static org.junit.Assert.assertNotNull;
import static org.junit.Assert.assertNull;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

public class SSDPTest {
    SSDP ssdp;
    final static String USN1 = "uuid:00000001-3d4f-339c-8c4d-f7c6da6771c8::upnp:rootdevice";
    final static String USN2 = "uuid:00000002-3d4f-339c-8c4d-f7c6da6771c8::upnp:rootdevice";

    @Before
    public void setUp() throws Exception {
        ssdp = new SSDP();
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

