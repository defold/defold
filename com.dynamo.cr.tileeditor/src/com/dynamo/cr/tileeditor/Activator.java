package com.dynamo.cr.tileeditor;

import org.eclipse.core.runtime.IStatus;
import org.eclipse.core.runtime.Status;
import org.eclipse.jface.resource.ImageRegistry;
import org.eclipse.ui.plugin.AbstractUIPlugin;
import org.eclipse.ui.statushandlers.StatusManager;
import org.osgi.framework.BundleContext;

import com.dynamo.cr.tileeditor.core.Messages;

/**
 * The activator class controls the plug-in life cycle
 */
public class Activator extends AbstractUIPlugin {

    // The plug-in ID
    public static final String PLUGIN_ID = "com.dynamo.cr.tileeditor"; //$NON-NLS-1$

    // Context ID
    public static final String CONTEXT_ID = "com.dynamo.cr.tileeditor.contexts.TileSetEditor"; //$NON-NLS-1$

    // Image ids
    public static final String COLLISION_GROUP_IMAGE_ID = "COLLISION_GROUP"; //$NON-NLS-1$

    // Status codes
    public static final int STATUS_TS_IMG_NOT_SPECIFIED = 101;
    public static final int STATUS_TS_IMG_NOT_FOUND = 102;
    public static final int STATUS_TS_COL_IMG_NOT_FOUND = 103;
    public static final int STATUS_TS_DIFF_IMG_DIMS = 104;
    public static final int STATUS_TS_INVALID_TILE_WIDTH = 105;
    public static final int STATUS_TS_INVALID_TILE_HEIGHT = 106;
    public static final int STATUS_TS_TILE_WIDTH_GT_IMG = 107;
    public static final int STATUS_TS_TILE_HEIGHT_GT_IMG = 108;
    public static final int STATUS_TS_MAT_NOT_SPECIFIED = 109;
    public static final int STATUS_TS_INVALID_TILE_MGN = 110;
    public static final int STATUS_TS_INVALID_TILE_SPCN = 111;

    public static final int STATUS_GRID_TS_NOT_SPECIFIED = 201;
    public static final int STATUS_GRID_TS_NOT_FOUND = 202;
    public static final int STATUS_GRID_INVALID_TILESET = 203;
    public static final int STATUS_GRID_INVALID_CELL_WIDTH = 204;
    public static final int STATUS_GRID_INVALID_CELL_HEIGHT = 205;
    public static final int STATUS_GRID_DUPLICATED_LAYER_IDS = 206;

    // The shared instance
    private static Activator plugin;

    /**
     * The constructor
     */
    public Activator() {
    }

    /*
     * (non-Javadoc)
     * @see org.eclipse.ui.plugin.AbstractUIPlugin#start(org.osgi.framework.BundleContext)
     */
    @Override
    public void start(BundleContext context) throws Exception {
        super.start(context);
        plugin = this;
    }

    /*
     * (non-Javadoc)
     * @see org.eclipse.ui.plugin.AbstractUIPlugin#stop(org.osgi.framework.BundleContext)
     */
    @Override
    public void stop(BundleContext context) throws Exception {
        plugin = null;
        super.stop(context);
    }

    public static void logException(Throwable e) {
        Status status = new Status(IStatus.ERROR, PLUGIN_ID, e.getMessage(), e);
        StatusManager.getManager().handle(status, StatusManager.LOG);
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
    protected void initializeImageRegistry(ImageRegistry registry) {
        super.initializeImageRegistry(registry);

        registry.put(COLLISION_GROUP_IMAGE_ID, imageDescriptorFromPlugin(PLUGIN_ID, "icons/collision_group.png"));
    }

    public static int getStatusSeverity(int code) {
        switch (code) {
        case STATUS_TS_IMG_NOT_SPECIFIED:
            return IStatus.INFO;
        default:
            return IStatus.ERROR;
        }
    }

    public static String getStatusMessage(int code) {
        switch (code) {
        case STATUS_TS_IMG_NOT_SPECIFIED:
            return Messages.TS_IMG_NOT_SPECIFIED;
        case STATUS_TS_IMG_NOT_FOUND:
            return Messages.TS_IMG_NOT_FOUND;
        case STATUS_TS_COL_IMG_NOT_FOUND:
            return Messages.TS_COL_IMG_NOT_FOUND;
        case STATUS_TS_DIFF_IMG_DIMS:
            return Messages.TS_DIFF_IMG_DIMS;
        case STATUS_TS_INVALID_TILE_WIDTH:
            return Messages.TS_INVALID_TILE_WIDTH;
        case STATUS_TS_INVALID_TILE_HEIGHT:
            return Messages.TS_INVALID_TILE_HEIGHT;
        case STATUS_TS_TILE_WIDTH_GT_IMG:
            return Messages.TS_TILE_WIDTH_GT_IMG;
        case STATUS_TS_TILE_HEIGHT_GT_IMG:
            return Messages.TS_TILE_HEIGHT_GT_IMG;
        case STATUS_TS_MAT_NOT_SPECIFIED:
            return Messages.TS_MAT_NOT_SPECIFIED;
        case STATUS_TS_INVALID_TILE_MGN:
            return Messages.TS_INVALID_TILE_MGN;
        case STATUS_TS_INVALID_TILE_SPCN:
            return Messages.TS_INVALID_TILE_SPCN;
        default:
            return "Unknown system error: " + code;
        }
    }
}
