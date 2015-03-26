package com.dynamo.cr.target.core;

import java.io.IOException;
import java.io.StringReader;
import java.net.InetAddress;
import java.net.MalformedURLException;
import java.net.SocketTimeoutException;
import java.net.URL;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.HashSet;
import java.util.Iterator;
import java.util.List;
import java.util.Map;
import java.util.Set;

import javax.inject.Inject;
import javax.ws.rs.core.UriBuilder;

import org.eclipse.core.commands.Command;
import org.eclipse.core.commands.State;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.commands.ICommandService;
import org.jdom2.Document;
import org.jdom2.Element;
import org.jdom2.IllegalNameException;
import org.jdom2.Namespace;
import org.jdom2.input.JDOMParseException;
import org.jdom2.input.SAXBuilder;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.common.util.NetworkUtil;
import com.dynamo.cr.editor.core.IConsoleFactory;
import com.dynamo.upnp.DeviceInfo;
import com.dynamo.upnp.ISSDP;

/**
 * <p><b>Log connection</b></p>
 * <p>
 * When the target is local, the logging is piped to the console through the IO streams of the launched process.
 * </p>
 * <p>
 * When the target is remote, a socket connection is established to the log port in the engine.
 * Whenever the connection experiences an error, a new socket connection is established.
 * If the connection can't be established, it is retried for a number of times.
 * </p>
 */
public class TargetService implements ITargetService, Runnable {

    private List<ITargetListener> listeners = new ArrayList<ITargetListener>();
    private Thread thread;
    private volatile boolean stopped = false;
    private ISSDP ssdp;
    private IURLFetcher urlFetcher;
    private int searchInterval = 60;
    private long lastSearch = 0;
    private ITarget[] targets;
    protected LogClient logClient;

    private static Logger logger = LoggerFactory.getLogger(TargetService.class);

    @SuppressWarnings("serial")
    private static class IncompleteXMLException extends Exception {
        public IncompleteXMLException(String msg) {
            super(msg);
        }
    }

    private static ITarget createLocalTarget() {
        InetAddress localAddress = null;
        String name = "Local (no ip)";
        try {
            Iterator<InetAddress> it = NetworkUtil.getValidHostAddresses().iterator();
            while (it.hasNext()) {
                InetAddress address = it.next();
                if (!address.isLoopbackAddress()) {
                    localAddress = address;
                    break;
                }
            }
            String hostAddress = "127.0.0.1";
            if (localAddress != null) {
                hostAddress = localAddress.getHostAddress();
            }
            name = String.format("Local (%s)", hostAddress);
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

        this.logClient = new LogClient(consoleFactory.getConsole("console"));
        this.logClient.start();

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
        logClient.stopLog();
        try {
            thread.join();
            logClient.join();
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
                    // we deduct the processing time for other tasks in this thread to get a fair base for device time expiration
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
                } catch (IOException e) {
                    // Do not log IOException. Network is down, etc
                }
            } catch (Throwable t) {
                // Any throwable would kill the TargetService, so log them here
                logger.error("Unexpected exception in TargetService", t);
            }
        }
    }

    @Override
    public void search() {
        // Force search now
        lastSearch = System.currentTimeMillis() - searchInterval * 1000;
    }

    private Element getRequiredElement(Element element, String name,
            Namespace ns) throws IncompleteXMLException {
        Element e = element.getChild(name, ns);
        if (e == null) {
            throw new IncompleteXMLException(String.format(
                    "Required element %s not found", name));
        }
        return e;
    }

    private String getRequiredField(Element element, String name,
            Namespace ns) throws IncompleteXMLException {
        Element e = getRequiredElement(element, name, ns);
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

        ITarget localTarget = null;
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
                Element device = getRequiredElement(doc.getRootElement(), "device", upnpNS);
                String manufacturer = getRequiredField(device, "manufacturer", upnpNS);
                String udn = getRequiredField(device, "UDN", upnpNS);
                String friendlyName = getRequiredField(device, "friendlyName", upnpNS);

                if (manufacturer.equalsIgnoreCase("defold")) {
                    String url = getRequiredField(device, "url", defoldNS);
                    String logPort = getRequiredField(device, "logPort", defoldNS);
                    // TODO: Local should be iPhone or Joe's iPhone or similar
                    // when iPhone supported is completed
                    String name = String.format("%s (%s)", friendlyName, deviceInfo.address);
                    InetAddress targetAddress = InetAddress.getByName(deviceInfo.address);
                    InetAddress hostAddress = NetworkUtil.getClosestAddress(NetworkUtil.getValidHostAddresses(),
                            targetAddress);
                    ITarget target = new Target(name, udn, targetAddress, url,
                            Integer.parseInt(logPort));

                    // The local target is separately added below to ensure its position in the list
                    if (deviceInfo.address.equals(hostAddress.getHostAddress())) {
                        localTarget = target;
                    } else {
                        targets.add(target);
                    }

                }
            } catch (JDOMParseException e) {
                // Do not log here. We've seen invalid xml responses in real networks
            } catch (IllegalNameException e) {
                // Do not log here. We've seen invalid xml responses in real networks
                // Example message from such an exception: "The name " urn:microsoft-com:wmc-1-0" is not legal for JDOM/XML Namespace URIs: Namespace URIs cannot begin with white-space."
            } catch (IncompleteXMLException e) {
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

        // Add pseudo-target to head of list.
        // It's a convention to be able to fallback to the first target
        targets.add(0, createLocalTarget());
        // If there is a local target present, this should override the pseudo target as fallback.
        // This is to ensure that the running engine is restarted, instead of a new application being launched.
        if (localTarget != null) {
            targets.add(0, localTarget);
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
                                    String socksProxy,
                                    int socksProxyPort,
                                    int httpServerPort) {

        ITarget targetToLaunch = getSelectedTarget();
        LaunchThread launchThread = new LaunchThread(targetToLaunch, customApplication, location, runInDebugger,
                autoRunDebugger, socksProxy, socksProxyPort, getHttpServerURL(targetToLaunch.getInetAddress(),
                        httpServerPort));
        launchThread.start();
        logClient.resetLogSocket(targetToLaunch);
    }

}

