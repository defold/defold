package com.dynamo.cr.go.core;

import org.eclipse.osgi.util.NLS;

/**
 * This class will be moved once the errors are moved to other packages.
 * @author rasv
 *
 */
public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.go.core.messages"; //$NON-NLS-1$

    // Property validation

    public static String ComponentNode_id_EMPTY;
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
