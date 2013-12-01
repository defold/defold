package com.dynamo.cr.target.core;

import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.io.StringReader;
import java.net.InetAddress;
import java.net.InetSocketAddress;
import java.net.MalformedURLException;
import java.net.Socket;
import java.net.SocketTimeoutException;
import java.net.URL;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.inject.Inject;
import javax.ws.rs.core.UriBuilder;

import org.apache.commons.io.IOUtils;
import org.eclipse.core.commands.Command;
import org.eclipse.core.commands.State;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.commands.ICommandService;
import org.jdom2.Document;
import org.jdom2.Element;
import org.jdom2.Namespace;
import org.jdom2.input.JDOMParseException;
import org.jdom2.input.SAXBuilder;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.common.util.NetworkUtil;
import com.dynamo.cr.editor.core.IConsole;
import com.dynamo.cr.editor.core.IConsoleFactory;
import com.dynamo.upnp.DeviceInfo;
import com.dynamo.upnp.ISSDP;

public class TargetService implements ITargetService, Runnable {

    private List<ITargetListener> listeners = new ArrayList<ITargetListener>();
    private Thread thread;
    private volatile boolean stopped = false;
    private ISSDP ssdp;
    private IURLFetcher urlFetcher;
    private IConsoleFactory consoleFactory;
    private int searchInterval = 60;
    private long lastSearch = 0;
    private ITarget[] targets;
    private Socket logSocket;
    private InetSocketAddress logSocketAddress;

    private static Logger logger = LoggerFactory.getLogger(TargetService.class);

    private static ITarget createLocalTarget() {
        InetAddress localAddress = null;
        String name = "Local (no ip)";
        try {
            localAddress = NetworkUtil.getValidHostAddresses().iterator().next();
            name = String.format("Local (%s)", localAddress.getHostAddress());
        } catch (Exception e) {
            logger.error("Could not get host address", e);
        }

        return new Target(name, LOCAL_TARGET_ID, localAddress, null, 0);
    }

    @Inject
    public TargetService(ISSDP ssdp, IURLFetcher urlFetcher, IConsoleFactory consoleFactory) {
        targets = new ITarget[] { createLocalTarget(), };

        lastSearch = System.currentTimeMillis() - searchInterval * 1000;
        this.ssdp = ssdp;
        this.urlFetcher = urlFetcher;
        this.consoleFactory = consoleFactory;
        thread = new Thread(this, "Targets Service");
        thread.start();
    }

    @Override
    public synchronized void addTargetsListener(ITargetListener listener) {
        listeners.add(listener);
    }

    @Override
    public synchronized void removeTargetsListener(ITargetListener listener) {
        listeners.remove(listener);
    }

    @Override
    public synchronized ITarget[] getTargets() {
        return targets;
    }

