package org.clojure.tools.nrepl.internal;

import java.util.concurrent.Callable;

import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.contexts.IContextActivation;
import org.eclipse.ui.contexts.IContextService;
import org.eclipse.ui.services.IDisposable;
import org.osgi.framework.BundleContext;

import clojure.java.api.Clojure;

import com.dynamo.cr.clojure_eclipse.ClojureEclipse;
import com.dynamo.cr.clojure_eclipse.ClojureHelper;
import com.dynamo.cr.clojure_eclipse.ClojureOSGi;

public class ReplWrapper {
  class StopThunk implements IDisposable {

    private Object replServer;
    private final BundleContext bundleContext;
    private IContextActivation activation;

    public StopThunk(BundleContext bundleContext, IContextActivation activation, Object replServer) {
      this.bundleContext = bundleContext;
      this.activation = activation;
      this.replServer = replServer;
    }

    @Override
    public void dispose() {
      if (replServer != null) {
        ClojureOSGi.withBundle(bundleContext.getBundle(), new Callable<Object>() {
          @Override
          public Object call() throws Exception {
            ClojureHelper.invoke("clojure.tools.nrepl.server", "stop-server", replServer);
            return null;
          }
        });

        if (activation != null) {
          activation.getContextService().deactivateContext(activation);
        }
      }
      replServer = null;
    }

    @Override
    public String toString() {
      return "NReplServer<" + replServer + ">";
    }
  }

  public IDisposable start(BundleContext bundleContext) {
    Object nrepl = ClojureOSGi.withBundle(bundleContext.getBundle(), new Callable<Object>() {
      public Object call() {
        ClojureHelper.require("clojure.tools.nrepl.server");
        Object server = ClojureHelper.invoke("clojure.tools.nrepl.server", "start-server");

        ClojureEclipse.getTracer().trace("log/info", "NREPL started: " + Clojure.var("clojure.core", "get").invoke(server, Clojure.read(":port")));
        return server;
      }
    });

    IContextService contextService = (IContextService) PlatformUI.getWorkbench().getService(IContextService.class);
    IContextActivation activation = contextService.activateContext("com.dynamo.cr.clojure-nrepl.server-running");

    return new StopThunk(bundleContext, activation, nrepl);
  }
}