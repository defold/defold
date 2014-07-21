package org.clojure.tools.nrepl.internal;

import java.util.concurrent.Callable;

import org.eclipse.ui.PlatformUI;
import org.eclipse.ui.contexts.IContextActivation;
import org.eclipse.ui.contexts.IContextService;
import org.osgi.framework.Bundle;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import clojure.java.api.Clojure;
import clojure.osgi.ClojureHelper;

import com.dynamo.cr.clojurenrepl.provider.IStop;

public class ReplWrapper {
    private static Logger logger = LoggerFactory.getLogger(ReplWrapper.class);

    public class StopThunk implements IStop {

        private Object replServer;
        private final Bundle bundle;
        private final IContextActivation activation;

        public StopThunk(Bundle bundle, IContextActivation activation, Object replServer) {
            this.bundle = bundle;
            this.activation = activation;
            this.replServer = replServer;
        }

        @Override
        public void stop() throws Exception {
            if (replServer != null) {
                logger.info("Stopping REPL");
                ClojureHelper.inBundle(bundle, new Callable<Object>() {
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

    public StopThunk start(Bundle bundle) throws Exception {
        Object nrepl = ClojureHelper.inBundle(bundle, new Callable<Object>() {
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

        return new StopThunk(bundle, activation, nrepl);
    }
}