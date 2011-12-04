package com.dynamo.cr.tileeditor.core;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.tileeditor.core.messages"; //$NON-NLS-1$
    public static String TS_DIFF_IMG_DIMS;
    public static String TS_TILE_WIDTH_GT_IMG;
    public static String TS_TILE_HEIGHT_GT_IMG;

    public static String GRID_INVALID_TILESET;
    public static String GRID_DUPLICATED_LAYER_IDS;

    public static String TileSetModel_image_EMPTY;
    public static String TileSetModel_image_NOT_FOUND;
    public static String TileSetModel_collision_EMPTY;
    public static String TileSetModel_collision_NOT_FOUND;
    public static String TileSetModel_tileWidth_OUTSIDE_RANGE;
    public static String TileSetModel_tileHeight_OUTSIDE_RANGE;
    public static String TileSetModel_tileMargin_OUTSIDE_RANGE;
    public static String TileSetModel_tileSpacing_OUTSIDE_RANGE;
    public static String TileSetModel_materialTag_EMPTY;

    public static String GridModel_tileSet_NOT_FOUND;
    public static String GridModel_tileSet_EMPTY;

    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
