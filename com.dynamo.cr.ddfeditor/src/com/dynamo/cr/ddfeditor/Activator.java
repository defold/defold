package com.dynamo.cr.ddfeditor;

import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.osgi.framework.BundleContext;

public class Activator extends AbstractUIPlugin {

    public static final String PLUGIN_ID = "com.dynamo.cr.ddfeditor";
    private static Activator plugin;

    public Activator() {
    }

    public static Activator getDefault() {
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

    @Override
    protected void initializeImageRegistry(ImageRegistry reg) {
        super.initializeImageRegistry(reg);
        registerImage(reg, "link_add", "icons/link_add.png");
        registerImage(reg, "page_add", "icons/page_add.png");
        registerImage(reg, "delete", "icons/delete.png");
        registerImage(reg, "exclamation", "icons/exclamation.png");
    }

    private void registerImage(ImageRegistry registry, String key,
            String fileName) {

        ImageDescriptor id = imageDescriptorFromPlugin(PLUGIN_ID, fileName);
        registry.put(key, id);
    }

}
