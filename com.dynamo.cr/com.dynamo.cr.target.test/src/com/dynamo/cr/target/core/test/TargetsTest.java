package com.dynamo.cr.target.core.test;

import static org.hamcrest.CoreMatchers.is;
import static org.junit.Assert.assertThat;
import static org.mockito.Matchers.anyString;
import static org.mockito.Mockito.atLeast;
import static org.mockito.Mockito.mock;
import static org.mockito.Mockito.times;
import static org.mockito.Mockito.verify;
import static org.mockito.Mockito.when;

import java.io.ByteArrayOutputStream;
import java.io.OutputStream;
import java.net.InetSocketAddress;
import java.net.ServerSocket;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Comparator;
import java.util.HashMap;

import javax.inject.Singleton;

import org.junit.After;
import org.junit.Before;
import org.junit.Test;

import com.dynamo.cr.editor.core.IConsole;
import com.dynamo.cr.editor.core.IConsoleFactory;
import com.dynamo.cr.target.core.ITarget;
import com.dynamo.cr.target.core.ITargetListener;
import com.dynamo.cr.target.core.ITargetService;
import com.dynamo.cr.target.core.IURLFetcher;
import com.dynamo.cr.target.core.TargetChangedEvent;
import com.dynamo.cr.target.core.TargetService;
import com.dynamo.upnp.DeviceInfo;
import com.dynamo.upnp.ISSDP;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Inject;
import com.google.inject.Injector;

public class TargetsTest implements ITargetListener {
    private ITargetService targetService;
    private ArrayList<TargetChangedEvent> events;
    private ISSDP ssdp;
    private Injector injector;

    private final String UDN = "uuid:0509f95d-3d4f-339c-8c4d-f7c6da6771c8";
    private final String URL = "http://localhost:1234";
    private final int LOG_PORT = 5678;

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
                    "        <defold:logPort>" + LOG_PORT + "</defold:logPort>\n" +
                    "    </device>\n" +
                    "</root>\n";
    private IURLFetcher urlFetcher;
    private IConsoleFactory consoleFactory;

    static class TestTargetService extends TargetService {

        @Inject
        public TestTargetService(ISSDP ssdp, IURLFetcher urlFetcher, IConsoleFactory consoleFactory) {
            super(ssdp, urlFetcher, consoleFactory);
        }

