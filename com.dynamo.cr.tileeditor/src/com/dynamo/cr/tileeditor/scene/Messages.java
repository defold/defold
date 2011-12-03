package com.dynamo.cr.tileeditor.scene;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.tileeditor.scene.messages"; //$NON-NLS-1$

    // Property validation

    // Tile Set

    public static String SceneModel_ResourceValidator_image_NOT_SPECIFIED;
    public static String SceneModel_ResourceValidator_image_NOT_FOUND;
    public static String SceneModel_ResourceValidator_collision_NOT_SPECIFIED;
    public static String SceneModel_ResourceValidator_collision_NOT_FOUND;
    public static String SceneModel_RangeValidator_tileWidth;
    public static String SceneModel_RangeValidator_tileHeight;
    public static String SceneModel_RangeValidator_tileMargin;
    public static String SceneModel_RangeValidator_tileSpacing;
    public static String SceneModel_NotEmptyValidator_materialTag;
    public static String TileSet_DIFF_IMG_DIMS;
    public static String TileSet_TILE_WIDTH_GT_IMG;
    public static String TileSet_TILE_HEIGHT_GT_IMG;

    // Sprite

    public static String SceneModel_ResourceValidator_tileSet_NOT_FOUND;
    public static String SceneModel_ResourceValidator_tileSet_NOT_SPECIFIED;
    public static String SpriteNode_tileSet_INVALID_REFERENCE;
    public static String SpriteNode_defaultAnimation_NOT_SPECIFIED;

    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
