package com.dynamo.cr.tileeditor.scene;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.tileeditor.scene.messages"; //$NON-NLS-1$

    // Property validation

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
