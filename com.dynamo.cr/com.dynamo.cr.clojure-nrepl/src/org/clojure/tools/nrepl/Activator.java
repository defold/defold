package org.clojure.tools.nrepl;

import org.clojure.tools.nrepl.internal.ReplWrapper;
import org.clojure.tools.nrepl.internal.ReplWrapper.StopThunk;
import org.osgi.framework.BundleActivator;
import org.osgi.framework.BundleContext;

public class Activator implements BundleActivator {
    private static BundleContext context;
    public static Activator plugin;

    static BundleContext getContext() {
        return context;
    }

    private StopThunk replServer;

    @Override
    public void start(BundleContext bundleContext) throws Exception {
        Activator.context = bundleContext;
        Activator.plugin = this;
    }

    @Override
    public void stop(BundleContext bundleContext) throws Exception {
        Activator.context = null;
        Activator.plugin = null;
    }

    public void startRepl() throws Exception {
        if (replServer == null) {
            replServer = new ReplWrapper().start(context);
        }
    }

    public void stopRepl() throws Exception {
        if (replServer != null) {
            replServer.stop();
            replServer = null;
        }
    }
}