    @Override
    public void stop() {
        stopped = true;
        thread.interrupt();
        try {
            thread.join();
        } catch (InterruptedException e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public void setSearchInternal(int searchInterval) {
        this.searchInterval = searchInterval;
    }

    @Override
    public void run() {
        while (!stopped) {
            try {
                Thread.sleep(100);
            } catch (InterruptedException e) {
                continue;
            }
            try {
                boolean search = false;
                long current = System.currentTimeMillis();
                if (current >= lastSearch + searchInterval * 1000) {
                    lastSearch = current;
                    search = true;
                }
                boolean changed = ssdp.update(search);

                /*
                 * NOTE: We can't just rely on the "changed" variable
                 * as TargetService doens't try to reconnect to
                 * devices in updateTargets() in case of timeout etc
                 */
                if (changed || search) {
                    updateTargets();
                    postEvent(new TargetChangedEvent(this));
                }
                updateLog();
            } catch (IOException e) {
                // Do not log IOException. Network is down, etc
            }
        }
    }

    @Override
    public void search() {
        // Force search now
        lastSearch = System.currentTimeMillis() - searchInterval * 1000;
    }

    private String getRequiredDeviceField(Element device, String name,
            Namespace ns) {
        Element e = device.getChild(name, ns);
        if (e == null) {
            throw new RuntimeException(String.format(
                    "Required element %s not found", name));
        }
        return e.getTextTrim();
    }

    private void updateTargets() {
        DeviceInfo[] devices = ssdp.getDevices();
        List<ITarget> targets = new ArrayList<ITarget>();
        // Certain devices, such as routers, may send multiple devices with the
        // same
        // location url. The cache is used to speed up the fetch
        Map<String, String> descriptionCache = new HashMap<String, String>();

        // A single host might advertise multiple devices.
        // We temporarily blacklist non-reachable devices (socket timeout)
        Set<String> blackList = new HashSet<String>();

        boolean localTargetFound = false;
        for (DeviceInfo deviceInfo : devices) {
            String location = deviceInfo.headers.get("LOCATION");
            URL locationURL = null;
            try {
                locationURL = new URL(location);
            } catch (MalformedURLException e2) {
                continue;
            }

            if (blackList.contains(locationURL.getHost())) {
                // NOTE: Do not log here
                continue;
            }

            String description = null;
            try {
                if (descriptionCache.containsKey(location)) {
                    description = descriptionCache.get(location);
                } else {
                    description = urlFetcher.fetch(location);
                    descriptionCache.put(location, description);
                }

                SAXBuilder builder = new SAXBuilder();

                Document doc = builder.build(new StringReader(description));
                Namespace upnpNS = Namespace
                        .getNamespace("urn:schemas-upnp-org:device-1-0");
                Namespace defoldNS = Namespace
                        .getNamespace("urn:schemas-defold-com:DEFOLD-1-0");
                Element device = doc.getRootElement()
                        .getChild("device", upnpNS);
                String manufacturer = device.getChild("manufacturer", upnpNS)
                        .getTextTrim();
                String udn = getRequiredDeviceField(device, "UDN", upnpNS);
                String friendlyName = getRequiredDeviceField(device, "friendlyName", upnpNS);

                if (manufacturer.equalsIgnoreCase("defold")) {
                    String url = getRequiredDeviceField(device, "url", defoldNS);
                    String logPort = getRequiredDeviceField(device, "logPort", defoldNS);
                    // TODO: Local should be iPhone or Joe's iPhone or similar
                    // when iPhone supported is completed
                    String name = String.format("%s (%s)", friendlyName, deviceInfo.address);
                    InetAddress targetAddress = InetAddress.getByName(deviceInfo.address);
                    InetAddress hostAddress = NetworkUtil.getClosestAddress(NetworkUtil.getValidHostAddresses(),
                            targetAddress);
                    ITarget target = new Target(name, udn, targetAddress, url,
                            Integer.parseInt(logPort));
                    targets.add(target);

                    if (deviceInfo.address.equals(hostAddress.getHostAddress())) {
                        localTargetFound = true;
                    }

                }
            } catch (JDOMParseException e) {
                // Do not log here. We've seen invalid xml responses in real networks
            } catch (IOException e) {
                // Do not log IOException. This happens...
                if (e instanceof SocketTimeoutException) {
                    blackList.add(locationURL.getHost());
                }
            } catch (Throwable e) {
                logger.error("Unexpected error", e);
            }
        }

        if (!localTargetFound) {
            // No running local target found. Add pseudo-target to head of list.
            // It's a convention to be able to fallback to the first target
            targets.add(0, createLocalTarget());
        }

        synchronized (this) {
            this.targets = targets.toArray(new ITarget[targets.size()]);
        }
    }

    private synchronized void postEvent(TargetChangedEvent event) {
        for (ITargetListener listener : listeners) {
            try {
                listener.targetsChanged(event);
            } catch (Throwable e) {
                logger.error(e.getMessage(), e);
            }
        }
    }

    private static Socket newLogConnection(InetSocketAddress address) throws IOException {
        Socket socket = new Socket();
        socket.setSoTimeout(2000);
        socket.connect(address);

        InputStream is = null;
        try {
            StringBuilder sb = new StringBuilder();
            is = socket.getInputStream();
            int c = is.read();
            while (c != '\n' && c != -1) {
                if (c != '\r') {
                    sb.append((char) c);
                }
                c = is.read();
            }
            if (!sb.toString().equals("0 OK")) {
                throw new IOException(String.format("Unable to connect to log-service (%s)", sb.toString()));
            }
            socket.setSoTimeout(0);
            return socket;
        } catch (IOException e) {
            IOUtils.closeQuietly(is);
            IOUtils.closeQuietly(socket);
            throw e;
        }
    }

    private synchronized void updateLog() {
        // Check to see if we should make a new connection
        if (this.logSocketAddress != null) {
            // Close if already connected
            if (this.logSocket != null) {
                try {
                    this.logSocket.close();
                } catch (IOException e) {
                    logger.warn("Log socket could not be closed: {}", e.getMessage());
                } finally {
                    this.logSocket = null;
                }
            }
            // Connect
            try {
                this.logSocket = newLogConnection(logSocketAddress);
            } catch (IOException e) {
                logger.warn("Log socket could not be opened: {}", e.getMessage());
            } finally {
                this.logSocketAddress = null;
            }
        }
        if (this.logSocket != null) {
            try {
                if (this.logSocket.isConnected()) {
                    byte[] data = null;
                    OutputStream out = null;
                    InputStream in = this.logSocket.getInputStream();
                    int available = in.available();
                    while (available > 0) {
                        if (out == null) {
                            IConsole console = this.consoleFactory.getConsole("console");
                            out = console.createOutputStream();
                            data = new byte[128];
                        }
                        int count = Math.min(available, data.length);
                        in.read(data, 0, count);
                        out.write(data, 0, count);
                        available = in.available();
                    }
                    if (out != null) {
                        out.close();
                    }
                }
            } catch (IOException e) {
                if (this.logSocket.isConnected()) {
                    try {
                        this.logSocket.close();
                    } catch (IOException e1) {
                        logger.error(e1.getMessage());
                    }
                }
                this.logSocket = null;
                logger.error(e.getMessage());
            }
        }
    }

    @Override
    public ITarget getSelectedTarget() {
        ICommandService commandService = (ICommandService) PlatformUI.getWorkbench().getService(ICommandService.class);
        Command selectTarget = commandService.getCommand("com.dynamo.cr.target.commands.selectTarget");
        State state = selectTarget.getState("org.eclipse.ui.commands.radioState");
        String stateValue = (String) state.getValue();
        ITarget activeTarget = null;
        for (ITarget target : this.targets) {
            if (target.getName().equals(stateValue)) {
                activeTarget = target;
                break;
            }
        }

        if (activeTarget == null) {
            // Preferred target not found. Fallback to to the first target.
            // The first target, is by convention, local
            activeTarget = targets[0];
        }
        return activeTarget;
    }

    public URL getHttpServerURL(InetAddress targetAddress, int httpServerPort) {
        String localAddress = "127.0.0.1";
        try {
            localAddress = NetworkUtil.getClosestAddress(NetworkUtil.getValidHostAddresses(), targetAddress)
                    .getHostAddress();
        } catch (Exception e) {
            logger.error(e.getMessage(), e);
        }

        try {
            return UriBuilder.fromPath("/").scheme("http").host(localAddress).port(httpServerPort).build().toURL();
        } catch (Exception e) {
            throw new RuntimeException(e);
        }
    }

    @Override
    public synchronized void launch(String customApplication, String location,
                                    boolean runInDebugger,
                                    boolean autoRunDebugger,
 String socksProxy, int socksProxyPort, int httpServerPort) {

        ITarget targetToLaunch = getSelectedTarget();
        LaunchThread launchThread = new LaunchThread(targetToLaunch, customApplication, location, runInDebugger,
                autoRunDebugger, socksProxy, socksProxyPort, getHttpServerURL(targetToLaunch.getInetAddress(),
                        httpServerPort));
        launchThread.start();
        connectToLogService(targetToLaunch);
    }

    protected void connectToLogService(ITarget target) {
        if (target.getUrl() != null) {
            try {
                URL url = new URL(target.getUrl());
                String host = url.getHost();
                logSocketAddress = new InetSocketAddress(host, target.getLogPort());
            } catch (MalformedURLException e) {
                logger.warn("Could not open log socket: {}", e.getMessage());
                logSocketAddress = null;
            }
        }
    }

}

