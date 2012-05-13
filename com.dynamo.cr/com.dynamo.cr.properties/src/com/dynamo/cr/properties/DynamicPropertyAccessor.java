package com.dynamo.cr.properties;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 * Annotation for property accessor. The annotated method
 * must return {@link IPropertyAccessor} given a {@link IPropertyObjectWorld}
 * @author chmu
 *
 */
@Retention(RetentionPolicy.RUNTIME)
@Target(value = ElementType.METHOD)
public @interface DynamicPropertyAccessor {

}
