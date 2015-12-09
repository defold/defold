package com.dynamo.cr.target.core;

import java.io.File;
import java.io.IOException;
import java.net.URISyntaxException;
import java.net.URL;

import javax.inject.Singleton;

import org.eclipse.core.runtime.FileLocator;
import org.eclipse.core.runtime.URIUtil;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.osgi.framework.BundleContext;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

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
    Logger logger = LoggerFactory.getLogger(TargetPlugin.class);

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
            Exec.exec("chmod", "+x", getUtilPath("/lib/codesign_allocate"));
            Exec.exec("chmod", "+x", getUtilPath("/lib/lipo"));
        }
    }

    public String getCodeSignAllocatePath() {
        return getUtilPath("/lib/codesign_allocate");
    }

    public String getLipoPath() {
        return getUtilPath("/lib/lipo");
    }

    private String getUtilPath(String path) {
        URL bundleUrl = getBundle().getEntry(path);

        try {
            if (null == bundleUrl) {
                throw new IOException(String.format("Utility does not exist: '%s'", path));
            }
            // Workaround badly handled UNC paths:
            //    wiki.eclipse.org/Eclipse/UNC_Paths
            URL fileUrl = FileLocator.toFileURL(bundleUrl);
            File file = URIUtil.toFile(URIUtil.toURI(fileUrl));
            return file.getAbsolutePath();
        }
        catch (IOException ioe) {
            throw new RuntimeException(ioe);
        }
        catch (URISyntaxException urie) {
            throw new RuntimeException(urie);
        }
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
