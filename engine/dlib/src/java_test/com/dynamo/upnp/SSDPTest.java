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
    final static String USN1 = "uuid:00000001-3d4f-339c-8c4d-f7c6da6771c8::upnp:rootdevice";
    final static String USN2 = "uuid:00000002-3d4f-339c-8c4d-f7c6da6771c8::upnp:rootdevice";

    private static void debug_info(String message) {
        long timestamp = System.currentTimeMillis() / 1000;
        System.out.println("(" + Long.toString(timestamp) + ") [JAVA] " + message);
    }

    @BeforeClass
    public static void setUpBaseClass() {
        SSDPTest.debug_info("Starting SSDPTest ...");
    }

    @Before
    public void setUp() throws Exception {
        SSDPTest.debug_info("Starting setup of test ...");
        ssdp = new SSDP();
        ssdp.setup();
        Thread.sleep(100);
        ssdp.update(false);
        ssdp.clearDiscovered();
        SSDPTest.debug_info("Completed setup of test!");
    }

    @After
    public void tearDown() throws Exception {
        SSDPTest.debug_info("Started teardown of test ...");
        ssdp.dispose();
        SSDPTest.debug_info("Completed teardown of test!");
    }

    void assertDevice(String d) {
        SSDPTest.debug_info("Assert \"" + d + "\" is not null");
        assertNotNull(ssdp.getDeviceInfo(d));
    }

    void assertNotDevice(String d) {
        SSDPTest.debug_info("Assert \"" + d + "\" is null");
        assertNull(ssdp.getDeviceInfo(d));
    }

    /*
     * The loops in tests are completely arbitrary
     * Don't know what you should expect about UDP package delivery
     * between processes
     */

    @Test
    public void testSearch() throws Exception {
        SSDPTest.debug_info("Starting test \"testSearch\" ...");
        ssdp.update(false);
        assertNotDevice(USN2);

        for (int i = 0; i < 5; ++i) {
            // Search in the first iteration
            ssdp.update(i == 0);
            Thread.sleep(100);
        }
        assertDevice(USN2);
        SSDPTest.debug_info("Completed test \"testSearch\"!");
    }

    @Test
    public void testAnnounce() throws Exception {
        SSDPTest.debug_info("Starting test \"testAnnounce\" ...");
        assertNotDevice(USN1);

        for (int i = 0; i < 15; ++i) {
            Thread.sleep(100);
            ssdp.update(false);
        }
        assertDevice(USN1);
        SSDPTest.debug_info("Completed test \"testAnnounce\"!");
    }

    @Test
    public void testExpiration() throws Exception {
        SSDPTest.debug_info("Starting test \"testExpiration\" ...");
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
        SSDPTest.debug_info("Completed test \"testExpiration\"!");
    }
}

