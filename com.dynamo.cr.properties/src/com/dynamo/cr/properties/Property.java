package com.dynamo.cr.properties;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@Retention(RetentionPolicy.RUNTIME)
public @interface Property {
    boolean isResource() default false;
    /**
     * Only used when isResource returns true.
     * @return an array of acceptable file extensions
     */
    String[] extensions() default {};
    String displayName() default "";
}
