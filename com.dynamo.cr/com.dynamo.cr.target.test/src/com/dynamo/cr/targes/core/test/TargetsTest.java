package com.dynamo.cr.targes.core.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.net.InetAddress;
import java.util.ArrayList;
import java.util.HashMap;

import javax.inject.Singleton;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.targets.core.ITarget;
import com.dynamo.cr.targets.core.ITargetsListener;
import com.dynamo.cr.targets.core.ITargetsService;
import com.dynamo.cr.targets.core.IURLFetcher;
import com.dynamo.cr.targets.core.TargetChangedEvent;
import com.dynamo.cr.targets.core.TargetsService;
import com.dynamo.upnp.DeviceInfo;
import com.dynamo.upnp.ISSDP;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

public class TargetsTest implements ITargetsListener {
    private ITargetsService targetService;
    private ArrayList<TargetChangedEvent> events;
    private ISSDP ssdp;

    private final String UDN = "uuid:0509f95d-3d4f-339c-8c4d-f7c6da6771c8";
    private final String URL = "http://localhost:1234";

    private final String DEVICE_DESC =
            "<?xml version=\"1.0\" encoding=\"utf-8\"?>\n" +
            "<root xmlns=\"urn:schemas-upnp-org:device-1-0\" xmlns:defold=\"urn:schemas-defold-com:DEFOLD-1-0\">\n" +
            "    <specVersion>\n" +
            "        <major>1</major>\n" +
            "        <minor>0</minor>\n" +
            "    </specVersion>\n" +
            "    <device>\n" +
            "        <deviceType>upnp:rootdevice</deviceType>\n" +
            "        <friendlyName>Defold System</friendlyName>\n" +
            "        <manufacturer>Defold</manufacturer>\n" +
            "        <modelName>Defold Engine 1.0</modelName>\n" +
            "        <UDN>" + UDN + "</UDN>\n" +
            "        <defold:url>" + URL + "</defold:url>\n" +
            "    </device>\n" +
            "</root>\n";
    private IURLFetcher urlFetcher;

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(ITargetsService.class).to(TargetsService.class).in(Singleton.class);
            bind(ISSDP.class).toInstance(ssdp);
            bind(IURLFetcher.class).toInstance(urlFetcher);
        }
    }

    @Override
    public void targetsChanged(TargetChangedEvent e) {
        synchronized(this) {
            events.add(e);
        }
    }

    @Before
    public void setUp() throws Exception {
        urlFetcher = mock(IURLFetcher.class);
        ssdp = mock(ISSDP.class);
        Module module = new Module();
        Injector injector = Guice.createInjector(module);
        targetService = injector.getInstance(ITargetsService.class);
        targetService.addTargetsListener(this);
        events = new ArrayList<TargetChangedEvent>();
    }

    @After
    public void tearDown() throws Exception {
        targetService.stop();
    }

    @Test
    public void testStartStopService() throws Exception {
        // Intensionally left empty
    }

    @Test
    public void testSearchLocal() throws Exception {
        /*
         * Search for local target and ensure that the psuedo-target is replaced
         * by the found local target
         */
        String localAddress = InetAddress.getLocalHost().getHostAddress();
        targetService.setSearchInternal(1);
        when(urlFetcher.fetch(anyString())).thenReturn(DEVICE_DESC);
        when(ssdp.getDevices()).thenReturn(new DeviceInfo[] { new DeviceInfo(System.currentTimeMillis() + 1000, new HashMap<String, String>(), localAddress) } );
        when(ssdp.update(true)).thenReturn(true);
        Thread.sleep(100);
        synchronized (this) {
            assertThat(events.size(), is(0));
            ITarget[] targets = targetService.getTargets();
            assertThat(targets.length, is(1));
            assertThat(targets[0].getId(), is(ITargetsService.LOCAL_TARGET_ID));
            assertThat(targets[0].getUrl(), is((String) null));
        }

        Thread.sleep(1100);
        verify(ssdp, atLeast(1)).update(false);
        verify(ssdp, times(1)).update(true);

        synchronized (this) {
            assertThat(events.size(), is(1));
            ITarget[] targets = targetService.getTargets();
            assertThat(targets.length, is(1));
            assertThat(targets[0].getId(), is(UDN));
            assertThat(targets[0].getUrl(), is(URL));
        }
    }

    @Test
    public void testSearchNetwork() throws Exception {
        /*
         * Search for network target. Expected device count is two. The network target and the local psuedo-target
         */
        targetService.setSearchInternal(1);
        when(urlFetcher.fetch(anyString())).thenReturn(DEVICE_DESC);
        when(ssdp.getDevices()).thenReturn(new DeviceInfo[] { new DeviceInfo(System.currentTimeMillis() + 1000, new HashMap<String, String>(), "127.0.0.1") } );
        when(ssdp.update(true)).thenReturn(true);
        Thread.sleep(100);
        synchronized (this) {
            assertThat(events.size(), is(0));
            ITarget[] targets = targetService.getTargets();
            assertThat(targets.length, is(1));
            assertThat(targets[0].getId(), is(ITargetsService.LOCAL_TARGET_ID));
            assertThat(targets[0].getUrl(), is((String) null));
        }

        Thread.sleep(1100);
        verify(ssdp, atLeast(1)).update(false);
        verify(ssdp, times(1)).update(true);

        synchronized (this) {
            assertThat(events.size(), is(1));
            ITarget[] targets = targetService.getTargets();
            assertThat(targets.length, is(2));
            assertThat(targets[0].getId(), is(UDN));
            assertThat(targets[0].getUrl(), is(URL));
            assertThat(targets[1].getId(), is(ITargetsService.LOCAL_TARGET_ID));
            assertThat(targets[1].getUrl(), is((String) null));
        }
    }

}

