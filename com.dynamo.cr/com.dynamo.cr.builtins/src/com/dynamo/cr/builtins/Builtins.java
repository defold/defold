package com.dynamo.cr.builtins;

import java.io.IOException;
import java.net.URL;

import org.eclipse.core.runtime.FileLocator;
import org.eclipse.core.runtime.Plugin;
import org.osgi.framework.BundleContext;


public class Builtins extends Plugin {

    private static Builtins plugin;

    public static Builtins getDefault() {
        return plugin;
    }

    @Override
    public void start(BundleContext context) throws Exception {
        super.start(context);
        plugin = this;
    }

    @Override
    public void stop(BundleContext context) throws Exception {
        super.stop(context);
        plugin = null;
    }

    public String getBuiltins() {
        URL bundleUrl = getBundle().getEntry("/builtins");
        URL fileUrl;
        try {
            fileUrl = FileLocator.toFileURL(bundleUrl);
            return fileUrl.getPath();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }
}
