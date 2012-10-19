package com.dynamo.cr.properties;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

/**
 * Property annotation
 *
 */
@Retention(RetentionPolicy.RUNTIME)
public @interface Property {

    /**
     * Editor type
     */
    public static enum EditorType {
        /**
         * Use default editor based on the property field type
         */
        DEFAULT,

        /**
         * Drop-down editor.
         */
        DROP_DOWN,

        /**
         * For resource location editing
         */
        RESOURCE,
    }

    /**
     * Only used when isResource returns true.
     * @return an array of acceptable file extensions
     */
    String[] extensions() default {};

    /**
     * Display-name. Default display-name is property field name.
     * Set display-name to override default behaviour
     * @return display name
     */
    String displayName() default "";

    /**
     * Property category for display purposes. Properties inherits "parent" category.
     * Hence, it's only necessary to set the value when to change category
     * @return category
     */
    String category() default "";

    /**
     * Editor type. Default editor-type is based on the type of the field.
     * Set to DROP_DOWN/RESOURE, etc for specific editor controls
     * @return editor type
     */
    EditorType editorType() default EditorType.DEFAULT;
}