        @Override
        public synchronized void launch(String customApplication, String location, boolean runInDebugger,
                                        boolean autoRunDebugger, String socksProxy, int socksProxyPort,
                                        int httpServerPort, boolean quitOnEsc) {
            ITarget[] targets = getTargets();
            logClient.resetLogSocket(targets[targets.length - 1]);
        }
    }

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(ITargetService.class).to(TestTargetService.class).in(Singleton.class);
            bind(ISSDP.class).toInstance(ssdp);
            bind(IURLFetcher.class).toInstance(urlFetcher);
            bind(IConsoleFactory.class).toInstance(consoleFactory);
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
        consoleFactory = mock(IConsoleFactory.class);
        Module module = new Module();
        injector = Guice.createInjector(module);
        events = new ArrayList<TargetChangedEvent>();
    }

    private void startTargetService() throws Exception {
        targetService = injector.getInstance(ITargetService.class);
        targetService.setSearchInternal(1);
        targetService.addTargetsListener(this);
    }

    @After
    public void tearDown() throws Exception {
        targetService.stop();
    }

    @Test
    public void testStartStopService() throws Exception {
        startTargetService();
    }

    @Test
    public void testSearchLocal() throws Exception {
        /*
         * Search for local target and ensure that the pseudo-target is still present at index 0
         */
        String localAddress = "127.0.0.1";
        when(urlFetcher.fetch(anyString())).thenReturn(DEVICE_DESC);
        when(ssdp.getDevices()).thenReturn(new DeviceInfo[] { newDeviceInfo(localAddress) } );
        when(ssdp.update(true)).thenReturn(true);
        startTargetService();
        Thread.sleep(100);
        synchronized (this) {
            assertThat(events.size(), is(0));
            ITarget[] targets = targetService.getTargets();
            assertThat(targets.length, is(1));
            assertThat(targets[0].getId(), is(ITargetService.LOCAL_TARGET_ID));
            assertThat(targets[0].getUrl(), is((String) null));
        }

        Thread.sleep(1100);
        verify(ssdp, atLeast(1)).update(false);
        // One initial and one due to timeout
        verify(ssdp, times(2)).update(true);

        synchronized (this) {
            assertThat(events.size(), is(2));
            ITarget[] targets = targetService.getTargets();
            assertThat(targets.length, is(2));
            assertThat(targets[0].getId(), is(UDN));
            assertThat(targets[0].getUrl(), is(URL));
            assertThat(targets[0].getLogPort(), is(LOG_PORT));
            assertThat(targets[1].getId(), is(ITargetService.LOCAL_TARGET_ID));
            assertThat(targets[1].getUrl(), is((String) null));
        }
    }

    private DeviceInfo newDeviceInfo(String localAddress) {
        HashMap<String, String> headers = new HashMap<String, String>();
        headers.put("LOCATION", "http://localhost");
        return new DeviceInfo(System.currentTimeMillis() + 1000, headers, localAddress, "127.0.0.1");
    }

    @Test
    public void testBlackList() throws Exception {
        String localAddress = "127.0.0.1";
        when(urlFetcher.fetch(anyString())).thenThrow(new SocketTimeoutException());
        when(ssdp.getDevices()).thenReturn(new DeviceInfo[] { newDeviceInfo(localAddress), newDeviceInfo(localAddress) } );
        when(ssdp.update(true)).thenReturn(true);
        startTargetService();
        Thread.sleep(100);
        synchronized (this) {
            assertThat(events.size(), is(0));
            ITarget[] targets = targetService.getTargets();
            assertThat(targets.length, is(1));
        }

        Thread.sleep(1100);
        verify(ssdp, atLeast(1)).update(false);
        // One initial and one due to timeout
        verify(ssdp, times(2)).update(true);

        // Check that blacklisted (socket timeout) is fetched only once per update
        verify(urlFetcher, times(2)).fetch(anyString());

        synchronized (this) {
            assertThat(events.size(), is(2));
            ITarget[] targets = targetService.getTargets();
            assertThat(targets.length, is(1));
        }
    }

    @Test
    public void testSearchNetwork() throws Exception {
        /*
         * Search for network target. Expected device count is two. The network target and the local psuedo-target
         */
        when(urlFetcher.fetch(anyString())).thenReturn(DEVICE_DESC);
        when(ssdp.getDevices()).thenReturn(new DeviceInfo[] { newDeviceInfo("1.2.3.4") });
        when(ssdp.update(true)).thenReturn(true);
        startTargetService();
        Thread.sleep(100);
        synchronized (this) {
            assertThat(events.size(), is(0));
            ITarget[] targets = targetService.getTargets();
            assertThat(targets.length, is(1));
            assertThat(targets[0].getId(), is(ITargetService.LOCAL_TARGET_ID));
            assertThat(targets[0].getUrl(), is((String) null));
        }

        Thread.sleep(1100);
        verify(ssdp, atLeast(1)).update(false);
        // One initial and one due to timeout
        verify(ssdp, times(2)).update(true);

        synchronized (this) {
            assertThat(events.size(), is(2));
            ITarget[] targets = targetService.getTargets();
            assertThat(targets.length, is(2));
            Arrays.sort(targets, new Comparator<ITarget>() {

                @Override
                public int compare(ITarget t0, ITarget t1) {
                    return t0.getId().compareTo(t1.getId());
                }
            });
            assertThat(targets[0].getId(), is(ITargetService.LOCAL_TARGET_ID));
            assertThat(targets[0].getUrl(), is((String) null));
            assertThat(targets[1].getId(), is(UDN));
            assertThat(targets[1].getUrl(), is(URL));
        }
    }

    private static class GameLogger implements Runnable {
        private ServerSocket logServer;
        private boolean done;

        public GameLogger(int log_port) {
            done = false;
            try {
                logServer = new ServerSocket();
                logServer.setReuseAddress(true);
                logServer.bind(new InetSocketAddress(log_port));
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        @Override
        public void run() {
            try {
                Socket socket = logServer.accept();
                OutputStream logOut = socket.getOutputStream();
                logOut.write("0 OK\ntesting".getBytes());
                logOut.close();
                done = true;
            } catch (Exception e) {
                throw new RuntimeException(e);
            }
        }

        public boolean isDone() {
            return done;
        }
    }

    @Test
    public void testRemoteLogging() throws Exception {
        /*
         * Search for network target. Expected device count is two. The network
         * target and the local psuedo-target
         */
        GameLogger gameLogger = new GameLogger(LOG_PORT);
        new Thread(gameLogger).start();
        when(urlFetcher.fetch(anyString())).thenReturn(DEVICE_DESC);
        IConsole console = mock(IConsole.class);
        ByteArrayOutputStream consoleOut = new ByteArrayOutputStream();
        when(console.createOutputStream()).thenReturn(consoleOut);
        when(consoleFactory.getConsole(anyString())).thenReturn(console);
        when(ssdp.getDevices()).thenReturn(new DeviceInfo[] { newDeviceInfo("1.2.3.4") });
        when(ssdp.update(true)).thenReturn(true);
        startTargetService();
        while (targetService.getTargets().length == 1) {
            Thread.sleep(100);
        }
        targetService.launch("", "", false, false, "", 1080, 8080, false);
        while (!gameLogger.isDone()) {
            Thread.sleep(100);
        }
        Thread.sleep(500);
        synchronized (this) {
            assertThat(consoleOut.toString("UTF8"), is("testing"));
        }
    }
}

