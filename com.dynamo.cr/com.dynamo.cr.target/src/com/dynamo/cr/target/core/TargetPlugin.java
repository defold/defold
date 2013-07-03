package com.dynamo.cr.target.core;

import java.io.IOException;
import java.net.URL;

import javax.inject.Singleton;

import org.eclipse.core.runtime.FileLocator;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.osgi.framework.BundleContext;
import org.slf4j.Logger;
import org.slf4j.LoggerFactory;

import com.dynamo.cr.common.util.Exec;
import com.dynamo.cr.editor.core.EditorCorePlugin;
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
        }
    }

    public String getCodeSignAllocatePath() {
        return getUtilPath("/lib/codesign_allocate");
    }

    public String getAndroirJarPath() {
        return getUtilPath("lib/android.jar");
    }

    public String getAaptPath() {
        return getExePath("aapt");
    }

    public String getApkcPath() {
        return getExePath("apkc");
    }

    private String getExePath(String name) {
        String platform = EditorCorePlugin.getPlatform();
        String ext = "";
        if (platform.equals("win32")) {
            ext = ".exe";
        }

        String p = getUtilPath(String.format("/lib/%s/%s%s", platform, name, ext));
        try {
            Exec.exec("chmod", "+x", p);
        } catch (IOException e) {
            logger.error("Failed to chmod " + name, e);
        }
        return p;
    }

    private String getUtilPath(String path) {
        URL bundleUrl = getBundle().getEntry(path);
        URL fileUrl;
        try {
            fileUrl = FileLocator.toFileURL(bundleUrl);
            return fileUrl.getPath();
        } catch (IOException e) {
            throw new RuntimeException(e);
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
