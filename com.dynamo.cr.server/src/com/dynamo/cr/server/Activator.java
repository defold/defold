package com.dynamo.cr.server;

import java.io.IOException;

import org.osgi.framework.BundleActivator;
import org.osgi.framework.BundleContext;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

public class Activator implements BundleActivator {

    private Server server;
    protected static Logger logger = LoggerFactory.getLogger(Activator.class);

    @Override
    public void start(BundleContext context) throws Exception {
        final String server_config = System.getProperty("server.config");
        if (server_config == null) {
            logger.warn("Property server.config not set. Server is not started");
            return;
        }

        Thread t = new Thread(new Runnable() {

            @Override
            public void run() {
                try {
                    server = new Server(server_config);
                } catch (IOException e) {
                    logger.error(e.getMessage(), e);
                }
            }
        });
        t.start();
    }

    @Override
    public void stop(BundleContext context) throws Exception {
        if (server != null)
            server.stop();
    }
}
