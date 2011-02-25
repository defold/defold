package com.dynamo.cr.contenteditor;

import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.osgi.framework.BundleContext;


public class Activator extends AbstractUIPlugin {
    public static final String PLUGIN_ID = "com.dynamo.cr.contenteditor";

    public static final String PROTOTYPE_IMAGE_ID = "PROTOTYPE";
    public static final String INSTANCE_IMAGE_ID = "INSTANCE";
    public static final String BROKEN_INSTANCE_IMAGE_ID = "BROKEN_INSTANCE";
    public static final String COLLECTION_IMAGE_ID = "COLLECTION";
    public static final String BROKEN_COLLECTION_IMAGE_ID = "BROKEN_COLLECTION";
    public static final String GENERIC_COMPONENT_IMAGE_ID = "CONVEXSHAPE";
    public static final String CONVEXSHAPE_IMAGE_ID = "CONVEXSHAPE";
    public static final String MODEL_IMAGE_ID = "MODEL";
    public static final String MESH_IMAGE_ID = "MESH";
    public static final String MOVE_IMAGE_ID = "MOVE";
    public static final String ROTATE_IMAGE_ID = "ROTATE";
    public static final String BROKEN_IMAGE_ID = "BROKEN";

    private static Activator plugin;

    /**
     * Returns the shared instance
     *
     * @return the shared instance
     */
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
    protected void initializeImageRegistry(ImageRegistry registry) {
        super.initializeImageRegistry(registry);

        registry.put(PROTOTYPE_IMAGE_ID, getImageDescriptor("icons/brick_link.png"));
        registry.put(INSTANCE_IMAGE_ID, getImageDescriptor("icons/brick.png"));
        registry.put(BROKEN_INSTANCE_IMAGE_ID, getImageDescriptor("icons/broken_brick.png"));
        registry.put(COLLECTION_IMAGE_ID, getImageDescriptor("icons/bricks.png"));
        registry.put(BROKEN_COLLECTION_IMAGE_ID, getImageDescriptor("icons/broken_bricks.png"));
        registry.put(GENERIC_COMPONENT_IMAGE_ID, getImageDescriptor("icons/16-cube-blue_16x16.png"));
        registry.put(CONVEXSHAPE_IMAGE_ID, getImageDescriptor("icons/16-cube-red_16x16.png"));
        registry.put(MODEL_IMAGE_ID, getImageDescriptor("icons/16-cube-green_16x16.png"));
        registry.put(MESH_IMAGE_ID, getImageDescriptor("icons/page_gear.png"));
        registry.put(MOVE_IMAGE_ID, getImageDescriptor("icons/move.png"));
        registry.put(ROTATE_IMAGE_ID, getImageDescriptor("icons/rotate_cw.png"));
        registry.put(BROKEN_IMAGE_ID, getImageDescriptor("icons/link_break.png"));
    }

    public static ImageDescriptor getImageDescriptor(String path) {
        return imageDescriptorFromPlugin(PLUGIN_ID, path);
    }

}
