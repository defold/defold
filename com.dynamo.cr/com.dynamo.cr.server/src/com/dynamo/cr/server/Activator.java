package com.dynamo.cr.server;

import javax.inject.Singleton;
import javax.persistence.EntityManagerFactory;

import org.osgi.framework.BundleActivator;
import org.osgi.framework.BundleContext;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.proto.Config.Configuration;
import com.dynamo.cr.server.billing.ChargifyService;
import com.dynamo.cr.server.billing.IBillingProvider;
import com.dynamo.cr.server.mail.IMailProcessor;
import com.dynamo.cr.server.mail.IMailer;
import com.dynamo.cr.server.mail.MailProcessor;
import com.dynamo.cr.server.mail.SmtpMailer;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;
import com.google.inject.name.Names;

public class Activator implements BundleActivator {

    private Server server;
    protected static Logger logger = LoggerFactory.getLogger(Activator.class);

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
            bind(IBillingProvider.class).to(ChargifyService.class).in(Singleton.class);
            bind(IMailer.class).to(SmtpMailer.class).in(Singleton.class);
            bind(EntityManagerFactory.class).toProvider(EntityManagerFactoryProvider.class).in(Singleton.class);
            bind(Server.class).in(Singleton.class);
        }
    }

    @Override
    public void start(BundleContext context) throws Exception {
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

    @Override
    public void stop(BundleContext context) throws Exception {
        if (server != null)
            server.stop();
    }
}
