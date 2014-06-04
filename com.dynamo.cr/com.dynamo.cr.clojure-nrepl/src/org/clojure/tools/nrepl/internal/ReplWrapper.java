package org.clojure.tools.nrepl.internal;

import java.util.concurrent.Callable;

import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.contexts.IContextActivation;
import org.eclipse.ui.contexts.IContextService;
import org.osgi.framework.BundleContext;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import clojure.java.api.Clojure;
import clojure.osgi.ClojureHelper;

public class ReplWrapper {
    private static Logger logger = LoggerFactory.getLogger(ReplWrapper.class);

    public class StopThunk {

        private Object replServer;
        private final BundleContext bundleContext;
        private final IContextActivation activation;

        public StopThunk(BundleContext bundleContext, IContextActivation activation, Object replServer) {
            this.bundleContext = bundleContext;
            this.activation = activation;
            this.replServer = replServer;
        }

        public void stop() throws Exception {
            if (replServer != null) {
                logger.info("Stopping REPL");
                ClojureHelper.inBundle(bundleContext.getBundle(), new Callable<Object>() {
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

    public StopThunk start(BundleContext bundleContext) throws Exception {
        Object nrepl = ClojureHelper.inBundle(bundleContext.getBundle(), new Callable<Object>() {
            @Override
            public Object call() {
                ClojureHelper.require("clojure.tools.nrepl.server");
                Object server = ClojureHelper.invoke("clojure.tools.nrepl.server", "start-server");

                logger.warn("Started REPL at nrepl://127.0.0.1:" + Clojure.var("clojure.core", "get").invoke(server, Clojure.read(":port")));
                return server;
            }
        });

        IContextService contextService = (IContextService) PlatformUI.getWorkbench().getService(IContextService.class);
        IContextActivation activation = contextService.activateContext("com.dynamo.cr.clojure-nrepl.server-running");

        return new StopThunk(bundleContext, activation, nrepl);
    }
}