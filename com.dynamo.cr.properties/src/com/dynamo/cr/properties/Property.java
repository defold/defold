package com.dynamo.cr.properties;

import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;

@Retention(RetentionPolicy.RUNTIME)
public @interface Property {
    Class<? extends IPropertyAccessor<?, ?>> accessor() default BeanPropertyAccessor.class;
    boolean isResource() default false;
}
