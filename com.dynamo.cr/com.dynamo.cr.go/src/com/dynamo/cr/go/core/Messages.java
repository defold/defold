package com.dynamo.cr.go.core;

import org.eclipse.osgi.util.NLS;

public class Messages extends NLS {
    private static final String BUNDLE_NAME = "com.dynamo.cr.go.core.messages"; //$NON-NLS-1$

    public static String GameObjectPresenter_ADD_COMPONENT;
    public static String GameObjectPresenter_ADD_COMPONENT_FROM_FILE;
    public static String GameObjectPresenter_SELECT_COMPONENT_TYPE;

    public static String CollectionPresenter_ADD_GAME_OBJECT;
    public static String CollectionPresenter_ADD_SUB_COLLECTION;

    // Property validation

    public static String ComponentNode_id_EMPTY;
    public static String ComponentNode_id_DUPLICATE;

    public static String RefComponentNode_component_INVALID_TYPE;
    public static String RefComponentNode_component_UNKNOWN_TYPE;

    public static String InstanceNode_id_EMPTY;
    public static String InstanceNode_id_DUPLICATE;

    public static String GameObjectInstanceNode_gameObject_INVALID_TYPE;
    public static String GameObjectInstanceNode_gameObject_UNKNOWN_TYPE;

    public static String CollectionInstanceNode_collection_INVALID_TYPE;
    public static String CollectionInstanceNode_collection_UNKNOWN_TYPE;

    static {
        // initialize resource bundle
        NLS.initializeMessages(BUNDLE_NAME, Messages.class);
    }

    private Messages() {
    }
}
