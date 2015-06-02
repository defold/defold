package com.dynamo.cr.server;

import java.io.File;
import java.net.URL;
import java.util.logging.Handler;
import java.util.logging.LogManager;

import javax.inject.Singleton;
import javax.persistence.EntityManagerFactory;

import org.slf4j.Logger;
import org.slf4j.LoggerFactory;
import org.slf4j.bridge.SLF4JBridgeHandler;

import ch.qos.logback.classic.LoggerContext;
import ch.qos.logback.classic.joran.JoranConfigurator;
import ch.qos.logback.core.joran.spi.JoranException;
import ch.qos.logback.core.util.StatusPrinter;

import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.server.mail.IMailProcessor;
import com.dynamo.cr.server.mail.IMailer;
import com.dynamo.cr.server.mail.MailProcessor;
import com.dynamo.cr.server.mail.SmtpMailer;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.name.Names;

public class Main {

    private Server server;
    private static Main instance;
    protected static Logger logger = LoggerFactory.getLogger(Main.class);

    class Module extends AbstractModule {

        private String configurationFile;

        public Module(String configurationFile) {
            this.configurationFile = configurationFile;
        }

        @Override
        protected void configure() {
            bind(String.class).annotatedWith(Names.named("configurationFile")).toInstance(configurationFile);
            bind(Configuration.class).toProvider(ConfigurationProvider.class).in(Singleton.class);
            bind(IMailProcessor.class).to(MailProcessor.class).in(Singleton.class);
            bind(IMailer.class).to(SmtpMailer.class).in(Singleton.class);
            bind(EntityManagerFactory.class).toProvider(EntityManagerFactoryProvider.class).in(Singleton.class);
            bind(Server.class).in(Singleton.class);
        }
    }

    public static Main getDefault() {
        return instance;
    }

    private void configureLogging() {
        LoggerContext lc = (LoggerContext) LoggerFactory.getILoggerFactory();

        // Configure logback logging
        // The logback jars are not located in the current bundle. Hence logback.xml
        // is not found. Configure logback explicitly
        try {
          JoranConfigurator configurator = new JoranConfigurator();
          configurator.setContext(lc);
          // the context was probably already configured by default configuration rules
          lc.reset();
          File logback = new File("logback.xml");
          if (logback.exists()) {
              // Use logback.xml in current dir
              configurator.doConfigure(logback);
          } else {
              // Fallback to default bundled logback.xml
              URL url = this.getClass().getClassLoader().getResource("logback.xml");
              configurator.doConfigure(url);
          }
        } catch (JoranException je) {
           je.printStackTrace();
        }
        StatusPrinter.printInCaseOfErrorsOrWarnings(lc);


        // Remove default java.util.logging-handlers
        java.util.logging.Logger rootLogger = LogManager.getLogManager().getLogger("");
        Handler[] handlers = rootLogger.getHandlers();
        for (int i = 0; i < handlers.length; i++) {
            rootLogger.removeHandler(handlers[i]);
        }

        // Install java.util.logging to slf4j logging bridge
        // java.util.logging is used by grizzly
        SLF4JBridgeHandler.install();
    }

    public void start() {
        instance = this;
        configureLogging();

        final String serverConfig = System.getProperty("server.config");
        if (serverConfig == null) {
            logger.warn("Property server.config not set. Server is not started");
            return;
        }

        Module module = new Module(serverConfig);
        final Injector injector = Guice.createInjector(module);

        Thread t = new Thread(new Runnable() {
            @Override
            public void run() {
                server = injector.getInstance(Server.class);
            }
        });
        t.start();
    }

    public void stop() {
        instance = null;
        if (server != null)
            server.stop();
    }

    public static void main(String[] args) {
        Main main = new Main();
        main.start();
    }
}
