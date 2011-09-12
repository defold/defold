package com.dynamo.cr.properties;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@Retention(RetentionPolicy.RUNTIME)
public @interface Property {
    boolean isResource() default false;
}
