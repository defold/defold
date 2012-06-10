package com.dynamo.cr.targets.core;

import javax.inject.Singleton;

import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.osgi.framework.BundleContext;

import com.dynamo.upnp.ISSDP;
import com.dynamo.upnp.SSDP;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

public class TargetsPlugin extends AbstractUIPlugin implements ITargetsListener {

    private static TargetsPlugin plugin;
    private ITargetsService targetsService;

    public static TargetsPlugin getDefault() {
        return plugin;
    }

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(ITargetsService.class).to(TargetsService.class).in(
                    Singleton.class);
            bind(ISSDP.class).to(SSDP.class).in(Singleton.class);
            bind(IURLFetcher.class).to(DefaultUrlFetcher.class).in(
                    Singleton.class);
        }
    }

    public void start(BundleContext bundleContext) throws Exception {
        plugin = this;

        Module module = new Module();
        Injector injector = Guice.createInjector(module);
        targetsService = injector.getInstance(ITargetsService.class);
        targetsService.setSearchInternal(30);
        targetsService.addTargetsListener(this);
    }

    public void stop(BundleContext bundleContext) throws Exception {
        plugin = null;
        targetsService.removeTargetsListener(this);
        targetsService = null;
    }

    public ITargetsService getTargetsService() {
        return targetsService;
    }

    @Override
    public void targetsChanged(TargetChangedEvent e) {
    }

    public synchronized ITarget[] getTargets() {
        return targetsService.getTargets();
    }

}
