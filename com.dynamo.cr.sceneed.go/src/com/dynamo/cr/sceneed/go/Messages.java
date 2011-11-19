package com.dynamo.cr.sceneed.go;

import org.eclipse.osgi.util.NLS;

/**
 * This class will be moved once the errors are moved to other packages.
 * @author rasv
 *
 */
public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.sceneed.go.messages"; //$NON-NLS-1$

    // Property validation

    public static String SceneModel_ResourceValidator_tileSet_NOT_FOUND;
    public static String SceneModel_ResourceValidator_tileSet_NOT_SPECIFIED;

    public static String SceneModel_ResourceValidator_component_NOT_FOUND;
    public static String SceneModel_ResourceValidator_component_NOT_SPECIFIED;

    public static String ComponentNode_id_NOT_SPECIFIED;
    public static String ComponentNode_id_DUPLICATED;

    public static String RefComponentNode_component_INVALID_REFERENCE;
    public static String RefComponentNode_component_INVALID_TYPE;
    public static String RefComponentNode_component_UNKNOWN_TYPE;

    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
