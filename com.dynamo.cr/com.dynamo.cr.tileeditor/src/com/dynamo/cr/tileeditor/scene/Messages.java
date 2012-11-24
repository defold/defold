package com.dynamo.cr.tileeditor.scene;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.tileeditor.scene.messages"; //$NON-NLS-1$

    // Property validation

    // Tile Set

    public static String TileSetNode_image_EMPTY;
    public static String TileSetNode_image_NOT_FOUND;
    public static String TileSetNode_collision_EMPTY;
    public static String TileSetNode_collision_NOT_FOUND;
    public static String TileSetNode_tileWidth_OUTSIDE_RANGE;
    public static String TileSetNode_tileHeight_OUTSIDE_RANGE;
    public static String TileSetNode_tileMargin_OUTSIDE_RANGE;
    public static String TileSetNode_tileSpacing_OUTSIDE_RANGE;
    public static String TileSetNode_DIFF_IMG_DIMS;
    public static String TileSetNode_TILE_WIDTH_GT_IMG;
    public static String TileSetNode_TILE_HEIGHT_GT_IMG;

    // Collision Group
    public static String CollisionGroupNode_id_OVERFLOW;

    // Animation
    public static String AnimationNode_startTile_INVALID;
    public static String AnimationNode_endTile_INVALID;

    // Sprite

    public static String TileGridNode_tileSet_EMPTY;

    public static String SpriteNode_tileSet_INVALID_REFERENCE;
    public static String SpriteNode_tileSet_CONTENT_ERROR;
    public static String SpriteNode_defaultAnimation_EMPTY;
    public static String SpriteNode_defaultAnimation_INVALID;

    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
