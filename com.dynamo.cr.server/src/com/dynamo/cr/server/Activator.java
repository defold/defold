package com.dynamo.cr.server;

import java.io.IOException;
import java.util.logging.FileHandler;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.logging.SimpleFormatter;

import org.osgi.framework.BundleActivator;
import org.osgi.framework.BundleContext;

public class Activator implements BundleActivator {

    private Server server;
    private static Logger logger;

    @Override
    public void start(BundleContext context) throws Exception {
        getLogger();

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

    public static Logger getLogger() {
        if (logger == null) {
            logger = Logger.getLogger(Logger.GLOBAL_LOGGER_NAME);
            FileHandler handler;
            try {
                handler = new FileHandler("log", true);
                handler.setFormatter(new SimpleFormatter());
                logger.addHandler(handler);
            } catch (Throwable e) {
                logger.log(Level.SEVERE, e.getMessage(), e);
            }
        }
        return logger;
    }

}
