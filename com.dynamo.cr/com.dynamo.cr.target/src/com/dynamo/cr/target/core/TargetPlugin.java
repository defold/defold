package com.dynamo.cr.target.core;

import java.io.File;
import java.io.IOException;
import java.io.InputStream;

import javax.inject.Singleton;

import org.apache.commons.io.FileUtils;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.osgi.framework.BundleContext;

import com.dynamo.cr.common.util.Exec;
import com.dynamo.cr.editor.core.EditorUtil;
import com.dynamo.cr.editor.core.IConsoleFactory;
import com.dynamo.cr.editor.ui.ConsoleFactory;
import com.dynamo.upnp.ISSDP;
import com.dynamo.upnp.SSDP;
import com.google.inject.AbstractModule;
import com.google.inject.Guice;
import com.google.inject.Injector;

public class TargetPlugin extends AbstractUIPlugin implements ITargetListener {

    private static TargetPlugin plugin;
    private ITargetService targetsService;
    private String codeSignAllocatePath;

    public static TargetPlugin getDefault() {
        return plugin;
    }

    class Module extends AbstractModule {
        @Override
        protected void configure() {
            bind(ITargetService.class).to(TargetService.class).in(
                    Singleton.class);
            bind(ISSDP.class).to(SSDP.class).in(Singleton.class);
            bind(IURLFetcher.class).to(DefaultUrlFetcher.class).in(
                    Singleton.class);
            bind(IConsoleFactory.class).to(ConsoleFactory.class).in(Singleton.class);
        }
    }

    public void start(BundleContext bundleContext) throws Exception {
        plugin = this;

        Module module = new Module();
        Injector injector = Guice.createInjector(module);
        targetsService = injector.getInstance(ITargetService.class);
        targetsService.setSearchInternal(30);
        targetsService.addTargetsListener(this);

        if (EditorUtil.isMac()) {
            copyCodeSign();
        }
    }

    private void copyCodeSign() throws IOException {
        InputStream codeSignAllocateStream = getClass().getResourceAsStream("/lib/codesign_allocate");
        File codeSignFile = File.createTempFile("codesign_allocate", "");
        codeSignFile.deleteOnExit();
        FileUtils.copyInputStreamToFile(codeSignAllocateStream, codeSignFile);
        codeSignAllocateStream.close();
        Exec.exec("chmod", "+x", codeSignFile.getAbsolutePath());
        codeSignAllocatePath = codeSignFile.getAbsolutePath();
    }

    public String getCodeSignAllocatePath() {
        return codeSignAllocatePath;
    }

    public void stop(BundleContext bundleContext) throws Exception {
        plugin = null;
        targetsService.removeTargetsListener(this);
        targetsService = null;
    }

    public ITargetService getTargetsService() {
        return targetsService;
    }

    @Override
    public void targetsChanged(TargetChangedEvent e) {
    }

    public synchronized ITarget[] getTargets() {
        return targetsService.getTargets();
    }

}
