package com.dynamo.cr.parted;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.parted.messages"; //$NON-NLS-1$

    // Property validation

    // Emitter

    public static String EmitterNode_tileSource_INVALID_REFERENCE;
    public static String EmitterNode_tileSource_CONTENT_ERROR;
    public static String EmitterNode_animation_EMPTY;
    public static String EmitterNode_animation_INVALID;

    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
