package com.dynamo.cr.ddfeditor.scene;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.ddfeditor.scene.messages"; //$NON-NLS-1$

    // Collision shapes
    public static String CollisionShape_bounds_ERROR;

    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
