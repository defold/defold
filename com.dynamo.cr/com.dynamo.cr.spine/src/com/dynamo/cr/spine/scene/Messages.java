package com.dynamo.cr.spine.scene;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.spine.scene.messages"; //$NON-NLS-1$

    // Property validation

    // Spine Model
    public static String SpineModelNode_atlas_INVALID_REFERENCE;
    public static String SpineModelNode_atlas_CONTENT_ERROR;
    public static String SpineModelNode_atlas_MISSING_ANIMS;
    public static String SpineModelNode_defaultAnimation_INVALID;
    public static String SpineModelNode_skin_INVALID;

    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
