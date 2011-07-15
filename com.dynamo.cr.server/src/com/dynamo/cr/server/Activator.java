package com.dynamo.cr.server;

import java.io.IOException;
import java.util.logging.Level;
import java.util.logging.Logger;

import org.osgi.framework.BundleActivator;
import org.osgi.framework.BundleContext;

public class Activator implements BundleActivator {

    private Server server;
    private static Logger logger;

    @Override
    public void start(BundleContext context) throws Exception {
        final String server_config = System.getProperty("server.config");
        if (server_config == null) {
            logger.warning("Property server.config not set. Server is not started");
            return;
        }

        Thread t = new Thread(new Runnable() {

            @Override
            public void run() {
                try {
                    server = new Server(server_config);
                } catch (IOException e) {
                    logger.log(Level.SEVERE, e.getMessage(), e);
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
