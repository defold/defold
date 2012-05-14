package com.dynamo.cr.templates;

import java.io.IOException;
import java.net.URL;

import org.eclipse.core.runtime.FileLocator;
import org.eclipse.core.runtime.Plugin;
import org.osgi.framework.Bundle;
import org.osgi.framework.BundleContext;

public class Templates extends Plugin {

    private static Templates plugin;

    public static Templates getDefault() {
        return plugin;
    }

	public void start(BundleContext bundleContext) throws Exception {
        super.start(bundleContext);
		plugin = this;
	}

	public void stop(BundleContext bundleContext) throws Exception {
        super.stop(bundleContext);
		plugin = null;
	}

    public String getTemplateRoot() {
        Bundle bundle = getBundle();
        URL bundleUrl = bundle.getEntry("/templates");
        URL fileUrl;
        try {
            fileUrl = FileLocator.toFileURL(bundleUrl);
            return fileUrl.getPath();
        } catch (IOException e) {
            throw new RuntimeException(e);
        }
    }
}
