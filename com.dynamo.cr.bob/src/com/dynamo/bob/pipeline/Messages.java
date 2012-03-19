package com.dynamo.bob.pipeline;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.bob.pipeline.messages"; //$NON-NLS-1$

    public static String BuilderUtil_EMPTY_RESOURCE;
    public static String BuilderUtil_MISSING_RESOURCE;

    public static String GuiBuilder_MISSING_TEXTURE;
    public static String GuiBuilder_MISSING_FONT;

    public static String TileSetBuilder_MISSING_IMAGE_AND_COLLISION;

    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
