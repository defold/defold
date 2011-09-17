package com.dynamo.cr.tileeditor.core;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.tileeditor.core.messages"; //$NON-NLS-1$
    public static String TS_DIFF_IMG_DIMS;
    public static String TS_TILE_WIDTH_GT_IMG;
    public static String TS_TILE_HEIGHT_GT_IMG;

    public static String GRID_INVALID_TILESET;
    public static String GRID_DUPLICATED_LAYER_IDS;

    public static String TileSetModel_ResourceValidator_image_NOT_SPECIFIED;
    public static String TileSetModel_ResourceValidator_image_NOT_FOUND;
    public static String TileSetModel_ResourceValidator_collision_NOT_SPECIFIED;
    public static String TileSetModel_ResourceValidator_collision_NOT_FOUND;
    public static String TileSetModel_RangeValidator_tileWidth;
    public static String TileSetModel_RangeValidator_tileHeight;
    public static String TileSetModel_RangeValidator_tileMargin;
    public static String TileSetModel_RangeValidator_tileSpacing;
    public static String TileSetModel_NotEmptyValidator_materialTag;

    public static String GridModel_ResourceValidator_tileSet_NOT_FOUND;
    public static String GridModel_ResourceValidator_tileSet_NOT_SPECIFIED;

    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
