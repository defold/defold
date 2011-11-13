package com.dynamo.cr.sceneed.core;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.sceneed.core.messages"; //$NON-NLS-1$

    public static String NodeModel_ResourceValidator_tileSet_NOT_FOUND;
    public static String NodeModel_ResourceValidator_tileSet_NOT_SPECIFIED;

    public static String NodeModel_ResourceValidator_component_NOT_FOUND;
    public static String NodeModel_ResourceValidator_component_NOT_SPECIFIED;

    public static String ComponentNode_id_NOT_SPECIFIED;
    public static String ComponentNode_id_DUPLICATED;

    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
