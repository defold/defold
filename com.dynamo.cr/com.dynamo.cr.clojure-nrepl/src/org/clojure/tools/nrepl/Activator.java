package org.clojure.tools.nrepl;

import org.clojure.tools.nrepl.internal.ReplWrapper;
import org.eclipse.ui.services.IDisposable;
import org.osgi.framework.BundleActivator;
import org.osgi.framework.BundleContext;

public class Activator implements BundleActivator {

  private static BundleContext context;
  public static Activator plugin;

  static BundleContext getContext() {
    return context;
  }

  private IDisposable replServer;

  public void start(BundleContext bundleContext) throws Exception {
    Activator.context = bundleContext;
    Activator.plugin = this;
  }

  public void stop(BundleContext bundleContext) throws Exception {
    Activator.context = null;
    Activator.plugin = null;
  }

  public void startRepl() {
    if (replServer == null) {
      replServer = new ReplWrapper().start(context);
    }
  }
  
  public void stopRepl() {
    if (replServer != null) {
      replServer.dispose();
      replServer = null;
    }
  }
}
