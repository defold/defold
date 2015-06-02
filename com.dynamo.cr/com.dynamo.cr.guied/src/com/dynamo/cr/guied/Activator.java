package com.dynamo.cr.guied;

import org.eclipse.jface.resource.ImageDescriptor;
import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.osgi.framework.BundleContext;

/**
 * The activator class controls the plug-in life cycle
 */
public class Activator extends AbstractUIPlugin {

	// The plug-in ID
	public static final String PLUGIN_ID = "com.dynamo.cr.guied"; //$NON-NLS-1$

	// The shared instance
	private static Activator plugin;

    // Image ids
    public static final String BOX_NODE_IMAGE_ID = "BOX_NODE_ICON"; //$NON-NLS-1$
    public static final String BOX_NODE_OVERRIDDEN_IMAGE_ID = "BOX_NODE_OVERRIDDEN_ICON"; //$NON-NLS-1$
    public static final String TEXT_NODE_IMAGE_ID = "TEXT_NODE_ICON"; //$NON-NLS-1$
    public static final String TEXT_NODE_OVERRIDDEN_IMAGE_ID = "TEXT_NODE_OVERRIDDEN_ICON"; //$NON-NLS-1$
    public static final String TEXTURE_IMAGE_ID = "TEXTURE_ICON"; //$NON-NLS-1$
    public static final String TEXTURE_ATLAS_IMAGE_ID = "TEXTURE_ATLAS_ICON"; //$NON-NLS-1$
    public static final String TEXTURE_TILESOURCE_IMAGE_ID = "TEXTURE_TILESOURCE_ICON"; //$NON-NLS-1$
    public static final String FONT_IMAGE_ID = "FONT_ICON"; //$NON-NLS-1$
    public static final String FOLDER_IMAGE_ID = "FOLDER_ICON"; //$NON-NLS-1$
    public static final String LAYER_IMAGE_ID = "LAYER_ICON"; //$NON-NLS-1$
    public static final String LAYOUT_IMAGE_ID = "LAYOUT_ICON"; //$NON-NLS-1$

    public static final String GUIEDITOR_CONTEXT_ID = "com.dynamo.cr.guied.contexts.GuiSceneEditor"; //$NON-NLS-1$

    /**
     * The constructor
     */
	public Activator() {
	}

	/*
	 * (non-Javadoc)
	 * @see org.eclipse.ui.plugin.AbstractUIPlugin#start(org.osgi.framework.BundleContext)
	 */
	public void start(BundleContext context) throws Exception {
		super.start(context);
		plugin = this;
	}

	/*
	 * (non-Javadoc)
	 * @see org.eclipse.ui.plugin.AbstractUIPlugin#stop(org.osgi.framework.BundleContext)
	 */
	public void stop(BundleContext context) throws Exception {
		plugin = null;
		super.stop(context);
	}

	/**
	 * Returns the shared instance
	 *
	 * @return the shared instance
	 */
	public static Activator getDefault() {
		return plugin;
	}

    @Override
	protected void initializeImageRegistry(ImageRegistry reg) {
	    super.initializeImageRegistry(reg);
        registerImage(reg, BOX_NODE_IMAGE_ID, "icons/picture.png");
        registerImage(reg, BOX_NODE_OVERRIDDEN_IMAGE_ID, "icons/picture_overridden.png");
        registerImage(reg, TEXT_NODE_IMAGE_ID, "icons/text_large_cap.png");
        registerImage(reg, TEXT_NODE_OVERRIDDEN_IMAGE_ID, "icons/text_large_cap_overridden.png");
        registerImage(reg, TEXTURE_IMAGE_ID, "icons/picture.png");
        registerImage(reg, TEXTURE_ATLAS_IMAGE_ID, "icons/images.png");
        registerImage(reg, TEXTURE_TILESOURCE_IMAGE_ID, "icons/tile_set.png");
        registerImage(reg, FONT_IMAGE_ID, "icons/font.png");
        registerImage(reg, FOLDER_IMAGE_ID, "icons/folder.png");
        registerImage(reg, LAYER_IMAGE_ID, "icons/layer.png");
        registerImage(reg, LAYOUT_IMAGE_ID, "icons/display_profile.png");
	}

    private void registerImage(ImageRegistry registry, String key, String fileName) {
        ImageDescriptor id = imageDescriptorFromPlugin(PLUGIN_ID, fileName);
        registry.put(key, id);
    }
}
