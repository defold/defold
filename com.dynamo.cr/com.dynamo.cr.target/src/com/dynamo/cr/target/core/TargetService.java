package com.dynamo.cr.target.core;

import java.io.IOException;
import java.io.StringReader;
import java.net.InetAddress;
import java.net.URL;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

import javax.inject.Inject;

import org.eclipse.core.commands.Command;
import org.eclipse.core.commands.State;
import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.commands.ICommandService;
import org.jdom2.Document;
import org.jdom2.Element;
import org.jdom2.Namespace;
import org.jdom2.input.JDOMParseException;
import org.jdom2.input.SAXBuilder;

import com.dynamo.upnp.DeviceInfo;
import com.dynamo.upnp.ISSDP;

public class TargetService implements ITargetService, Runnable {

    private List<ITargetListener> listeners = new ArrayList<ITargetListener>();
    private Thread thread;
    private volatile boolean stopped = false;
    private ISSDP ssdp;
    private IURLFetcher urlFetcher;
    private int searchInterval = 60;
    private long lastSearch = 0;
    private ITarget[] targets;

    private static ITarget createLocalTarget() {
        String localAddress = "127.0.0.1";
        try {
            localAddress = InetAddress.getLocalHost().getHostAddress();
        } catch (UnknownHostException e) {
            // TODO: Logging
            e.printStackTrace();
        }

        String name = String.format("Local (%s)", localAddress);
        return new Target(name, LOCAL_TARGET_ID, null);
    }

    @Inject
    public TargetService(ISSDP ssdp, IURLFetcher urlFetcher) {
        targets = new ITarget[] { createLocalTarget(), };

        lastSearch = System.currentTimeMillis();
        this.ssdp = ssdp;
        this.urlFetcher = urlFetcher;
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
                if (changed) {
                    updateTargets();
                    postEvent(new TargetChangedEvent(this));
                }
            } catch (IOException e) {
                // TODO: Logging
                e.printStackTrace();
            }
        }
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
        String localAddress = "127.0.0.1";
        try {
            localAddress = InetAddress.getLocalHost().getHostAddress();
        } catch (UnknownHostException e) {
            // TODO: Logging
            e.printStackTrace();
        }

        boolean localTargetFound = false;
        for (DeviceInfo deviceInfo : devices) {
            String location = deviceInfo.headers.get("LOCATION");
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
                    // TODO: Local should be iPhone or Joe's iPhone or similar
                    // when iPhone supported is completed
                    String name = String.format("%s (%s)", friendlyName, deviceInfo.address);
                    ITarget target = new Target(name, udn, url);
                    targets.add(target);

                    if (deviceInfo.address.equals(localAddress)) {
                        localTargetFound = true;
                    }

                }
            } catch (JDOMParseException e) {
                // TODO: Logging
                e.printStackTrace();
            } catch (Throwable e) {
                // TODO: Logging
                e.printStackTrace();
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
                // TODO: Logging
                e.printStackTrace();
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

    @Override
    public synchronized void launch(String customApplication, String location,
                                    boolean runInDebugger,
                                    boolean autoRunDebugger,
                                    String socksProxy, int socksProxyPort,
                                    URL serverUrl) {

        ITarget targetToLaunch = getSelectedTarget();
        LaunchThread launchThread = new LaunchThread(targetToLaunch, customApplication, location, runInDebugger, autoRunDebugger, socksProxy, socksProxyPort, serverUrl);
        launchThread.start();
    }
}

