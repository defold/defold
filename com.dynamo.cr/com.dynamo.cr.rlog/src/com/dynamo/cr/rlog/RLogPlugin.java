package com.dynamo.cr.rlog;

import org.eclipse.core.runtime.Platform;
import org.osgi.framework.BundleActivator;
import org.osgi.framework.BundleContext;

public class RLogPlugin implements BundleActivator {

    private static BundleContext context;

    static BundleContext getContext() {
        return context;
    }

    private RLogListener listener;
    private static RLogPlugin plugin;

    public void start(BundleContext bundleContext) throws Exception {
        RLogPlugin.context = bundleContext;
        plugin = this;
    }

    public void stop(BundleContext bundleContext) throws Exception {
        stopLogging();
        RLogPlugin.context = null;
        plugin = null;
    }

    public void startLogging() {
        if (listener == null) {
            stopLogging();
            listener = new RLogListener(new RLogHttpTransport());
            Platform.addLogListener(listener);
            listener.start();
        }
    }

    public void stopLogging() {
        if (listener != null) {
            Platform.removeLogListener(listener);
            listener.stop();
            listener = null;
        }
    }

    public static RLogPlugin getDefault() {
        return plugin;
    }


}
