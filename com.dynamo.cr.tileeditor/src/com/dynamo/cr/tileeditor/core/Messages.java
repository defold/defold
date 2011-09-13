package com.dynamo.cr.tileeditor.core;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.tileeditor.core.messages"; //$NON-NLS-1$
    public static String TS_IMG_NOT_SPECIFIED;
    public static String TS_IMG_NOT_FOUND;
    public static String TS_COL_IMG_NOT_FOUND;
    public static String TS_DIFF_IMG_DIMS;
    public static String TS_INVALID_TILE_WIDTH;
    public static String TS_INVALID_TILE_HEIGHT;
    public static String TS_TILE_WIDTH_GT_IMG;
    public static String TS_TILE_HEIGHT_GT_IMG;
    public static String TS_MAT_NOT_SPECIFIED;
    public static String TS_INVALID_TILE_MGN;
    public static String TS_INVALID_TILE_SPCN;
    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
